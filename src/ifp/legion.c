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
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "glk.h"
#include "glkstart.h"
#include "ifp.h"
#include "ifp_internal.h"  /* Needed for overriding a Glk function. */


/* Glk arguments data. */
glkunix_argumentlist_t glkunix_arguments[] = {
  {.name = (char *) "",
   .argtype = glkunix_arg_ValueCanFollow,
   .desc = (char *) "filename|URL    file or URL to load"},
  {.name = NULL, .argtype = glkunix_arg_End, .desc = NULL}
};

/*
 * A main window for plugin details, and a message and input window below
 * the main window.  Messaging functions use the main window if no lower
 * input window is possible.
 */
static winid_t main_window = NULL,
               message_window = NULL;

/*
 * Timeout definitions; a baseline timer interval and timeouts in units
 * of 0.1 second.
 */
static const int TIMEOUT_BASELINE = 100,
                 MESSAGE_TIMEOUT = 50,
                 URL_PAUSE_TIMEOUT = 5;

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
override_glkunix_stream_open_pathname (char *pathname,
                                       glui32 textmode, glui32 rock)
{
  return gli_stream_open_pathname (pathname, (textmode != 0), rock);
}


/*
 * message_begin_dialog()
 * message_read_line()
 * message_write_line()
 *
 * Helper functions to implement a single-line message and prompted input
 * window at the bottom of the frame.  If the Glk in use doesn't permit
 * windows, these functions degrade to use the main window instead.  The
 * message print can either return immediately, or wait until keypress or
 * about five seconds have elapsed.
 */
static void
message_begin_dialog (void)
{
  /* If no windows, due, say, to plugin cleanup, invalidate references. */
  if (!glk_window_get_root ())
    {
      main_window = NULL;
      message_window = NULL;
    }

  /* If not yet, or no longer, a message window, carve one out. */
  if (!message_window)
    {
      winid_t window;

      /*
       * Try for a message window at the base of the main window, which
       * may or may not be ours.  And if there's no main window available,
       * create one.
       */
      window = glk_window_get_root ();
      if (!window)
        {
          main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
          if (!main_window)
            {
              fprintf (stderr, "GLK INTERNAL ERROR: can't open main window\n");
              glk_exit ();
            }

          window = main_window;
        }

      message_window = glk_window_open (window,
                                        winmethod_Below|winmethod_Fixed,
                                        1, wintype_TextGrid, 0);
    }

  if (message_window)
    glk_window_clear (message_window);
}

static void
message_read_line (const char *prompt, char *buffer, int length)
{
  winid_t window;
  strid_t stream;
  event_t event;

  message_begin_dialog ();

  window = message_window ? message_window : glk_window_get_root ();
  stream = glk_window_get_stream (window);

  if (!message_window)
    glk_put_char_stream (stream, '\n');
  glk_c_put_string_stream (stream, prompt);

  glk_request_line_event (window, buffer, length - 1, strlen (buffer));

  do
    {
      glk_select (&event);
    }
  while (event.type != evtype_LineInput);

  buffer[event.val1] = '\0';
}

static void __attribute__ ((format (printf, 2, 3)))
message_write_line (int return_immediately, const char *format, ...)
{
  winid_t window;
  strid_t stream;
  event_t event;
  int timeouts = 0;
  va_list ap;
  char message[1024];

  message_begin_dialog ();

  window = message_window ? message_window : glk_window_get_root ();
  stream = glk_window_get_stream (window);

  va_start (ap, format);
  vsnprintf (message, sizeof (message), format, ap);
  va_end (ap);

  glk_c_put_string_stream (stream, message);

  if (return_immediately)
    {
      if (!message_window)
        glk_put_char_stream (stream, '\n');
      return;
    }

  if (!message_window)
    glk_c_put_string_stream (stream, "\n<Press Return to continue...>");

  glk_request_char_event (window);
  if (glk_gestalt (gestalt_Timer, 0))
    {
      glk_request_timer_events (TIMEOUT_BASELINE);
      timeouts = 0;
    }

  do
    {
      glk_select (&event);
      if (event.type == evtype_Arrange || event.type == evtype_Redraw)
        {
          message_begin_dialog ();
          glk_c_put_string_stream (stream, message);
          if (!message_window)
            glk_c_put_string_stream (stream, "\n<Press Return to continue...>");
        }
      else if (event.type == evtype_Timer)
        timeouts++;
    }
  while (event.type != evtype_CharInput && timeouts < MESSAGE_TIMEOUT);

  glk_cancel_char_event (window);
  if (glk_gestalt (gestalt_Timer, 0))
    glk_request_timer_events (0);
}


