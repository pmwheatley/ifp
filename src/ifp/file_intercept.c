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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Open files information structures.  Entries indicate either an open file
 * descriptor or an open file stream.
 */
struct ifp_fd_info
{
  int fd;
  struct ifp_fd_info *next;
};
typedef struct ifp_fd_info *ifp_fd_inforef_t;

struct ifp_stream_info
{
  FILE* stream;
  struct ifp_stream_info *next;
};
typedef struct ifp_stream_info *ifp_stream_inforef_t;

/* List of open files and open streams. */
static ifp_fd_inforef_t ifp_fd_list = NULL;
static ifp_stream_inforef_t ifp_stream_list = NULL;


/*
 * ifp_file_add_file()
 * ifp_file_add_stream()
 * ifp_file_remove_file()
 * ifp_file_remove_stream()
 *
 * Add and remove files and streams to/from the open file lists.
 *
 * Unlike our malloc-tracking cousin, we're a lot less strict about seeing
 * attempts to, say, delete a file not on our list.  This is because many
 * more library functions return file descriptors than return malloc'ed
 * memory that needs to be freed.  For malloc, we are really strict; for
 * files, we just make the best effort to cover the most we can without re-
 * implementing too many functions.
 */
static void
ifp_file_add_file (int fd)
{
  ifp_fd_inforef_t entry;

  entry = ifp_malloc (sizeof (*entry));
  entry->fd = fd;
  entry->next = ifp_fd_list;
  ifp_fd_list = entry;
  ifp_trace ("file: added file descriptor %d", fd);
}

static void
ifp_file_add_stream (FILE *stream)
{
  ifp_stream_inforef_t entry;

  entry = ifp_malloc (sizeof (*entry));
  entry->stream = stream;
  entry->next = ifp_stream_list;
  ifp_stream_list = entry;
  ifp_trace ("file: added file stream file_%p", ifp_trace_pointer (stream));
}

static void
ifp_file_remove_file (int fd)
{
  ifp_fd_inforef_t entry, prior, next;

  /* Search for the given file, and remove it if found. */
  prior = NULL;
  for (entry = ifp_fd_list; entry; entry = next)
    {
      next = entry->next;
      if (entry->fd == fd)
        {
          ifp_trace ("file: removed file descriptor %d", fd);
          if (!prior)
            {
              assert (entry == ifp_fd_list);
              ifp_fd_list = next;
            }
          else
            prior->next = next;

          ifp_free (entry);
        }
      else
        prior = entry;
    }
}

static void
ifp_file_remove_stream (FILE *stream)
{
  ifp_stream_inforef_t entry, prior, next;

  /* Search for the given stream, and remove it if found. */
  prior = NULL;
  for (entry = ifp_stream_list; entry; entry = next)
    {
      next = entry->next;
      if (entry->stream == stream)
        {
          ifp_trace ("file: removed file stream"
                     " file_%p", ifp_trace_pointer (stream));
          if (!prior)
            {
              assert (entry == ifp_stream_list);
              ifp_stream_list = next;
            }
          else
            prior->next = next;

          ifp_free (entry);
        }
      else
        prior = entry;
    }
}


/*
 * ifp_libc_intercept_open_2()
 * ifp_libc_intercept_open_3()
 * ifp_libc_intercept_creat()
 * ifp_libc_intercept_dup()
 * ifp_libc_intercept_dup2()
 * ifp_libc_intercept_close()
 *
 * Intercept function for calls to open, close, and other file-related
 * operations.  These call the real libc functions, and keep each file or
 * stream returned on a list.  Files closed are removed from the list.
 * On finalization, files that remain on the list are closed out.
 */
int
ifp_libc_intercept_open_2 (const char *pathname, int flags)
{
  int fd;

  fd = open (pathname, flags);
  if (fd != -1)
    ifp_file_add_file (fd);

  return fd;
}

int
ifp_libc_intercept_open_3 (const char *pathname, int flags, mode_t mode)
{
  int fd;

  fd = open (pathname, flags, mode);
  if (fd != -1)
    ifp_file_add_file (fd);

  return fd;
}

int
ifp_libc_intercept_close (int fd)
{
  int status;

  status = close (fd);
  if (status != -1)
    ifp_file_remove_file (fd);

  return status;
}

int
ifp_libc_intercept_creat (const char *pathname, mode_t mode)
{
  int fd;

  fd = creat (pathname, mode);
  if (fd != -1)
    ifp_file_add_file (fd);

  return fd;
}

int
ifp_libc_intercept_dup (int oldfd)
{
  int fd;

  fd = dup (oldfd);
  if (fd != -1)
    ifp_file_add_file (fd);

  return fd;
}

int
ifp_libc_intercept_dup2 (int oldfd, int newfd)
{
  int fd;

  fd = dup2 (oldfd, newfd);
  if (fd != -1)
    {
      /* Remove newfd if listed, as dup2 will have closed it. */
      ifp_file_remove_file (newfd);
      ifp_file_add_file (fd);
    }

  return fd;
}


/*
 * ifp_libc_intercept_fopen()
 * ifp_libc_intercept_fdopen()
 * ifp_libc_intercept_freopen()
 * ifp_libc_intercept_fclose()
 *
 * More intercept function for calls to fopen, fclose, and other stream-
 * related operations.
 */
FILE *
ifp_libc_intercept_fopen (const char *path, const char *mode)
{
  FILE *stream;

  stream = fopen (path, mode);
  if (stream)
    ifp_file_add_stream (stream);

  return stream;
}

FILE *
ifp_libc_intercept_fdopen (int filedes, const char *mode)
{
  FILE *stream;

  stream = fdopen (filedes, mode);
  if (stream)
    {
      /* Remove filedes, and list the stream instead. */
      ifp_file_remove_file (filedes);
      ifp_file_add_stream (stream);
    }

  return stream;
}

FILE *
ifp_libc_intercept_freopen (const char *path, const char *mode, FILE * stream)
{
  FILE *new_stream;

  new_stream = freopen (path, mode, stream);
  if (new_stream)
    {
      /* Remove stream if listed, as freopen will have closed it. */
      ifp_file_remove_stream (stream);
      ifp_file_add_stream (new_stream);
    }

  return new_stream;
}

int
ifp_libc_intercept_fclose (FILE * stream)
{
  int status;

  status = fclose (stream);
  if (status != EOF)
    ifp_file_remove_stream (stream);

  return status;
}


/**
 * ifp_file_open_files_cleanup()
 *
 * Close any files and streams that are listed as left open by a completed
 * plugin.
 */
void
ifp_file_open_files_cleanup (void)
{
  ifp_fd_inforef_t fd_entry, fd_next;
  ifp_stream_inforef_t stream_entry, stream_next;

  ifp_trace ("file: ifp_file_open_files_cleanup <- void");

  for (fd_entry = ifp_fd_list;
       fd_entry; fd_entry = fd_next)
    {
      fd_next = fd_entry->next;

      close (fd_entry->fd);
      ifp_trace ("file: reaped file descriptor %d", fd_entry->fd);
      ifp_free (fd_entry);
    }
  ifp_fd_list = NULL;

  for (stream_entry = ifp_stream_list;
       stream_entry; stream_entry = stream_next)
    {
      stream_next = stream_entry->next;

      fclose (stream_entry->stream);
      ifp_trace ("file: reaped file stream"
                 " file_%p", ifp_trace_pointer (stream_entry->stream));
      ifp_free (stream_entry);
    }
  ifp_stream_list = NULL;
}
