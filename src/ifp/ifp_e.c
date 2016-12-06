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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "glk.h"
#include "glkstart.h"
#include "ifp.h"


/* Glk arguments data. */
glkunix_argumentlist_t glkunix_arguments[] = {
  {.name = (char *) "",
   .argtype = glkunix_arg_ValueCanFollow,
   .desc = (char *) "filename|URL    file or URL to load"},
  {.name = NULL, .argtype = glkunix_arg_End, .desc = NULL}
};

/* Information passed between startup and main. */
static ifp_pluginref_t current_plugin = NULL;
static ifp_urlref_t current_url = NULL;
static char error_message[1024] = "";


/*
 * glkunix_startup_code()
 */
int
glkunix_startup_code (glkunix_startup_t *data)
{
  ifp_urlref_t url;
  ifp_pluginref_t plugin;

  if (data->argc != 2)
    {
      snprintf (error_message, sizeof (error_message),
                "Usage: %s [-glk <library>] <game_file or game URL>\n",
                data->argv[0]);
      return TRUE;
    }

  url = ifp_url_new_resolve (data->argv[1]);
  if (!url)
    {
      snprintf (error_message, sizeof (error_message),
                "Can't find, read, or resolve '%s': %s\n",
                data->argv[1], strerror (errno));
      return TRUE;
    }

  plugin = ifp_manager_locate_plugin_url (url);
  if (!plugin)
    {
      snprintf (error_message, sizeof (error_message),
                "No plugin engine accepted the file/URL '%s'\n",
                data->argv[1]);
      ifp_url_forget (url);
      return TRUE;
    }

  current_url = url;
  current_plugin = plugin;
  return TRUE;
}


/*
 * glk_main()
 */
void
glk_main ()
{
  if (!current_plugin)
    {
      winid_t mainwin;

      mainwin = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
      if (!mainwin)
        {
          fprintf (stderr, "GLK INTERNAL ERROR: can't open main window\n");
          glk_exit ();
        }

      glk_set_window (mainwin);
      glk_set_style (style_Header);
      glk_c_put_string ("Ifpe Error\n\n");
      glk_set_style (style_Normal);
      glk_put_string (error_message);
      glk_exit ();
    }

  ifp_manager_run_plugin (current_plugin);

  ifp_loader_forget_plugin (current_plugin);
  ifp_url_forget (current_url);
}