/*
 * print_converted_date()
 *
 * Shrink a standard IFP engine timestamp down from "mmm dd yyyy, hh:mm:ss"
 * to just "mmm dd yyyy".
 */
static void
print_converted_date (const char *ifp_date)
{
  int year, day, hours, minutes, seconds, count;
  char trailer;

  /* Check the format of the string before removing the time portion. */
  count = sscanf (ifp_date, " %*3s %2d %4d, %2d:%2d:%2d%c",
                  &day, &year, &hours, &minutes, &seconds, &trailer);
  if (!(count == 5 || (count == 6 && trailer == ' ')))
    {
      glk_c_put_string (ifp_date);
      return;
    }

  if (day < 1 || day > 31)
    {
      glk_c_put_string (ifp_date);
      return;
    }

  glk_c_put_buffer (ifp_date, strchr (ifp_date, ',') - ifp_date);
}


/*
 * print_plugin_details()
 *
 * Write a plugin's header details to the given Glk window.
 */
static void
print_plugin_details (ifp_pluginref_t plugin, winid_t window)
{
  strid_t stream;
  int version, acceptor_offset, acceptor_length;
  const char *engine_type, *engine_name, *engine_version, *build_timestamp,
             *blorb_pattern, *acceptor_pattern, *author_name, *author_email,
             *engine_home_url, *builder_name, *builder_email,
             *engine_description, *engine_copyright;

  stream = glk_window_get_stream (window);

  ifp_plugin_dissect_header (plugin, &version,
                             &engine_type, &engine_name, &engine_version,
                             &build_timestamp, &blorb_pattern,
                             &acceptor_offset, &acceptor_length,
                             &acceptor_pattern, &author_name, &author_email,
                             &engine_home_url, &builder_name, &builder_email,
                             &engine_description, &engine_copyright);

  glk_set_style (style_Subheader);
  glk_c_put_string (engine_name);
  glk_set_style (style_Normal);

  if (engine_version)
    {
      glk_c_put_string (" version ");
      glk_c_put_string (engine_version);
    }

  if (engine_type)
    {
      glk_c_put_string (" (");
      glk_c_put_string (engine_type);
      glk_put_char (')');
    }
  glk_put_char ('\n');

  if (author_name)
    {
      glk_c_put_string ("Author: ");
      glk_set_style (style_Emphasized);
      glk_c_put_string (author_name);
      glk_set_style (style_Normal);

      if (author_email)
        {
          glk_set_style (style_Emphasized);
          glk_c_put_string (" <");
          glk_c_put_string (author_email);
          glk_put_char ('>');
          glk_set_style (style_Normal);
        }
    }

  if (build_timestamp)
    {
      glk_c_put_string ("  Built: ");
      glk_set_style (style_Emphasized);
      print_converted_date (build_timestamp);
      glk_set_style (style_Normal);
    }

  if (author_name || build_timestamp)
    glk_put_char ('\n');

  if (engine_description)
    glk_c_put_string (engine_description);

  if (engine_copyright)
    {
      glk_set_style (style_Emphasized);
      glk_c_put_string ("Licensing: ");
      glk_set_style (style_Normal);
      glk_c_put_string (engine_copyright);
    }

  if (engine_home_url)
    {
      glk_c_put_string ("Home Page: ");
      glk_set_style (style_Emphasized);
      glk_c_put_string (engine_home_url);
      glk_set_style (style_Normal);
      glk_put_char ('\n');
    }

  glk_put_char ('\n');
}


