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
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include "ifp.h"
#include "ifp_internal.h"

/* Temporary file template. */
static const char *TMPFILE_TEMPLATE = "/tmp/ifp_url_XXXXXX";

/* URL magic identifier, for safety purposes. */
static const unsigned int URL_MAGIC = 0x28cbc2f8;

/* Timeout on pause call, for a regular nudge for asynchronous downloads. */
static int ifp_pause_timeout = 250000;

/*
 * Internal definition of a URL structure.  Client callers get to see
 * an opaque handle for these, and they pass them back and forth to functions
 * in this module.  This is the only module that should be concerned with
 * the details of what is in a URL.
 *
 * A newly minted URL is unresolved.  At some later point, it is "resolved"
 * by being handed a URL.  If the URL is a local file, the data_file member
 * just points to the local disk file.  If it's a real remote file, then
 * the file is transferred into a temporary file, and data_file points to
 * the temporary file.  On scrubbing it, temporary files are removed, the
 * string space for data_file is freed, and the URL is set back to the
 * unresolved state.
 */
typedef enum ifp_url_type
{
  URL_NONE = 0, URL_FILE, URL_HTTP, URL_FTP
}
ifp_url_type_t;

struct ifp_url
{
  unsigned int magic;

  /* URL type, and current URL state. */
  ifp_url_type_t type;
  enum { URL_UNRESOLVED = 1, URL_RESOLVING, URL_RESOLVED } state;

  /*
   * Progress for remote URLs, status, the path resolved, and file that
   * contains the URL data.
   */
  int progress;
  int status;
  char *url_path;
  char *data_file;
};

/*
 * Definition of a temporary file list, used to track temporary files in
 * existence, so they can be removed if a cleanup on process exit is needed.
 */
struct ifp_tmpfile
{
  const char *tmpfile;

  struct ifp_tmpfile *next;
};

typedef struct ifp_tmpfile *ifp_tmpfileref_t;
static ifp_tmpfileref_t ifp_tmpfile_list = NULL;


/**
 * ifp_url_is_valid()
 *
 * Confirms that the address passed in refers to a URL.  Returns TRUE
 * if the address is to a URL, FALSE otherwise.
 */
int
ifp_url_is_valid (ifp_urlref_t url)
{
  return url && url->magic == URL_MAGIC;
}


/**
 * ifp_url_set_pause_timeout()
 * ifp_url_get_pause_timeout()
 *
 * Set and get the timeout that occurs when ifp_url_pause() is called.
 * The timeout is set in microseconds.  After sitting in ifp_url_pause()
 * for this long without any activity on URL download channels, a call
 * to ifp_url_pause() will return.
 */
void
ifp_url_set_pause_timeout (int timeout)
{
  if (timeout < 0)
    {
      ifp_error ("url: invalid pause timeout, %d", timeout);
      return;
    }

  ifp_trace ("url: pause timeout set to %d usec", timeout);
  ifp_pause_timeout = timeout;
}

int
ifp_url_get_pause_timeout (void)
{
  static int env_pause_timeout, initialized = FALSE;
  static const char *ifp_url_timeout;

  if (!initialized)
    {
      ifp_url_timeout = getenv ("IFP_URL_TIMEOUT");
      if (ifp_url_timeout)
        {
          env_pause_timeout = atoi (ifp_url_timeout);
          ifp_notice ("url: %s initialized URL pause timeout to"
                      " %d microseconds", "IFP_URL_TIMEOUT", env_pause_timeout);
        }
      initialized = TRUE;
    }

  return ifp_url_timeout ? env_pause_timeout : ifp_pause_timeout;
}


/**
 * ifp_url_new()
 *
 * Return a new, and empty, URL.
 */
ifp_urlref_t
ifp_url_new (void)
{
  ifp_urlref_t url;

  ifp_trace ("url: ifp_url_new <- void");

  url = ifp_malloc (sizeof (*url));
  memset (url, 0, sizeof (*url));
  url->magic = URL_MAGIC;
  url->type = URL_NONE;
  url->state = URL_UNRESOLVED;

  ifp_trace ("url: returned url_%p", ifp_trace_pointer (url));
  return url;
}


