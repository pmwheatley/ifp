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

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Duplication of the definition of a Glk internal function that is common to
 * all three main Glk implementations (Xglk, Glkterm, and Cheapglk).  For a
 * Glk library to work with IFP, it must either define this function, or
 * allow its glkunix_stream_open_pathname() to be called from glk_main() as
 * well as from glkunix_startup_code().
 *
 * This is rather icky coding.  Here, we're relying on the fact that this
 * Glk internal function exists, with this exact signature, in each Glk
 * library we build with (well, either this exists, or the Glk library's
 * glkunix_stream_open_pathname() does not have the only-available-inside-
 * glkunix_startup_code()-calls restriction...).  Sorry.
 */
extern strid_t gli_stream_open_pathname (char *pathname,
                                         int textmode, glui32 rock);
extern strid_t fallback_gli_stream_open_pathname (char *pathname,
                                                  int textmode, glui32 rock);


/*
 * fallback_gli_stream_open_pathname()
 *
 * Directly using gli_stream_open_pathname() is an abuse of Glk, but we need
 * it to open files without Glk pathname mangling.  Ordinarily, we'd use a
 * weak version of the function here, but because we already have a weak
 * version in the Glk loader, we have to do something even "weaker".
 *
 * The solution here is to have a "fallback" version of the function.  On
 * loading Glk, if the library loaded has no gli_stream_open_pathname(), this
 * function is inserted into the Glk interface in its place.  If it gets
 * called, then, this means that the Glk library opened by IFP has no
 * gli_stream_open_pathname() of its own.  This is a problem area: this Glk
 * library can't be used directly with IFP without further work.
 */
strid_t
fallback_gli_stream_open_pathname (char *pathname, int textmode, glui32 rock)
{
  strid_t glk_stream;

  ifp_trace ("glkstream: fallback_gli_stream_open_pathname <-"
             " '%s' %d %ld", pathname, textmode, rock);

  /*
   * Even with all of the above, it's possible that a call to the function
   * ifp_glkstream_open_pathname() could end up here when in a chaining
   * plugin.  So we need to retry the public Glk function here too.
   */
  glk_stream = glkunix_stream_open_pathname (pathname, textmode, rock);
  if (glk_stream)
    {
      ifp_trace ("glkstream:"
                 " stream opened with glkunix_stream_open_pathname()");
      return glk_stream;
    }

  /*
   * For now, if we arrive here, this function just returns a null stream.
   *
   * It is probably possible to cobble together a working version of something
   * to replicate gli_stream_open_pathname() using standard Glk functions,
   * alongside normal file calls.  For example, something like
   *
   *   fd = open (pathname, O_RDONLY);
   *   glk_stream = glk_fileref_create_temp (filemode_ReadWrite, 0);
   *   bytes = read (fd, buffer, sizeof(buffer);
   *   while (bytes > 0)
   *       {
   *           glk_put_buffer_stream (glk_stream, buffer, bytes);
   *           bytes = read (fd, buffer, sizeof(buffer) != 0)
   *       }
   *   close (fd);
   *   if (bytes < 0)
   *       {
   *           error (...);
   *           glk_stream_close (glk_stream, NULL);
   *           return NULL;
   *       }
   *   glk_stream_set_position (glk_stream, 0, seekmode_Start);
   *   return glk_stream;
   *
   * But it's not pretty.  And it's somewhat inefficient, too.
   *
   * One further point: if this function is ever called, it's always the
   * version in the main program.  A chaining plugin will pick up the symbol
   * gli_stream_open_pathname from libifppi, and pass the call through the
   * Glk interface to the main program.
   *
   * Oh, and one final note -- for some reason, compiling with -O3 somehow
   * loses the real gli_stream_open_pathname(), and always drops us into
   * here...  -O2 seems okay, though.
   */

  ifp_error ("glkstream: gli_stream_open_pathname() not implemented");
  return NULL;
}


/*
 * ifp_glkstream_open_pathname()
 *
 * Opens a Glk stream directly to a given path.  This function bypasses
 * any path name mangling that the Glk library might apply when opening
 * a fileref by name and then opening the file.  It behaves by first
 * calling glkunix_stream_open_pathname().  This will work if we are in
 * a glkunix_startup_code() call, and also in glk_main() if the library
 * does not enforce that this function may be called only inside of
 * glkunix_startup_code().  If this call fails, then it tries to invoke
 * gli_stream_open_pathname() instead.  This is a Glk internal function
 * that happens to be present in all three main Glk libraries (Xglk,
 * Glkterm, and Cheapglk).  Should this fail, the function returns NULL.
 */
strid_t
ifp_glkstream_open_pathname (char *pathname, glui32 textmode, glui32 rock)
{
  strid_t glk_stream;
  assert (pathname);

  ifp_trace ("glkstream: ifp_glkstream_open_pathname <-"
             " '%s' %lu %ld", pathname, textmode, rock);

  /*
   * First, try the front door.  If the stream can be opened using the
   * publicly-available, and advertised, Glk function, fine.
   */
  glk_stream = glkunix_stream_open_pathname (pathname, textmode, rock);
  if (glk_stream)
    {
      ifp_trace ("glkstream:"
                 " stream opened with glkunix_stream_open_pathname()");
      return glk_stream;
    }

  /*
   * Now, try the back door.  If we see NULL back from here, then either the
   * open genuinely fails, or we've run into the weak gli_stream_open_pathname
   * that we carry about to satisfy calls to this function where the Glk
   * library we have happens not to define it.
   */
  glk_stream = gli_stream_open_pathname (pathname, (textmode != 0), rock);
  if (glk_stream)
    {
      ifp_trace ("glkstream: stream opened with gli_stream_open_pathname()");
      return glk_stream;
    }

  return NULL;
}


/*
 * ifp_glkstream_close()
 *
 * Close a Glk stream opened with ifp_glkstream_open_pathname().
 */
void
ifp_glkstream_close (strid_t glk_stream, stream_result_t *result)
{
  ifp_trace ("glkstream: ifp_glkstream_close <- stream_%p result_%p",
             ifp_trace_pointer (glk_stream), ifp_trace_pointer (result));

  glk_stream_close (glk_stream, result);
}