/*
 * print_splash_screen()
 *
 * Load up and print the details of every plugin that the program can find.
 */
static void
print_splash_screen (winid_t window, int plugin_count_only)
{
  strid_t stream;
  int plugin_count;
  char buffer[1024];
  glui32 glk_version;

  stream = glk_window_get_stream (window);

  glk_set_style_stream (stream, style_Header);
  glk_c_put_string_stream (stream, "\n\n        Welcome to Legion\n\n");
  glk_set_style_stream (stream, style_Normal);

  glk_c_put_string_stream (stream,
    "Legion is an Interactive Fiction game playing program that can use a"
    " selection of different interpreter engines to run a game file for you."
    " It tries to select the right interpreter for the game file that you"
    " give it.  You can also give Legion a URL, and it will play the game file"
    " found from downloading that URL.\n\n");

  ifp_loader_search_plugins_path (ifp_manager_get_plugin_path ());
  plugin_count = ifp_loader_count_plugins ();

  if (plugin_count == 0)
    glk_c_put_string_stream (stream,
      "You do not have any interpreter plugins available at the moment."
      " Try setting a value for the environment variable IF_PLUGIN_PATH,"
      " or if you have set a value, please check it.  Without plugins, Legion"
      " will not be able to play any Interactive Fiction games.\n\n");
  else
    {
      if (plugin_count_only)
        {
          snprintf (buffer, sizeof (buffer),
                    "Legion found %d interpreter engine%s on your system.\n\n",
                    plugin_count, plugin_count > 1 ? "s" : "");
          glk_c_put_string_stream (stream, buffer);
        }
      else
        {
          ifp_pluginref_t plugin;

          glk_c_put_string_stream (stream,
            "Legion found the following interpreter engines available"
            " on your system:\n\n");

          for (plugin = ifp_loader_iterate_plugins (NULL); plugin;
               plugin = ifp_loader_iterate_plugins (plugin))
            print_plugin_details (plugin, window);
        }
    }

  glk_version = glk_gestalt (gestalt_Version, 0);
  snprintf (buffer, sizeof (buffer),
            "Legion loaded a version %lu.%lu.%lu Glk library",
            glk_version >> 16, (glk_version >> 8) & 0xff, glk_version & 0xff);
  glk_c_put_string_stream (stream, buffer);

  glk_c_put_string_stream (stream, ", supporting ");
  glk_c_put_string_stream (stream,
                           glk_gestalt (gestalt_Timer, 0) ? "" : "no ");
  glk_c_put_string_stream (stream, "timers, ");
  glk_c_put_string_stream (stream,
                           glk_gestalt (gestalt_Graphics, 0) ? "" : "no ");
  glk_c_put_string_stream (stream, "graphics, ");
  glk_c_put_string_stream (stream,
                           glk_gestalt (gestalt_Sound, 0) ? "" : "no ");
  glk_c_put_string_stream (stream, "sound, ");
  glk_c_put_string_stream (stream,
                           glk_gestalt (gestalt_Hyperlinks, 0) ? "" : "no ");
  glk_c_put_string_stream (stream, "hyperlinks, and ");
  glk_c_put_string_stream (stream,
                           glk_gestalt (gestalt_Unicode, 0) ? "" : "no ");
  glk_c_put_string_stream (stream, "unicode.\n\n");

  glk_c_put_string_stream (stream,
    "Legion contains code from Glk.  For more information on Glk, visit"
    " 'http://www.eblong.com/zarf/glk/index.html', or contact"
    " Andrew Plotkin <erkyrath@eblong.com>.\n\n");

  glk_set_style_stream (stream, style_Emphasized);
  glk_c_put_string_stream (stream,
    "Legion is copyright (C) 2001-2007"
    "  Simon Baldwin (simon_baldwin@yahoo.com)\n");
  glk_set_style_stream (stream, style_Normal);

  glk_c_put_string_stream (stream,
    "This program is free software; you can redistribute it and/or modify it"
    " under the terms of the GNU General Public License as published by the"
    " Free Software Foundation; either version 2 of the License, or (at your"
    " option) any later version.\n\n");

  glk_c_put_string_stream (stream,
    "This release of Legion was built using IFP version '");
  glk_c_put_string_stream (stream, ifp_manager_build_timestamp ());
  glk_c_put_string_stream (stream, "'.\n\n");
}