/**
 * ifp_url_destroy()
 *
 * Empty and destroy a URL.  It is an error to try to destroy a URL that is
 * not unresolved.
 */
void
ifp_url_destroy (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  ifp_trace ("url: ifp_url_destroy <-"
             " url_%p", ifp_trace_pointer (url));

  if (url->state != URL_UNRESOLVED)
    {
      ifp_error ("url: attempt to destroy a loaded URL");
      return;
    }

  memset (url, 0xaa, sizeof (*url));
  ifp_free (url);
  ifp_trace ("url: destroyed url_%p", ifp_trace_pointer (url));
}


/*
 * ifp_url_finalize_cleanup()
 *
 * Go through the list of temporary files we have and delete each one.  This
 * function is called on process shutdown, to clean up temporary file.  URLs
 * resolved and connected to these files will see invalid filenames once
 * the function has run.
 */
static void
ifp_url_finalize_cleanup (void)
{
  ifp_tmpfileref_t entry;

  ifp_trace ("url: ifp_url_finalize_cleanup <- void");

  for (entry = ifp_tmpfile_list; entry; entry = ifp_tmpfile_list)
    {
      ifp_tmpfile_list = entry->next;

      ifp_trace ("url: finalizer unlinking '%s'", entry->tmpfile);
      unlink (entry->tmpfile);
      ifp_free (entry);
    }
}


/*
 * ifp_url_tmpfile_add()
 *
 * Add a new temporary filename to the cleanup list.  If this is the first
 * call, also register our cleanup routine for a call on process exit.
 */
static void
ifp_url_tmpfile_add (const char *tmpfile_)
{
  static int initialized = FALSE;
  ifp_tmpfileref_t entry;

  ifp_trace ("url: ifp_url_tmpfile_add <- '%s'", tmpfile_);

  if (!initialized)
    {
      ifp_register_finalizer (ifp_url_finalize_cleanup);
      initialized = TRUE;
    }

  entry = ifp_malloc (sizeof (*entry));
  entry->tmpfile = tmpfile_;

  entry->next = ifp_tmpfile_list;
  ifp_tmpfile_list = entry;
}


/*
 * ifp_url_tmpfile_delete()
 *
 * Delete a temporary filename from the cleanup list.
 */
static void
ifp_url_tmpfile_delete (const char *tmpfile_)
{
  ifp_tmpfileref_t entry, prior, next;
  assert (tmpfile_);

  ifp_trace ("url: ifp_url_tmpfile_delete <- '%s'", tmpfile_);

  prior = NULL;
  for (entry = ifp_tmpfile_list; entry; entry = next)
    {
      next = entry->next;
      if (entry->tmpfile == tmpfile_)
        {
          ifp_trace ("url: removed tmpfile list entry '%s'", tmpfile_);
          if (!prior)
            {
              assert (entry == ifp_tmpfile_list);
              ifp_tmpfile_list = next;
            }
          else
            prior->next = next;

          ifp_free (entry);
        }
      else
        prior = entry;
    }
}


/**
 * ifp_url_get_url_path()
 * ifp_url_get_data_file()
 *
 * Return the URL path, and the temporary data file containing the URL's
 * downloaded data.
 */
const char *
ifp_url_get_url_path (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  if (url->state != URL_RESOLVING && url->state != URL_RESOLVED)
    {
      ifp_error ("url: attempt to access an unused URL");
      return NULL;
    }

  return url->url_path;
}

const char *
ifp_url_get_data_file (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  if (url->state != URL_RESOLVED)
    {
      ifp_error ("url: attempt to access an unresolved URL");
      return NULL;
    }

  return url->data_file;
}


/**
 * ifp_url_is_remote()
 *
 * Returns TRUE if this URL is represents a remote (that is, non-'file:'),
 * URL.
 */
int
ifp_url_is_remote (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  return url->type == URL_HTTP || url->type == URL_FTP;
}


/**
 * ifp_url_scrub()
 *
 * Decrement the reference count of any temporary file associated with
 * this URL in the URL cache, or if busy resolving, delete the temporary
 * file immediately, then empty the URL ready for destruction.
 */
