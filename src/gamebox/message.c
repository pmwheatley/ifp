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
 * USA
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "protos.h"


/*
 * The message and input window will go below the main window.  Messaging
 * functions use the main window if no lower input window is possible.
 */
static winid_t message_window = NULL;

/* Timeout messages in 5 seconds (100mS * 50). */
static const int TIMEOUT_BASELINE = 100, MESSAGE_TIMEOUT = 50;


/*
 * message_begin_dialog()
 * message_end_dialog()
 * message_read_line()
 * message_confirm()
 * message_wait_for_keypress()
 * message_write_line()
 *
 * Helper functions to implement a single-line message and prompted input
 * window at the bottom of the frame.  If the Glk in use doesn't permit
 * windows, these functions degrade to use the main window instead.  The
 * message print can either return immediately, or wait until keypress or
 * about five seconds have elapsed.
 */
void
message_begin_dialog (void)
{
  if (!message_window)
    {
      winid_t main_window;

      /*
       * Try to identify the current main window automatically, to avoid
       * us having to be told about it.
       */
      main_window = glk_window_get_root ();
      if (!main_window)
        {
          main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
          if (!main_window)
            {
              fprintf (stderr, "GLK INTERNAL ERROR: can't open main window.\n");
              abort ();
            }
        }

      /* Try for a message window at the base of the main window. */
      message_window = glk_window_open (main_window,
                                        winmethod_Below|winmethod_Fixed,
                                        1, wintype_TextGrid, 0);
    }

  if (message_window)
    glk_window_clear (message_window);
}

void
message_end_dialog (void)
{
  if (message_window)
    {
      glk_window_close (message_window, NULL);
      message_window = NULL;
    }
}

void
message_read_line (const char *prompt, char *buffer, int length)
{
  winid_t window;
  strid_t stream;
  event_t event;
  assert (prompt && buffer);

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
      if (event.type == evtype_Arrange || event.type == evtype_Redraw)
        {
          event_t partial;

          display_handle_redraw ();

          glk_cancel_line_event (window, &partial);

          if (message_window)
            glk_window_clear (message_window);
          else
            glk_put_char_stream (stream, '\n');
          glk_c_put_string_stream (stream, prompt);

          glk_request_line_event (window, buffer, length - 1, partial.val1);
        }
    }
  while (event.type != evtype_LineInput);

  buffer[event.val1] = '\0';

  /* Trim leading and trailing whitespace. */
  memmove (buffer, buffer + strspn (buffer, " \t"),
           sizeof (buffer) - strspn (buffer, " \t"));
  buffer[strcspn (buffer, " \t")] = '\0';

  message_end_dialog ();
}

int
message_confirm (const char *prompt)
{
  winid_t window;
  strid_t stream;
  event_t event;
  char response;
  assert (prompt);

  message_begin_dialog ();

  window = message_window ? message_window : glk_window_get_root ();
  stream = glk_window_get_stream (window);

  if (!message_window)
    glk_put_char_stream (stream, '\n');
  glk_c_put_string_stream (stream, prompt);

  do
    {
      glk_request_char_event (window);

      do
        {
          glk_select (&event);
          if (event.type == evtype_Arrange || event.type == evtype_Redraw)
            {
              display_handle_redraw ();

              glk_cancel_char_event (window);

              if (message_window)
                glk_window_clear (message_window);
              else
                glk_put_char_stream (stream, '\n');
              glk_c_put_string_stream (stream, prompt);

              glk_request_char_event (window);
            }
        }
      while (event.type != evtype_CharInput);

      response = glk_char_to_lower (event.val1);
    }
  while (!(response == 'y' || response == 'n'));

  message_end_dialog ();

  return response == 'y';
}

void
message_wait_for_keypress (void)
{
  winid_t window;
  strid_t stream;
  event_t event;

  message_begin_dialog ();

  window = message_window ? message_window : glk_window_get_root ();
  stream = glk_window_get_stream (window);

  if (message_window)
    glk_c_put_string_stream (stream, "Press any key to continue...");
  else
    glk_c_put_string_stream (stream, "\n<Press Return to continue...>");

  glk_request_char_event (window);

  do
    {
      glk_select (&event);
      if (event.type == evtype_Arrange || event.type == evtype_Redraw)
        {
          display_handle_redraw ();

          glk_cancel_char_event (window);

          if (message_window)
            {
              glk_window_clear (message_window);
              glk_c_put_string_stream (stream, "Press any key to continue...");
            }
          else
            glk_c_put_string_stream (stream, "\n<Press Return to continue...>");

          glk_request_char_event (window);
        }
    }
  while (event.type != evtype_CharInput);

  message_end_dialog ();
}

void
message_write_line (int return_immediately, const char *format, ...)
{
  winid_t window;
  strid_t stream;
  event_t event;
  int timeouts = 0;
  va_list ap;
  char message[1024];
  assert (format);

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
          display_handle_redraw ();

          if (message_window)
            glk_window_clear (message_window);
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

  message_end_dialog ();
}


/*
 * message_windows_closed()
 *
 * Clear the record of windows closed externally (IFP garbage collection).
 */
void
message_windows_closed (void)
{
  /* Safe if an external agency already closed these windows. */
  message_window = NULL;
}
