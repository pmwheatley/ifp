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
 * holds the active/inactive flag, the receiving file descriptor, the FTP
 * control socket and incoming data socket, the client's progress monitor
 * address, a count of bytes transferred so far, and a flag indicating SIGIO
 * reception.
 */
static struct {
  int is_active;
  int fd;
  int control_socket;
  int data_socket;
  int *progress_ptr;
  int *errno_ptr;
  int bytes_transferred;
  volatile int sigio_is_pending;
} ifp_current_ftp;


/*
 * ifp_ftp_read_buffer()
 *
 * Fill a buffer from a file descriptor, as much as possible.
 */
static int
ifp_ftp_read_buffer (int fromfd, char *buffer, int maxlen)
{
  int buflen = 0, n;

  while (buflen < maxlen)
    {
      n = read (fromfd, buffer + buflen, maxlen - buflen);
      if (n < 0 && errno != EAGAIN)
        {
          ifp_error ("ftp: read error, output may be truncated");
          return buflen;
        }
      if (n == 0)
        break;
      buflen += n;
    }

  return buflen;
}


/*
 * ifp_ftp_write_buffer()
 *
 * Write a buffer completely to a file descriptor.
 */
static int
ifp_ftp_write_buffer (int tofd, const char *buffer, int buflen)
{
  int sentlen = 0, n;

  while (sentlen < buflen)
    {
      n = write (tofd, buffer + sentlen, buflen - sentlen);
      if (n < 0)
        {
          ifp_error ("ftp: write error, output may be truncated");
          return sentlen;
        }
      sentlen += n;
    }

  return sentlen;
}


/*
 * ifp_ftp_send_line()
 *
 * Given a line format and an argument, put the two together into a command,
 * and send it as a request string to the server, followed by a newline.
 */
static int
ifp_ftp_send_line (int sock, const char *line_format, const char *argument)
{
  int allocation;
  char *line;

  allocation = strlen (line_format) + strlen (argument) + 1;
  line = ifp_malloc (allocation);
  snprintf (line, allocation, line_format, argument);

  ifp_trace ("ftp: sending FTP command: '%s'", line);
  if (ifp_ftp_write_buffer (sock, line, strlen (line)) != (int) strlen (line)
      || ifp_ftp_write_buffer (sock, "\n", 1) != 1)
    {
      ifp_free (line);
      return FALSE;
    }

  ifp_free (line);
  return TRUE;
}


/*
 * ifp_ftp_receive_line()
 *
 * Receive one line of response from an FTP server.  The return buffer must
 * be freed by the caller when finished.  Returns FALSE if there was an
 * error receiving the line.  On FALSE, *line is not assigned.
 */
static int
ifp_ftp_receive_line (int sock, char **line)
{
  char *buffer, c;
  int allocation, index_;

  /* Skip any leading newlines and carriage returns. */
  do
    {
      if (ifp_ftp_read_buffer (sock, &c, 1) != 1)
        {
          ifp_trace ("ftp: error or unexpected end of FTP control channel");
          return FALSE;
        }
    }
  while (c == '\n' || c == '\r');

  /* Start with an unallocated and empty buffer string. */
  buffer = NULL;
  allocation = index_ = 0;

  /* Read up to the end of the line. */
  while (!(c == '\n' || c == '\r'))
    {
      if (index_ == allocation)
        {
          allocation = (allocation == 0) ? 1 : allocation << 1;
          buffer = ifp_realloc (buffer, allocation);
        }
      buffer[index_++] = c;

      if (ifp_ftp_read_buffer (sock, &c, 1) != 1)
        {
          ifp_trace ("ftp: error or unexpected end of FTP control channel");
          ifp_free (buffer);
          return FALSE;
        }
    }

  if (index_ == allocation)
    {
      allocation = (allocation == 0) ? 1 : allocation << 1;
      buffer = ifp_realloc (buffer, allocation);
    }
  buffer[index_++] = '\0';

  ifp_trace ("ftp: received FTP response: '%s'", buffer);
  *line = buffer;
  return TRUE;
}


/*
 * ifp_ftp_receive_last_line()
 *
 * Receive only the last line of a command response from an FTP server.
 * Intermediate (continuation) lines (those with a '-' after the status
 * code) are ignored.  The return buffer should be freed by the caller when
 * finished.  Returns FALSE if there was an error receiving the line.  On
 * FALSE, *line is not assigned.
 */