void
ifp_url_scrub (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  ifp_trace ("url: ifp_url_scrub <-"
             " url_%p", ifp_trace_pointer (url));

  if (url->state == URL_UNRESOLVED)
    return;

  /* If the state indicates the URL is busy resolving, cancel it. */
  if (url->state == URL_RESOLVING)
    {
      if (url->type == URL_HTTP)
        {
          ifp_trace ("url: canceling pending HTTP download");
          ifp_http_cancel_download ();
        }
      else if (url->type == URL_FTP)
        {
          ifp_trace ("url: canceling pending FTP download");
          ifp_ftp_cancel_download ();
        }
    }

  /*
   * If busy resolving, remove any temporary file indicated by the URL state.
   * Alternatively, if resolved, release the reference hold the URL has on
   * the cache entry.
   */
  if (url->state == URL_RESOLVING)
    {
      ifp_url_tmpfile_delete (url->data_file);
      ifp_trace ("url: unlinking pending file '%s'", url->data_file);
      unlink (url->data_file);
    }
  else if (url->state == URL_RESOLVED)
    {
      if (ifp_url_is_remote (url))
        {
          ifp_trace ("url: release cache entry for '%s'", url->url_path);
          ifp_cache_release_entry (url->url_path);
        }
    }

  ifp_free (url->url_path);
  ifp_free (url->data_file);

  memset (url, 0xaa, sizeof (*url));
  url->magic = URL_MAGIC;
  url->type = URL_NONE;
  url->state = URL_UNRESOLVED;
}


/*
 * ifp_url_sigio_handler()
 *
 * Called when SIGIO indicates that I/O is ready on asynchronous channels.
 * Calls all subordinate SIGIO handlers.
 */
static void
ifp_url_sigio_handler (int signum)
{
  ifp_trace ("url: received IO signal %d", signum);

  ifp_http_sigio_handler ();
  ifp_ftp_sigio_handler ();
}


/*
 * ifp_url_install_handler()
 *
 * Install the URL SIGIO hander as the handler for this signal.
 */
static int
ifp_url_install_handler (void)
{
  static int initialized = FALSE;
  struct sigaction sa, old_sa;

  if (!initialized)
    {
      ifp_trace ("url: installing SIGIO handler");

      memset (&sa, 0, sizeof (sa));
      sa.sa_handler = ifp_url_sigio_handler;
      sigemptyset (&sa.sa_mask);
      sa.sa_flags = 0;
      if (sigaction (SIGIO, &sa, &old_sa) == -1)
        {
          ifp_error ("url: failed to install a SIGIO handler");
          return FALSE;
        }

      /*
       * If we trampled on an already set SIGIO handler, there's not a lot
       * we can (or will) do about it.  It's highly unlikely.
       */
      if (old_sa.sa_handler != SIG_DFL && old_sa.sa_handler != SIG_IGN)
        {
          ifp_error ("url: overwrote signal %d handler", SIGIO);
        }

      initialized = TRUE;
    }

  return TRUE;
}


/**
 * ifp_url_poll_resolved_async()
 *
 * Polls the URL to see if any asynchronous download has completed.  Returns
 * TRUE if the URL is ready to be used, FALSE otherwise.
 */
int
ifp_url_poll_resolved_async (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  if (url->state == URL_RESOLVED)
      return TRUE;

  /* Nudge the asynchronous downloads, and call individual poll handlers. */
  if (raise (SIGIO) != 0)
    {
      ifp_error ("url: unable to generate IO signal");
      return FALSE;
    }

  ifp_http_poll_handler ();
  ifp_ftp_poll_handler ();

  /*
   * If it's resolving, see if it has completed.  If it has, then set its
   * state to resolved, add the downloaded file to the URL cache, and remove
   * the temporary file from our list of things to remove on deletion, as
   * control is now passed off to the cache.
   */
  if (url->state == URL_RESOLVING && url->status != EAGAIN)
    {
      url->state = URL_RESOLVED;

      ifp_trace ("url: pass to cache for '%s'", url->url_path);
      ifp_cache_add_entry (url->url_path, url->data_file);

      ifp_url_tmpfile_delete (url->data_file);
      return TRUE;
    }

  return FALSE;
}


/**
 * ifp_url_poll_progress_async()
 *
 * Polls the URL to see how much data has accumulated in asynchronous
 * downloads.  Returns the byte count.
 */