/*
 * resolve_url_from_path()
 *
 * Resolve a URL, printing regular status update messages where the download
 * is lengthy.  If there's a problem, url_errno is handed the details.
 */
static ifp_urlref_t
resolve_url_from_path (const char *url_path, int *url_errno)
{
  ifp_urlref_t url;
  int url_progress, url_status;

  url = ifp_url_new_resolve_async (url_path);
  if (!url)
    {
      *url_errno = errno;
      return NULL;
    }

  url_progress = 0;
  while (!ifp_url_poll_resolved_async (url))
    {
      event_t event;
      int progress;

      glk_select_poll (&event);
      progress = ifp_url_poll_progress_async (url);

      if (progress > url_progress
          || event.type == evtype_Arrange || event.type == evtype_Redraw)
        {
          message_write_line (TRUE, "Downloading, %d bytes...", progress);
          url_progress = progress;
        }

      /*
       * If we have Glk timers, use those to wait in between URL polls.
       * This both keeps the display refreshed and avoids breakage in
       * at least one Glk library's glk_select_poll() (you know who you
       * are!).  If no Glk timers, fall back to using a URL pause with
       * a prior glk_select_poll() in hopes of keeping the display fresh.
       */
      if (glk_gestalt (gestalt_Timer, 0))
        {
          int timeouts;

          timeouts = 0;
          glk_request_timer_events (TIMEOUT_BASELINE);
          do
            {
              glk_select (&event);
              if (event.type == evtype_Timer)
                timeouts++;
            }
          while (timeouts < URL_PAUSE_TIMEOUT);
          glk_request_timer_events (0);
        }
      else
        {
          glk_select_poll (&event);
          ifp_url_pause_async (url);
        }
    }

  /* If the URL failed to resolve, destroy it and return NULL. */
  url_status = ifp_url_get_status_async (url);
  if (url_status != 0)
    {
      ifp_url_forget (url);
      *url_errno = url_status;
      return NULL;
    }

  return url;
}


/*
 * override_main()
 *
 * This is our version of the main() function in Glk.  In effect, it overrides
 * most of the main() in Glk - it's called from glk_main, and never returns,
 * and glkunix_startup_code is a stub.  For reference, here's the usual Glk
 * main() code:
 *
 * int main(int argc, char *argv[])
 * {
 *   int err;
 *   glkunix_startup_t startdata;
 *
 *   err = xglk_init(argc, argv, &startdata);
 *   if (!err) {
 *     fprintf(stderr, "%s: exiting.\n", argv[0]);
 *     return 1;
 *   }
 *
 *   inittime = TRUE;
 *   if (!glkunix_startup_code(&startdata)) {
 *     glk_exit();
 *   }
 *   inittime = FALSE;
 *
 *   glk_main();
 *   glk_exit();
 *
 *   return 0;
 * }
 *
 * By making glkunix_startup_code() a stub, and calling this function from a
 * call to our glk_main, we've overridden the final 2/3 of this function.
 * Overriding glkunix_stream_open_pathname() with our own version of this
 * function negates the inittime flag in the above code.
 *
 * Really, we should override main properly...
 */