static int
ifp_ftp_receive_last_line (int sock, char **line)
{
  int ftp_code, check_code;
  char *response;

  if (!ifp_ftp_receive_line (sock, &response))
    {
      ifp_trace ("ftp: no initial response line");
      return FALSE;
    }

  /*
   * A continuation line is one where the first character is non-space, and
   * the fourth character (after status) is '-'.  If we got such a line, it's
   * the start of a contination block; note the block and read forwards to the
   * block end.
   */
  if (response[0] != ' ' && response[3] == '-'
      && sscanf (response, "%3d-", &ftp_code) == 1)
    {
      while (TRUE)
        {
          ifp_free (response);

          if (!ifp_ftp_receive_line (sock, &response))
            {
              ifp_trace ("ftp: no continuation response line");
              return FALSE;
            }

          /* Look for block end. */
          if (response[0] != ' ' && response[3] == ' '
              && sscanf (response, "%3d ", &check_code) == 1
              && check_code == ftp_code)
            break;
        }
    }

  *line = response;
  return TRUE;
}


/*
 * ifp_ftp_sigio_handler()
 *
 * Signal handler for asynchronous download signal SIGIO.  Called from the
 * main SIGIO handler for URLs.  Sets a flag, handled from the poll handler.
 */
void
ifp_ftp_sigio_handler (void)
{
  ifp_trace ("ftp: received SIGIO");

  if (!ifp_current_ftp.is_active)
    {
      ifp_trace ("ftp: state is inactive, ignored");
      return;
    }

  ifp_current_ftp.sigio_is_pending = TRUE;
}


/*
 * ifp_ftp_poll_handler()
 *
 * Called from the main poll handler for URLs.  If SIGIO was received, reads
 * as much data as is available and stores in the file used to collect
 * download data.
 */
void
ifp_ftp_poll_handler (void)
{
  char buffer[4096];
  int buflen, saved_errno;

  ifp_trace ("ftp: ifp_ftp_poll_handler <- void");

  if (!ifp_current_ftp.is_active)
    {
      ifp_trace ("ftp: state is inactive, ignored");
      ifp_current_ftp.sigio_is_pending = FALSE;
      return;
    }

  if (!ifp_current_ftp.sigio_is_pending)
    {
      ifp_trace ("ftp: no pending SIGIO, ignored");
      return;
    }
  ifp_current_ftp.sigio_is_pending = FALSE;

  /*
   * Transfer as much data as is available from the socket to the current
   * open output file.  Save errno from this loop for later.
   */
  buflen = read (ifp_current_ftp.data_socket, buffer, sizeof (buffer));
  while (buflen > 0)
    {
      if (write (ifp_current_ftp.fd, buffer, buflen) != buflen)
        {
          ifp_error ("ftp: write failed, download may be incomplete");
          break;
        }

      ifp_current_ftp.bytes_transferred += buflen;
      buflen = read (ifp_current_ftp.data_socket, buffer, sizeof (buffer));
    }
  saved_errno = errno;

  *ifp_current_ftp.progress_ptr = ifp_current_ftp.bytes_transferred;
  ifp_trace ("ftp: transfer count is now %d bytes",
             ifp_current_ftp.bytes_transferred);

  /*
   * If buflen is 0, then the download just completed successfully.  If
   * buflen is -1 and errno is not EAGAIN, then the download has failed in
   * some way.  Update the errno saved for the transfer as appropriate, and
   * close all file descriptors.  Then return to idle state.
   *
   * If buflen is > 0, the transfer is not yet complete.
   */
  if (buflen == 0 || (buflen == -1 && saved_errno != EAGAIN))
    {
      if (buflen == 0)
        {
          ifp_trace ("ftp: transfer is complete");
          *ifp_current_ftp.errno_ptr = 0;
        }
      else
        {
          ifp_error ("ftp: error %d reading download data", saved_errno);
          *ifp_current_ftp.errno_ptr = saved_errno;
        }

      ifp_ftp_send_line (ifp_current_ftp.control_socket, "QUIT", "");
      close (ifp_current_ftp.control_socket);
      close (ifp_current_ftp.data_socket);
      close (ifp_current_ftp.fd);

      ifp_current_ftp.is_active = FALSE;
      return;
    }

  ifp_trace ("ftp: transfer is not yet complete");
}


/*
 * ifp_ftp_cancel_download()
 *
 * Cancel any current asynchronous FTP download.
 */
void
ifp_ftp_cancel_download (void)
{
  if (!ifp_current_ftp.is_active)
    {
      ifp_trace ("ftp: unnecessary cancel, ignored");
      return;
    }

  ifp_trace ("ftp: transfer is being canceled");
  ifp_ftp_send_line (ifp_current_ftp.control_socket, "ABOR", "");
  ifp_ftp_send_line (ifp_current_ftp.control_socket, "QUIT", "");
  close (ifp_current_ftp.control_socket);
  close (ifp_current_ftp.data_socket);
  close (ifp_current_ftp.fd);

  ifp_current_ftp.is_active = FALSE;
  *ifp_current_ftp.errno_ptr = EINTR;
}