int
ifp_url_poll_progress_async (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  /* Nudge the asynchronous downloads, and call individual poll handlers. */
  if (raise (SIGIO) != 0)
    {
      ifp_error ("url: unable to generate IO signal");
      return url->progress;
    }

  ifp_http_poll_handler ();
  ifp_ftp_poll_handler ();

  return url->progress;
}


/**
 * ifp_url_get_status_async()
 *
 * Return the errno status of an asynchronous download.  Returns EAGAIN if
 * the URL is still resolving.
 */
int
ifp_url_get_status_async (ifp_urlref_t url)
{
  return url->state == URL_RESOLVED ? url->status : EAGAIN;
}


/*
 * ifp_url_resolve_cache()
 *
 * Try to resolve a URL from the remote URL cache.  If successful, return
 * TRUE and set up the URL data.  Otherwise, return FALSE.
 */
static int
ifp_url_resolve_cache (ifp_urlref_t url,
                       const char *urlpath, ifp_url_type_t type)
{
  const char *cache_file;
  char *data_file;
  assert (ifp_url_is_valid (url) && urlpath);

  cache_file = ifp_cache_find_entry (urlpath);
  if (!cache_file)
    {
      ifp_trace ("url: cache miss for '%s'", urlpath);
      return FALSE;
    }

  data_file = ifp_malloc (strlen (cache_file) + 1);
  strcpy (data_file, cache_file);

  ifp_trace ("url: cache hit for '%s', file '%s'", urlpath, data_file);
  url->type = type;
  url->state = URL_RESOLVED;
  url->progress = 0;
  url->status = 0;
  url->data_file = data_file;

  return TRUE;
}


/*
 * ifp_url_resolve_remote()
 *
 * Given the back part of an HTTP or FTP URL, download the URL's contents
 * into a temporary system file.  Set up the URL structure with the temporary
 * file's details.
 */
static int
ifp_url_resolve_remote (ifp_urlref_t url, const char *scheme,
                        const char *hier_part, int default_port,
                        ifp_url_type_t type,
                        int (*resolver) (int, const char *, int,
                                         const char *, int *, int *))
{
  char *host, *document, *tmpfilename;
  int port, tmpfile_, status;

  ifp_trace ("url: ifp_url_resolve_remote <-"
             " url_%p '%s' '%s' %d %d function_%p",
             ifp_trace_pointer (url), scheme,
             hier_part, default_port, type, ifp_trace_pointer (resolver));

  host = ifp_malloc (strlen (hier_part) + 1);
  document = ifp_malloc (strlen (hier_part) + 1);

  /*
   * Find the system name, port, and document parts from the urlpath.
   *
   * Note that with this simple match, we regard attempts to access a server's
   * default page (for example, "http://hostname[:port]/" as a malformed URL.
   * I doubt anybody is ever going to make their server's default page an IF
   * game...
   *
   * Note also that this match does not handle URLs with optional
   * "user:password@..." portions.  It reports these as malformed URLs.
   */
  if (sscanf (hier_part,
              "//%[a-zA-Z0-9.=_-]:%d/%s", host, &port, document) != 3)
    {
      /* Look for a portless urlpath. */
      if (sscanf (hier_part, "//%[a-zA-Z0-9.=_-]/%s", host, document) != 2)
        {
          ifp_trace ("url: malformed URL '%s:%s'", scheme, hier_part);
          ifp_free (host);
          ifp_free (document);
          errno = EINVAL;
          return FALSE;
        }

      /* Port number not given, so take the default. */
      port = default_port;
    }
  if (port <= 0 || port > 65535)
    {
      ifp_trace ("url: bad port number, %d ", port);
      ifp_free (host);
      ifp_free (document);
      errno = EINVAL;
      return FALSE;
    }
  ifp_trace ("url:"
             " URL host is '%s', port %d, document '%s'", host, port, document);

  /* Create a temporary file for the retrieved contents. */
  tmpfilename = ifp_malloc (strlen (TMPFILE_TEMPLATE) + 1);
  strcpy (tmpfilename, TMPFILE_TEMPLATE);
  tmpfile_ = mkstemp (tmpfilename);
  if (tmpfile_ == -1)
    {
      ifp_error ("url: error creating temporary file '%s'", tmpfilename);
      unlink (tmpfilename);
      ifp_free (tmpfilename);
      ifp_free (host);
      ifp_free (document);
      return FALSE;
    }
  ifp_trace ("url: temporary file is '%s'", tmpfilename);

  /* Add this temporary file to the deletions list. */
  ifp_url_tmpfile_add (tmpfilename);

  /*
   * Call the resolver function to begin downloading the document into the
   * temporary file, and fail if initiating the transfer fails.
   */
  status = resolver (tmpfile_,
                     host, port, document, &url->progress, &url->status);
  if (!status)
    {
      ifp_trace ("url: error retrieving //%s:%d//%s", host, port, document);

      ifp_url_tmpfile_delete (tmpfilename);
      unlink (tmpfilename);
      ifp_free (tmpfilename);

      ifp_free (host);
      ifp_free (document);
      return FALSE;
    }

  ifp_free (host);
  ifp_free (document);

  /*
   * Update the URL with details of the temporary file we are now filling
   * with the data.  Set state to actively resolving.
   */
  url->type = type;
  url->state = URL_RESOLVING;
  url->data_file = tmpfilename;

  return TRUE;
}