static void
override_main (int argc, char *argv[])
{
  char url_path[1024] = "";

  main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
  if (!main_window)
    {
      fprintf (stderr, "GLK INTERNAL ERROR: can't open main window\n");
      glk_exit ();
    }

  if (argc > 2)
    {
      char message[1024];

      glk_set_window (main_window);
      glk_set_style (style_Header);
      glk_c_put_string ("Legion Error\n\n");
      glk_set_style (style_Normal);

      snprintf (message, sizeof (message),
                "Usage: %s [-glk <library>] [game_file or game URL]\n",
                argv[0]);
      glk_c_put_string (message);

      glk_exit ();
    }

  /*
   * Display a welcome screen.  If the game loads quickly this may not be
   * displayed.  Otherwise, it's something to read while downloading.  Here
   * we create a message window if we can ahead of time, as it saves a bit of
   * a screen shuffle later on.  We can also do a shorter splash for non-
   * windowing Glk libraries.
   */
  glk_set_window (main_window);
  message_begin_dialog ();
  print_splash_screen (main_window, message_window == NULL);

  /* If a URL path was given use it; if not, get a new one. */
  if (argc == 2)
    {
      strncpy (url_path, argv[1], sizeof (url_path) - 1);
      url_path[sizeof (url_path) - 1] = '\0';
    }
  else
    {
      message_read_line ("Enter a file path or URL: ",
                         url_path, sizeof (url_path));

      if (strlen (url_path) == 0)
        glk_exit ();
    }

  /*
   * Resolve the URL path into a real URL.  If it resolves, run the game
   * it contains, or at least try to, then request a new URL path.  Continue
   * until the user enters an empty URL path.
   */
  do
    {
      ifp_urlref_t url;
      int url_errno;

      url = resolve_url_from_path (url_path, &url_errno);
      if (url)
        {
          ifp_pluginref_t plugin;

          plugin = ifp_manager_locate_plugin_url (url);
          if (plugin)
            {
              /*
               * This looks dodgy, but it's not.  On success, the plugin
               * location call will have fully reset Glk, including
               * closing all windows.  So anything we hold is now invalid.
               */
              main_window = NULL;
              message_window = NULL;

              ifp_manager_run_plugin (plugin);
              ifp_loader_forget_plugin (plugin);
            }
          else
              message_write_line (FALSE, "No plugin"
                                  " engine accepted the file or URL.");

          /* Clearing plugins list preserves first-found ordering. */
          ifp_loader_forget_all_plugins ();
          ifp_url_forget (url);
        }
      else
        message_write_line (FALSE, "Invalid file path or URL [%s].",
                            strerror (url_errno));

      message_read_line ("Enter a file path or URL: ",
                         url_path, sizeof (url_path));
    }
  while (strlen (url_path) > 0);

  glk_exit ();
}


/* Argc and argv, captured by the glkunix_startup_code() call. */
static int noted_argc = 0;
static char **noted_argv = NULL;

/*
 * glkunix_startup_code()
 *
 * Capture the argc and argv passed in, and return TRUE so that Glk's
 * main() continues on to call our glk_main().
 */
int
glkunix_startup_code (glkunix_startup_t *data)
{
  noted_argc = data->argc;
  noted_argv = data->argv;

  return TRUE;
}


/*
 * glk_main()
 *
 * Capture routine for main().  Install our glkunix_stream_open_pathname()
 * into the Glk interface, then call the personalized main() function.
 */
void
glk_main (void)
{
  assert (noted_argc > 0 && noted_argv);
  ifp_glk_interfaceref_t glk;

  glk = ifp_glk_get_interface ();
  glk->glkunix_stream_open_pathname = override_glkunix_stream_open_pathname;

  override_main (noted_argc, noted_argv);
  glk_exit ();
}
