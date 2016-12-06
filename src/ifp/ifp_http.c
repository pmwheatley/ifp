/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2001-2007  Simon Baldwin (simon_baldwin@yahoo.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>

#include "ifp.h"
#include "ifp_internal.h"

/*
 * Record of current asynchronous work in progress, and pending SIGIO.  This
 * holds the active/inactive flag, the receiving file descriptor, the HTTP
 * incoming data socket, the client's progress monitor address, a count of
 * bytes transferred so far, and a flag indicating SIGIO reception.
 */
static struct {
  int is_active;
  int fd;
  int http_socket;
  int *progress_ptr;
  int *errno_ptr;
  int bytes_transferred;
  volatile int sigio_is_pending;
} ifp_current_http;


/*
 * ifp_http_read_buffer()
 *
 * Fill a buffer from a file descriptor, as much as possible.
 */
static int
ifp_http_read_buffer (int fromfd, char *buffer, int maxlen)
{
  int buflen = 0, n;

  while (buflen < maxlen)
    {
      n = read (fromfd, buffer + buflen, maxlen - buflen);
      if (n < 0 && errno != EAGAIN)
        {
          ifp_error ("http: read error, output may be truncated");
          return buflen;
        }
      if (n == 0)
        break;
      buflen += n;
    }

  return buflen;
}


/*
 * ifp_http_write_buffer()
 *
 * Write a buffer completely to a file descriptor.
 */
static int
ifp_http_write_buffer (int tofd, const char *buffer, int buflen)
{
  int sentlen = 0, n;

  while (sentlen < buflen)
    {
      n = write (tofd, buffer + sentlen, buflen - sentlen);
      if (n < 0)
        {
          ifp_error ("http: write error, output may be truncated");
          return sentlen;
        }
      sentlen += n;
    }

  return sentlen;
}


/*
 * ifp_http_send_request()
 *
 * Send an HTTP request for a document to a socket.  The host passed in it
 * set in the Host field of the request header, and the document in the
 * GET target.  Uses HTTP 1.0 protocol.
 */
static int
ifp_http_send_request (int sock, const char *host, const char *document)
{
  const char *request_format;
  char *request;
  int allocation;

  request_format =
    "GET /%s HTTP/1.0\n"
    "User-Agent: IFP/1.4\n"
    "Host: %s\n"
    "Connection: Close\n\n";

  allocation = strlen (request_format) + strlen (host) + strlen (document) + 1;
  request = ifp_malloc (allocation);
  snprintf (request, allocation, request_format, document, host);

  ifp_trace ("http: sending HTTP request:\n%s", request);
  if (ifp_http_write_buffer (sock, request, strlen (request))
      != (int) strlen (request))
    {
      ifp_free (request);
      return FALSE;
    }

  ifp_free (request);
  return TRUE;
}


/*
 * ifp_http_sigio_handler()
 *
 * Signal handler for asynchronous download signal SIGIO.  Called from the
 * main SIGIO handler for URLs.  Sets a flag, handled from the poll handler.
 */
void
ifp_http_sigio_handler (void)
{
  ifp_trace ("http: ifp_http_sigio_handler <- void");

  if (!ifp_current_http.is_active)
    {
      ifp_trace ("http: state is inactive, ignored");
      return;
    }

  ifp_current_http.sigio_is_pending = TRUE;
}


/*
 * ifp_http_poll_handler()
 *
 * Called from the main poll handler for URLs.  If SIGIO was received, reads
 * as much data as is available and stores in the file used to collect
 * download data.
 */
void
ifp_http_poll_handler (void)
{
  char buffer[4096];
  int buflen, saved_errno;

  ifp_trace ("http: ifp_http_poll_handler <- void");

  if (!ifp_current_http.is_active)
    {
      ifp_trace ("http: state is inactive, ignored");
      ifp_current_http.sigio_is_pending = FALSE;
      return;
    }

  if (!ifp_current_http.sigio_is_pending)
    {
      ifp_trace ("http: no pending SIGIO, ignored");
      return;
    }
  ifp_current_http.sigio_is_pending = FALSE;

  /*
   * Transfer as much data as is available from the socket to the current
   * open output file.  Save errno from this loop for later.
   */
  buflen = read (ifp_current_http.http_socket, buffer, sizeof (buffer));
  while (buflen > 0)
    {
      if (write (ifp_current_http.fd, buffer, buflen) != buflen)
        {
          ifp_error ("http: write failed, download may be incomplete");
          break;
        }

      ifp_current_http.bytes_transferred += buflen;
      buflen = read (ifp_current_http.http_socket, buffer, sizeof (buffer));
    }
  saved_errno = errno;

  *ifp_current_http.progress_ptr = ifp_current_http.bytes_transferred;
  ifp_trace ("http: transfer count is now %d bytes",
             ifp_current_http.bytes_transferred);

  /*
   * If buflen is 0, then the download just completed successfully.  If
   * buflen is -1 and errno is not EAGAIN, then the download has failed in
   * some way.  Update the errno saved for the transfer as appropriate, and
   * close all file descriptors.  Then return to inactive state.
   *
   * If buflen is > 0, the transfer is not yet complete.
   */
  if (buflen == 0 || (buflen == -1 && saved_errno != EAGAIN))
    {
      if (buflen == 0)
        {
          ifp_trace ("http: transfer is complete");
          *ifp_current_http.errno_ptr = 0;
        }
      else
        {
          ifp_error ("http: error %d reading download data", saved_errno);
          *ifp_current_http.errno_ptr = saved_errno;
        }

      close (ifp_current_http.http_socket);
      close (ifp_current_http.fd);

      ifp_current_http.is_active = FALSE;
      return;
    }

  ifp_trace ("http: transfer is not yet complete");
}


/*
 * ifp_http_cancel_download()
 *
 * Cancel any current asynchronous HTTP download.
 */
void
ifp_http_cancel_download (void)
{
  if (!ifp_current_http.is_active)
    {
      ifp_trace ("http: unnecessary cancel, ignored");
      return;
    }

  ifp_trace ("http: transfer is being canceled");
  close (ifp_current_http.http_socket);
  close (ifp_current_http.fd);

  ifp_current_http.is_active = FALSE;
  *ifp_current_http.errno_ptr = EINTR;
}


/*
 * ifp_http_download()
 *
 * Download HTTP data asynchronously from a host, port, and document into a
 * given file descriptor.  Report bytes received in *progress; set to -1 on
 * completion of the download.  Report completion status (errno) in *status.
 */
int
ifp_http_download (int tofd, const char *host,
                   int port, const char *document, int *progress, int *status)
{
  struct hostent *hostent;
  int http_socket, one = 1;
  struct sockaddr_in sin_;
  char http_status[13];  /* Room for "HTTP/1.x nnn". */
  int http_code, count;
  assert (host && document && port > 0 && progress && status);

  ifp_trace ("http: ifp_http_download ->"
             " %d '%s' %d '%s' intaddr_%p intaddr_%p",
             tofd, host, port, document,
             ifp_trace_pointer (progress), ifp_trace_pointer (status));

  if (ifp_current_http.is_active)
    {
      ifp_error ("http: download already busy");
      errno = EBUSY;
      return FALSE;
    }

 /*
  * Look up the host name.  If the lookup fails, try to map hostname lookup
  * errors into something that we can check for in errno.
  */
  hostent = gethostbyname (host);
  if (!hostent)
    {
      ifp_trace ("http: error looking up host '%s'", host);
      switch (h_errno)
        {
        case HOST_NOT_FOUND:
          errno = EHOSTUNREACH;
          break;
        case NO_RECOVERY:
          errno = EHOSTUNREACH;
          break;
        case TRY_AGAIN:
          errno = EAGAIN;
          break;
        case NO_ADDRESS:
          errno = EHOSTUNREACH;
          break;
        default:
          ifp_error ("http: unknown name lookup status");
          errno = EINVAL;
          break;
        }
      return FALSE;
    }
  assert (hostent->h_addrtype == AF_INET);
  ifp_trace ("http:"
             " host is '%s', 0x%X", host, *(int *) hostent->h_addr_list[0]);

  /* Open the socket, and set it how we want it. */
  http_socket = socket (PF_INET, SOCK_STREAM, 0);
  if (http_socket == -1)
    {
      ifp_error ("http: unable to create a socket");
      return FALSE;
    }
  if (setsockopt (http_socket,
                  SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one)) == -1)
    {
      ifp_error ("http: error setting socket options");
      close (http_socket);
      return FALSE;
    }

  /* Set up sockaddr to connect to host/port, and connect to the socket. */
  sin_.sin_family = AF_INET;
  sin_.sin_port = htons (port);
  memcpy (&sin_.sin_addr, hostent->h_addr_list[0], sizeof (sin_.sin_addr));

  if (connect (http_socket, (struct sockaddr *) &sin_, sizeof (sin_)) == -1)
    {
      ifp_trace ("http: error connecting to '%s' port %d", host, port);
      close (http_socket);
      return FALSE;
    }

  /* Send the HTTP request to the socket. */
  if (!ifp_http_send_request (http_socket, host, document))
    {
      ifp_error ("http: error writing HTTP GET request");
      close (http_socket);
      return FALSE;
    }

  /*
   * Read the return data from the socket, up to the end of the HTTP header.
   * We're looking for something like "HTTP/1.1 200"
   */
  if (ifp_http_read_buffer (http_socket, http_status, sizeof (http_status) - 1)
      != sizeof (http_status) - 1)
    {
      ifp_error ("http: error reading HTTP status response");
      close (http_socket);
      return FALSE;
    }
  http_status[sizeof (http_status) - 1] = '\0';
  ifp_trace ("http: initial response is: %s", http_status);
  if (sscanf (http_status, "HTTP/%*d.%*d %d", &http_code) != 1)
    {
      ifp_error ("http: unrecognized HTTP status string");
      close (http_socket);
      errno = EPROTO;
      return FALSE;
    }
  ifp_trace ("http: extracted HTTP status code %d", http_code);

  /*
   * Try to make at least something of the return code.  TODO this code could
   * cope better with things like HTTP redirection.
   */
  switch (http_code)
    {
    case 200:
      break;
    case 404:
      ifp_trace ("http: HTTP error 404: document not found");
      close (http_socket);
      errno = ENOENT;
      return FALSE;
    case 401:
    case 403:
      ifp_trace ("http: HTTP error 401/403: not authorized");
      close (http_socket);
      errno = EPERM;
      return FALSE;
    default:
      ifp_error ("http: can't handle HTTP status %d", http_code);
      close (http_socket);
      errno = EPROTO;
      return FALSE;
    }

  /*
   * Read past the entire rest of the HTTP response header.  This could be
   * much more efficient, but the header isn't long.  The loop reads until it
   * sees two blank lines in a row.
   */
  for (count = 0; count < 2;)
    {
      char c;

      if (ifp_http_read_buffer (http_socket, &c, 1) != 1)
        {
          ifp_error ("http: unexpected end of HTTP response");
          close (http_socket);
          errno = EPIPE;
          return FALSE;
        }
      if (c == '\n')
        count++;
      else if (c != '\r')
        count = 0;
    }

  /*
   * All set to stream data back asynchronously.  Set up notes of what we're
   * doing in module variables.
   */
  ifp_current_http.fd = tofd;
  ifp_current_http.http_socket = http_socket;
  ifp_current_http.progress_ptr = progress;
  ifp_current_http.errno_ptr = status;
  ifp_current_http.bytes_transferred = 0;
  ifp_current_http.is_active = TRUE;
  ifp_trace ("http: set up for asynchronous download");

  /* Set up progress and status for what we know so far. */
  *ifp_current_http.progress_ptr = 0;
  *ifp_current_http.errno_ptr = EAGAIN;

  /*
   * Now set the socket for asynchronous transfers through SIGIO, and start
   * the download ball rolling by sending ourselves a SIGIO.
   */
  if (fcntl (http_socket, F_SETOWN, getpid ()) == -1
      || fcntl (http_socket, F_SETFL, O_NONBLOCK | O_ASYNC) == -1)
    {
      ifp_error ("http: problem setting up async transfer");
      return FALSE;
    }

  if (raise (SIGIO) != 0)
    {
      ifp_error ("http: problem starting async transfer");
      return FALSE;
    }

  return TRUE;
}