/*
 * ifp_url_resolve_file_as_ftp()
 *
 * Resolve a "file:" URL as an "ftp:" URL.  Called when the local file
 * resolver function detects a hostname other than "localhost" in the host
 * id part of a "file:" URL.
 */
static int
ifp_url_resolve_file_as_ftp (ifp_urlref_t url, const char *hier_part)
{
  int allocation, status;
  char *urlpath;

  ifp_trace ("url: ifp_url_resolve_file_as_ftp <-"
             " url_%p '%s'", ifp_trace_pointer (url), hier_part);

  /* Construct an ftp: URL from the hierarchical part passed in. */
  allocation = strlen (hier_part) + 1 + strlen ("ftp:");
  urlpath = ifp_malloc (allocation);
  snprintf (urlpath, allocation, "ftp:%s", hier_part);

  /* If not in the cache, begin resolving as an FTP URL. */
  status = ifp_url_resolve_cache (url, urlpath, URL_FTP);
  if (!status)
    {
      status = ifp_url_resolve_remote (url, "ftp", hier_part,
                                       21, URL_FTP, ifp_ftp_download);
    }

  ifp_free (urlpath);
  return status;
}


/*
 * ifp_url_resolve_local()
 *
 * A bit of string jiggery-pokery to find the file path to a "file:" URL.
 * If the host id part of the hierarchical part is "//<host_id>", and host_id
 * is not "localhost", the URL is reconsidered as an "ftp:" URL.
 */
static int
ifp_url_resolve_local (ifp_urlref_t url, const char *hier_part)
{
  const char *file_part;
  char *data_file;

  ifp_trace ("url: ifp_url_resolve_local <-"
             " url_%p '%s'", ifp_trace_pointer (url), hier_part);

  /*
   * Find the path to the real file from the URL hierarchical part.  It seems
   * that the right way is to look for // at the front of this.  If
   * //[localhost] is found, then go past that and arrive at the full path to
   * the file.  If //<something_else> seen, then this is an ftp: URL in
   * disguise, so handle it specially.  And if not //, then the whole
   * hierarchical part is just, simply, the path to the file.
   *
   * TODO is all, or even any, of this right?
   */
  if (strncmp (hier_part, "//", strlen ("//")) == 0)
    {
      if (strncmp (hier_part, "///", strlen ("///")) == 0)
        file_part = hier_part + strlen ("//");
      else if (strncmp (hier_part,
                        "//localhost/", strlen ("//localhost/")) == 0)
        file_part = hier_part + strlen ("//localhost");
      else
        {
          /* This URL appears to be an FTP URL in disguise. */
          ifp_trace ("url: decided file: URL is remote");
          return ifp_url_resolve_file_as_ftp (url, hier_part);
        }
    }
  else
    {
      /* Just assume a vanilla file path. */
      file_part = hier_part;
    }
  ifp_trace ("url:"
             " file URL 'file:%s' resolved to file '%s'", hier_part, file_part);

  if (access (file_part, R_OK) == -1)
    {
      ifp_trace ("url: unable to access file '%s'", file_part);
      return FALSE;
    }

  data_file = ifp_malloc (strlen (file_part) + 1);
  strcpy (data_file, file_part);

  url->type = URL_FILE;
  url->state = URL_RESOLVED;
  url->progress = 0;
  url->status = 0;
  url->data_file = data_file;

  return TRUE;
}