/*
 * ifp_ftp_download()
 *
 * Download FTP data asynchronously from a host, port, and document into a
 * given file descriptor.  Report bytes received in *progress; set to -1 on
 * completion of the download.  Report completion status (errno) in *status.
 */
int
ifp_ftp_download (int tofd, const char *host,
                  int port, const char *document, int *progress, int *status)
{
  struct hostent *hostent;
  int control_socket, one = 1;
  struct sockaddr_in sin_, dsin;
  char *response;
  unsigned int d0, d1, d2, d3, p0, p1, data_host, ndata_host;
  int data_port, data_socket, ftp_code;
  assert (host && document && port > 0 && progress && status);

  ifp_trace ("ftp: ifp_ftp_download <-"
             " %d '%s' %d '%s' intaddr_%p intaddr_%p",
             tofd, host, port, document,
             ifp_trace_pointer (progress), ifp_trace_pointer (status));

  if (ifp_current_ftp.is_active)
    {
      ifp_error ("ftp: download already busy");
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
      ifp_trace ("ftp: error looking up host '%s'", host);
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
          ifp_error ("ftp: unknown name lookup status");
          errno = EINVAL;
          break;
        }
      return FALSE;
    }
  assert (hostent->h_addrtype == AF_INET);
  ifp_trace ("ftp: host is '%s', 0x%X", host, *(int *) hostent->h_addr_list[0]);

  /* Open the socket, and set it how we want it. */
  control_socket = socket (PF_INET, SOCK_STREAM, 0);
  if (control_socket == -1)
    {
      ifp_error ("ftp: unable to create a socket");
      return FALSE;
    }
  if (setsockopt (control_socket,
                  SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one)) == -1)
    {
      ifp_error ("ftp: error setting socket options");
      close (control_socket);
      return FALSE;
    }

  /* Set up sockaddr to connect to host/port, and connect to the socket. */
  sin_.sin_family = AF_INET;
  sin_.sin_port = htons (port);
  memcpy (&sin_.sin_addr, hostent->h_addr_list[0], sizeof (sin_.sin_addr));

  if (connect (control_socket, (struct sockaddr *) &sin_, sizeof (sin_)) == -1)
    {
      ifp_trace ("ftp: error connecting to '%s' port %d", host, port);
      close (control_socket);
      return FALSE;
    }

  /* Log in to the FTP server, and set binary transfers. */
  response = NULL;
  if (!ifp_ftp_receive_last_line (control_socket, &response)
      || strncmp (response, "220 ", 4) != 0)
    {
      ifp_trace ("ftp: no FTP login invitation apparent");
      ifp_free (response);
      close (control_socket);
      errno = EPROTO;
      return FALSE;
    }
  ifp_free (response);

  response = NULL;
  if (!ifp_ftp_send_line (control_socket, "USER %s", "anonymous")
      || !ifp_ftp_receive_last_line (control_socket, &response)
      || strncmp (response, "331 ", 4) != 0)
    {
      ifp_trace ("ftp: failed with USER command");
      ifp_free (response);
      close (control_socket);
      errno = EPERM;
      return FALSE;
    }
  ifp_free (response);

  response = NULL;
  if (!ifp_ftp_send_line (control_socket, "PASS %s", "ifp@nowhere.com")
      || !ifp_ftp_receive_last_line (control_socket, &response)
      || strncmp (response, "230 ", 4) != 0)
    {
      ifp_trace ("ftp: failed with PASS command");
      ifp_free (response);
      close (control_socket);
      errno = EPERM;
      return FALSE;
    }
  ifp_free (response);

  response = NULL;
  if (!ifp_ftp_send_line (control_socket, "TYPE %s", "I")
      || !ifp_ftp_receive_last_line (control_socket, &response)
      || strncmp (response, "200 ", 4) != 0)
    {
      ifp_trace ("ftp: failed with TYPE command");
      ifp_free (response);
      close (control_socket);
      errno = EPROTO;
      return FALSE;
    }
  ifp_free (response);

  /* Set passive FTP mode on this connection. */
  response = NULL;
  if (!ifp_ftp_send_line (control_socket, "PASV", "")
      || !ifp_ftp_receive_last_line (control_socket, &response)
      || strncmp (response, "227 ", 4) != 0)
    {
      ifp_trace ("ftp: failed with PASV command");
      ifp_free (response);
      close (control_socket);
      errno = EPROTO;
      return FALSE;
    }

  /*
   * Dissect the response to find the data host and port.  The PASV 227 FTP
   * response format is not well specified.
   */
  if (sscanf (response, "227 %*[^0-9]%u,%u,%u,%u,%u,%u",
              &d0, &d1, &d2, &d3, &p0, &p1) != 6)
    {
      ifp_trace ("ftp: failed to dissect PASV response");
      ifp_free (response);
      close (control_socket);
      errno = EPROTO;
      return FALSE;
    }
  ifp_free (response);
  assert (d0 < 256 && d1 < 256 && d2 < 256 && d3 < 256);
  assert (p0 < 256 && p1 < 256);

  /* Form the data host and port address. */
  data_host = (d0 << 24) | (d1 << 16) | (d2 << 8) | d3;
  data_port = (p0 << 8) | p1;
  ifp_trace ("ftp: data connection is 0x%X, port %d", data_host, data_port);

  /* Open the data socket, and set it how we want it. */
  data_socket = socket (PF_INET, SOCK_STREAM, 0);
  if (data_socket == -1)
    {
      ifp_error ("ftp: unable to create a socket");
      close (control_socket);
      return FALSE;
    }
  if (setsockopt (data_socket,
                  SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one)) == -1)
    {
      ifp_error ("ftp: error setting socket options");
      close (control_socket);
      close (data_socket);
      return FALSE;
    }

  /* Set up sockaddr to connect to host/port, connect to the data socket. */
  dsin.sin_family = AF_INET;
  dsin.sin_port = htons (data_port);
  ndata_host = htonl (data_host);
  memcpy (&dsin.sin_addr, &ndata_host, sizeof (dsin.sin_addr));

  if (connect (data_socket, (struct sockaddr *) &dsin, sizeof (dsin)) == -1)
    {
      ifp_trace ("ftp: error connecting to data port");
      close (control_socket);
      close (data_socket);
      return FALSE;
    }

  /* Send the retrieval command, and obtain the response. */
  response = NULL;
  if (!ifp_ftp_send_line (control_socket, "RETR %s", document)
      || !ifp_ftp_receive_last_line (control_socket, &response))
    {
      ifp_trace ("ftp: failed with RETR command");
      ifp_free (response);
      close (control_socket);
      close (data_socket);
      errno = EPROTO;
      return FALSE;
    }

  if (sscanf (response, "%d ", &ftp_code) != 1)
    {
      ifp_error ("ftp: error finding RETR response code");
      ifp_free (response);
      close (control_socket);
      close (data_socket);
      errno = EPROTO;
      return FALSE;
    }
  ifp_free (response);
  response = NULL;

  /* Check the response indicates data is on the way. */
  switch (ftp_code)
    {
    case 150:
      break;
    case 450:
    case 550:
      /* FTP status for both permission and no such file. */
      ifp_trace ("ftp: RETR received status 550");
      close (control_socket);
      close (data_socket);
      errno = ENOENT;
      return FALSE;
    default:
      ifp_error ("ftp: can't handle FTP status %d", ftp_code);
      close (control_socket);
      close (data_socket);
      errno = EPROTO;
      return FALSE;
    }

  /*
   * All set to stream data back asynchronously.  Set up notes of what we're
   * doing in module variables.
   */
  ifp_current_ftp.fd = tofd;
  ifp_current_ftp.control_socket = control_socket;
  ifp_current_ftp.data_socket = data_socket;
  ifp_current_ftp.progress_ptr = progress;
  ifp_current_ftp.errno_ptr = status;
  ifp_current_ftp.bytes_transferred = 0;
  ifp_current_ftp.is_active = TRUE;
  ifp_trace ("ftp: set up for asynchronous download");

  /* Set up progress and status for what we know so far. */
  *ifp_current_ftp.progress_ptr = 0;
  *ifp_current_ftp.errno_ptr = EAGAIN;

  /*
   * Now set the socket for asynchronous transfers through SIGIO, then start
   * the download ball rolling by sending ourselves a SIGIO.
   */
  if (fcntl (data_socket, F_SETOWN, getpid ()) == -1
      || fcntl (data_socket, F_SETFL, O_NONBLOCK | O_ASYNC) == -1)
    {
      ifp_error ("ftp: problem setting up async transfer");
      return FALSE;
    }

  if (raise (SIGIO) != 0)
    {
      ifp_error ("ftp: problem starting async transfer");
      return FALSE;
    }

  return TRUE;
}
