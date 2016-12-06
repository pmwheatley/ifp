/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2006-2007  Simon Baldwin (simon_baldwin@yahoo.com)
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
 * USA.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ifp.h>
#include <ifp_internal.h>

#include "protos.h"


/* Glk arguments list. */
glkunix_argumentlist_t ifpi_glkunix_arguments[] = {
  {.name = (char *) "",
   .argtype = glkunix_arg_ValueCanFollow,
   .desc = (char *) "XML/INI_file"
                    "    XML or INI game collection description file"},
  {.name = NULL, .argtype = glkunix_arg_End, .desc = NULL}
};


/*
 * override_startup_code()
 *
 * Personalized startup_code() function, parses the input file and builds
 * from it a set of games to offer in the user dialog.
 *
 * This function cannot safely call glk_exit().  Here's why:
 *
 * When a plugin that we chain to completes, it may call glk_exit().  This
 * call somehow "uses up" the jump buffer that we had set up for us.  It's
 * not clear how, perhaps an IFP bug, but it does.  Any subsequent glk_exit()
 * that we make will then abort on assertion failure.  For now, it's handled
 * by just not calling glk_exit() from within Gamebox, but instead making
 * sure that glk_startup_code() and glk_main() return.
 */
static int
override_startup_code (int argc, char *argv[])
{
  int parse_status, debug_dump;
  char *meta_file, header[12];
  strid_t stream;

  if (argc < 1)
    return FALSE;

  meta_file = argv[1];
  debug_dump = (getenv ("GAMEBOX_DEBUG_DUMP") != NULL);

  /* [Re-]read the magic header from the input file. */
  stream = glkunix_stream_open_pathname (meta_file, FALSE, 0);
  if (stream)
    {
      if (glk_get_buffer_stream (stream, header, sizeof (header))
          != sizeof (header))
        {
          glk_stream_close (stream, NULL);
          return FALSE;
        }
      glk_stream_close (stream, NULL);
    }
  else
    return FALSE;

  /* Parse according to the header read in. */
  if (memcmp (header, "<?xml", 5) == 0)
    parse_status = xml_parse_file (meta_file, debug_dump);

  else if (memcmp (header + 1, "GAMEBOX_0.4", 11) == 0
           || memcmp (header + 1, "GAMEBOX_0.3", 11) == 0)
    parse_status = ini_parse_file (meta_file, debug_dump);

  else
    parse_status = FALSE;

  if (!parse_status || !gamegroup_get_root ())
    {
      gameset_erase ();
      gamegroup_erase ();
      return FALSE;
    }

  return TRUE;
}


/*
 * override_main()
 *
 * Personalized main() function, takes user requests for games and finds and
 * runs a plugin interpreter for each selected.
 *
 * This function cannot safely call glk_exit().  See above for why.  And if
 * that's not enough, we also want to restore glkunix_stream_open_pathname(),
 * overridden by our caller, and restored when we return.  So it's doubly
 * important here.
 */
static void
override_main (void)
{
  /* Discover interpreters, then run the main dialog loop. */
  terps_discover ();
  display_main_loop ();

  /* Reset SIGIO handlers if changed by our local URL resolver. */
  url_cleanup ();

  /*
   * Free allocated memory for tidiness, even though IFP will garbage collect
   * if we don't bother.
   */
  gameset_erase ();
  gamegroup_erase ();
  terps_erase ();
}


/*
 * override_glkunix_stream_open_pathname()
 *
 * Abuse the Glk library, by taking over its implementation of the
 * glkunix_stream_open_pathname() function.  This removes the restriction on
 * this routine being available only at "initialization" time.
 *
 * Glk prevents a game from opening an arbitrary disk file, but this
 * restriction must be lifted at init time in order for an interpreter to
 * begin to run a game at all.  Here, by shifting the Glk function in the
 * interface into here, we hijack the function and can now bypass this
 * restriction.  We could just shift gli_stream_open_pathname() directly,
 * but this way gives a useful location for a debugger breakpoint if needed.
 */
extern strid_t gli_stream_open_pathname (char *, glui32, glui32);
extern strid_t override_glkunix_stream_open_pathname (char *, glui32, glui32);
strid_t
override_glkunix_stream_open_pathname (char *pathname, glui32 textmode,
                                       glui32 rock)
{
  return gli_stream_open_pathname (pathname, (textmode != 0), rock);
}


/* State monitoring for safety, and to validate calling code. */
static enum { READY, IN_STARTUP, INITIALIZED, IN_MAIN, DEAD } state = READY;

/*
 * ifpi_glkunix_startup_code()
 *
 * Capture routine for startup_code().  Calls the personalized startup_code()
 * function.
 *
 * This function cannot safely call glk_exit().  See above for why.
 */
int
ifpi_glkunix_startup_code (glkunix_startup_t *data)
{
  int status;
  assert (state == READY);

  state = IN_STARTUP;
  status = override_startup_code (data->argc, data->argv);
  state = INITIALIZED;

  return status;
}


/*
 * ifpi_glk_main()
 *
 * Capture routine for main().  Install our glkunix_stream_open_pathname()
 * into the Glk interface, then call the personalized main() function.
 *
 * This function cannot safely call glk_exit().  See above for why.
 */
void
ifpi_glk_main (void)
{
  ifp_glk_interfaceref_t glk;
  strid_t (*old_glkunix_stream_open_pathname) (char *, glui32, glui32);
  assert (state == INITIALIZED);

  /*
   * Override glkunix_stream_open_pathname(), saving the old copy.  We have
   * to override this so that interpreter plugins we call can successfully
   * open streams in their own glk_startup_code() functions.
   */
  glk = ifp_glk_get_interface ();
  old_glkunix_stream_open_pathname = glk->glkunix_stream_open_pathname;
  glk->glkunix_stream_open_pathname = override_glkunix_stream_open_pathname;

  state = IN_MAIN;
  override_main ();
  state = DEAD;

  /*
   * Restore glkunix_stream_open_pathname().  This avoids leaving pointers
   * to our code, about to be unloaded, in the Glk interface.
   */
  glk->glkunix_stream_open_pathname = old_glkunix_stream_open_pathname;
}