/**
 * ifp_url_pause_async()
 *
 * Wait for the next block of an asynchronous transfer, or until some other
 * signaled event occurs.  This function returns immediately if the given
 * URL transfer is already complete.
 *
 * This function should be used only where the Glk library supporting IFP
 * does not implement timers.  If Glk timers are available, those used in
 * in conjunction with glk_select() may be a better way to wait for transfers.
 */
void
ifp_url_pause_async (ifp_urlref_t url)
{
  int usec;
  struct timespec requested, remainder;
  assert (ifp_url_is_valid (url));

  ifp_trace ("url: ifp_url_pause <-"
             " url_%p", ifp_trace_pointer (url));

  /*
   * Poll resolution; this raises SIGIO and checks for completion.  If this
   * results in completion (or was already completed even before we called),
   * return pauselessly.
   */
  if (ifp_url_poll_resolved_async (url))
    return;

  /*
   * This is a minor fiddle.  Sometimes SIGIO transfers get hung up.  Here,
   * in pause, we'll wait for a short period, then return.  The caller will
   * then call ifp_url_poll_resolved_async() or ifp_url_poll_progress_async(),
   * or re-call us, and asynchronous IO transfers are nudged on these calls.
   */
  usec = ifp_url_get_pause_timeout ();
  requested.tv_sec = usec / 1000000;
  requested.tv_nsec = (usec % 1000000) * 1000;

  if (nanosleep (&requested, &remainder) == -1 && errno != EINTR)
    {
      ifp_error ("url: problem sleeping for a short period");
      return;
    }
}


/**
 * ifp_url_resolve_async()
 *
 * Resolve a URL.  If it is a local file, find the path to the file and
 * return it.  If it is an HTTP or FTP URL, begin downloading the data
 * into a temporary file and note the file's path.  Other URL schemes are
 * not supported.
 */
int
ifp_url_resolve_async (ifp_urlref_t url, const char *urlpath)
{
  char *scheme, *urlpath_copy, colon;
  const char *hier_part;
  int status;
  assert (ifp_url_is_valid (url) && urlpath);

  ifp_trace ("url: ifp_url_resolve_async <-"
             " url_%p '%s'", ifp_trace_pointer (url), urlpath);

  if (!ifp_url_install_handler ())
    {
      ifp_error ("url: unable to install async handler");
      return FALSE;
    }

  /* Malloc enough space for the scheme string, at minimum 5 bytes. */
  if (strlen (urlpath) + 1 >= strlen ("file") + 1)
    scheme = ifp_malloc (strlen (urlpath) + 1);
  else
    scheme = ifp_malloc (strlen ("file") + 1);

  /*
   * Begin by looking to see if this is a URL at all.  If not, then fake a
   * file: URL anyway.  For our purposes, we define a URL as a string
   * beginning with a chunk of alphabetical characters (the scheme), followed
   * by a colon.  A pathological case of a file's name containing a colon,
   * something like "my:file" will be incorrectly recognized as a URL with
   * an unrecognized scheme.
   */
  if (sscanf (urlpath, "%[a-zA-Z]%c", scheme, &colon) == 2 && colon == ':')
    {
      /* Set up the hierarchical part pointer to all after scheme. */
      hier_part = urlpath + strlen (scheme) + 1;
    }
  else
    {
      /* Nothing like foobar: found - default to file: */
      strcpy (scheme, "file");
      hier_part = urlpath;
    }

  /* Identify the type of the URL that came in. */
  if (strcasecmp (scheme, "file") == 0)
      status = ifp_url_resolve_local (url, hier_part);

  else if (strcasecmp (scheme, "http") == 0)
    {
      /* Identified the URL as an HTTP one; check the cache first. */
      status = ifp_url_resolve_cache (url, urlpath, URL_HTTP);
      if (!status)
        {
          /* Begin downloading the URL data. */
          status = ifp_url_resolve_remote (url, scheme, hier_part,
                                           80, URL_HTTP, ifp_http_download);
        }
    }

  else if (strcasecmp (scheme, "ftp") == 0)
    {
      /* Identified the URL as an FTP one; check the cache first. */
      status = ifp_url_resolve_cache (url, urlpath, URL_FTP);
      if (!status)
        {
          /* Begin downloading the URL data. */
          status = ifp_url_resolve_remote (url, scheme, hier_part,
                                           21, URL_FTP, ifp_ftp_download);
        }
    }

  else if (strcasecmp (scheme, "mailto") == 0)
    {
      ifp_trace ("url: '%s' URL?  Don't be silly", scheme);
      errno = ENOTSUP;
      status = FALSE;
    }

  else
    {
      ifp_trace ("url: URL scheme '%s' not supported yet", scheme);
      errno = ENOTSUP;
      status = FALSE;
    }

  ifp_free (scheme);

  /* If it resolved or is downloading successfully, add the URL field. */
  if (status)
    {
      urlpath_copy = ifp_malloc (strlen (urlpath) + 1);
      strcpy (urlpath_copy, urlpath);

      url->url_path = urlpath_copy;
    }

  return status;
}


/**
 * ifp_url_resolve()
 *
 * Resolve a URL synchronously.  In the case of remote URLs, this function
 * will wait until the download is complete before returning.
 */
int
ifp_url_resolve (ifp_urlref_t url, const char *urlpath)
{
  int url_status;
  assert (ifp_url_is_valid (url) && urlpath);

  ifp_trace ("url: ifp_url_resolve <-"
             " url_%p '%s'", ifp_trace_pointer (url), urlpath);

  /*
   * Resolve the URL the normal way, then wait for the URL to indicate that
   * it has completed.
   */
  if (!ifp_url_resolve_async (url, urlpath))
    {
      ifp_trace ("url: failed to resolve URL at all");
      return FALSE;
    }

  while (!ifp_url_poll_resolved_async (url))
    {
      ifp_trace ("url: waiting for URL ready");
      ifp_url_pause_async (url);
    }

  url_status = ifp_url_get_status_async (url);
  if (url_status != 0)
    {
      ifp_trace ("url: URL download failed, errno %d", url_status);
      ifp_url_scrub (url);
      errno = url_status;
      return FALSE;
    }

  ifp_trace ("url: url_%p is ready", ifp_trace_pointer (url));
  return TRUE;
}


/**
 * ifp_url_new_resolve_async()
 *
 * Convenience function to create a new URL already resolved to a URL string.
 * If the URL can't be resolved, the function returns NULL.
 */
ifp_urlref_t
ifp_url_new_resolve_async (const char *urlpath)
{
  ifp_urlref_t url;
  assert (urlpath);

  ifp_trace ("url: ifp_url_new_resolve_async <- '%s'", urlpath);

  url = ifp_url_new ();
  if (!ifp_url_resolve_async (url, urlpath))
    {
      ifp_url_destroy (url);
      return NULL;
    }

  return url;
}


/**
 * ifp_url_new_resolve()
 *
 * Convenience function to create a new URL already resolved to a URL string.
 * If the URL can't be resolved, the function returns NULL.
 */
ifp_urlref_t
ifp_url_new_resolve (const char *urlpath)
{
  ifp_urlref_t url;
  assert (urlpath);

  ifp_trace ("url: ifp_url_new_resolve <- '%s'", urlpath);

  url = ifp_url_new ();
  if (!ifp_url_resolve (url, urlpath))
    {
      ifp_url_destroy (url);
      return NULL;
    }

  return url;
}


/**
 * ifp_url_forget()
 *
 * Convenience function to both scrub and destroy a resolved (or unresolved)
 * URL.
 */
void
ifp_url_forget (ifp_urlref_t url)
{
  assert (ifp_url_is_valid (url));

  ifp_trace ("url: ifp_url_forget <-"
             " url_%p", ifp_trace_pointer (url));

  ifp_url_scrub (url);
  ifp_url_destroy (url);
}
