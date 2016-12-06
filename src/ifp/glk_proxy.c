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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Function prototype, present only to silence compiler warnings. */
strid_t gli_stream_open_pathname (char *pathname, int textmode, glui32 rock);

/* Pointer to our interface table. */
static ifp_glk_interfaceref_t glk_interface = NULL;


/*
 * ifpi_attach_glk_interface()
 *
 * Attach a Glk interface to the shared object.  This function is called
 * on loading the plugin to inform it of how its Glk requests are going to
 * happen.  It needs to be called before any Glk proxy function will work.
 *
 * It may be re-called to redefine the interface.  The function keeps the
 * address it is passed, so the data passed in must not be in automatic
 * (stack) storage - pass static data for safety.
 *
 * The function returns TRUE if the interface is usable (that is, has
 * the expected version number), otherwise FALSE.
 */
int
ifpi_attach_glk_interface (ifp_glk_interfaceref_t interface)
{
  if (interface && interface->version != IFP_GLK_VERSION)
    return FALSE;

  glk_interface = interface;
  return TRUE;
}


/*
 * ifpi_retrieve_glk_interface()
 *
 * Return the current attached Glk interface.  The function returns NULL
 * if no Glk interface is attached.
 */
ifp_glk_interfaceref_t
ifpi_retrieve_glk_interface (void)
{
  return glk_interface;
}


/*
 * ifp_check_interface()
 *
 * Ensure there is a Glk interface to work with.  If we somehow try to make
 * a Glk call without one, it *might* be possible to recover.  However, it's
 * not easy, and not really worthwhile, since this is a plugin usage error.
 * So, we'll just dump a message to stderr, and exit.  We have to exit with
 * raise(SIGABRT) or _exit(), because glk_exit() won't work under these
 * conditions (it's NULL, and that fact might actually have got us to here
 * in any case), and exit() is remapped to a call through the libc interface
 * back to the main program.
 */
static void
ifp_check_interface (void)
{
  if (!glk_interface)
    {
      fprintf (stderr,
               "IFP plugin library error: "
               "glk_proxy: no usable Glk interface\n");
      raise (SIGABRT);
      _exit (1);
    }
}


/**
 * glk_*()
 *
 * Glk proxy functions.  These functions handle calls from the interpreter
 * itself, communicating them to the outside world through the Glk interface
 * passed in when attaching the plugin.  They need to be global, but not
 * necessarily visible outside the library.
 *
 * Calls to Glk functions from within a plugin bind to these definitions,
 * and when called, these proxies forward the actual call through the Glk
 * interface to the real display functions.
 */
void
glk_exit (void)
{
  ifp_check_interface ();
  glk_interface->glk_exit ();
}

void
glk_set_interrupt_handler (void (*func) (void))
{
  ifp_check_interface ();
  glk_interface->glk_set_interrupt_handler (func);
}

void
glk_tick (void)
{
  ifp_check_interface ();
  glk_interface->glk_tick ();
}

glui32
glk_gestalt (glui32 sel, glui32 val)
{
  ifp_check_interface ();
  return glk_interface->glk_gestalt (sel, val);
}

glui32
glk_gestalt_ext (glui32 sel, glui32 val, glui32 *arr, glui32 arrlen)
{
  ifp_check_interface ();
  return glk_interface->glk_gestalt_ext (sel, val, arr, arrlen);
}
unsigned char
glk_char_to_lower (unsigned char ch)
{
  ifp_check_interface ();
  return glk_interface->glk_char_to_lower (ch);
}
unsigned char
glk_char_to_upper (unsigned char ch)
{
  ifp_check_interface ();
  return glk_interface->glk_char_to_upper (ch);
}

winid_t
glk_window_get_root (void)
{
  ifp_check_interface ();
  return glk_interface->glk_window_get_root ();
}

winid_t
glk_window_open (winid_t split, glui32 method, glui32 size,
                 glui32 wintype, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_window_open (split, method, size, wintype, rock);
}

void
glk_window_close (winid_t win, stream_result_t *result)
{
  ifp_check_interface ();
  glk_interface->glk_window_close (win, result);
}

void
glk_window_get_size (winid_t win, glui32 *widthptr, glui32 *heightptr)
{
  ifp_check_interface ();
  glk_interface->glk_window_get_size (win, widthptr, heightptr);
}

void
glk_window_set_arrangement (winid_t win, glui32 method,
                            glui32 size, winid_t keywin)
{
  ifp_check_interface ();
  glk_interface->glk_window_set_arrangement (win, method, size, keywin);
}

void
glk_window_get_arrangement (winid_t win, glui32 *methodptr,
                            glui32 *sizeptr, winid_t *keywinptr)
{
  ifp_check_interface ();
  glk_interface->glk_window_get_arrangement (win, methodptr, sizeptr,
                                             keywinptr);
}

winid_t
glk_window_iterate (winid_t win, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface->glk_window_iterate (win, rockptr);
}

glui32
glk_window_get_rock (winid_t win)
{
  ifp_check_interface ();
  return glk_interface->glk_window_get_rock (win);
}

glui32
glk_window_get_type (winid_t win)
{
  ifp_check_interface ();
  return glk_interface->glk_window_get_type (win);
}

winid_t
glk_window_get_parent (winid_t win)
{
  ifp_check_interface ();
  return glk_interface->glk_window_get_parent (win);
}

winid_t
glk_window_get_sibling (winid_t win)
{
  ifp_check_interface ();
  return glk_interface->glk_window_get_sibling (win);
}

void
glk_window_clear (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_window_clear (win);
}

void
glk_window_move_cursor (winid_t win, glui32 xpos, glui32 ypos)
{
  ifp_check_interface ();
  glk_interface->glk_window_move_cursor (win, xpos, ypos);
}

strid_t
glk_window_get_stream (winid_t win)
{
  ifp_check_interface ();
  return glk_interface->glk_window_get_stream (win);
}

void
glk_window_set_echo_stream (winid_t win, strid_t str)
{
  ifp_check_interface ();
  glk_interface->glk_window_set_echo_stream (win, str);
}

strid_t
glk_window_get_echo_stream (winid_t win)
{
  ifp_check_interface ();
  return glk_interface->glk_window_get_echo_stream (win);
}

void
glk_set_window (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_set_window (win);
}

strid_t
glk_stream_open_file (frefid_t fileref, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_open_file (fileref, fmode, rock);
}

strid_t
glk_stream_open_memory (char *buf, glui32 buflen, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_open_memory (buf, buflen, fmode, rock);
}

void
glk_stream_close (strid_t str, stream_result_t *result)
{
  ifp_check_interface ();
  glk_interface->glk_stream_close (str, result);
}

strid_t
glk_stream_iterate (strid_t str, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_iterate (str, rockptr);
}

glui32
glk_stream_get_rock (strid_t str)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_get_rock (str);
}

void
glk_stream_set_position (strid_t str, glsi32 pos, glui32 seekmode)
{
  ifp_check_interface ();
  glk_interface->glk_stream_set_position (str, pos, seekmode);
}

glui32
glk_stream_get_position (strid_t str)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_get_position (str);
}

void
glk_stream_set_current (strid_t str)
{
  ifp_check_interface ();
  glk_interface->glk_stream_set_current (str);
}

strid_t
glk_stream_get_current (void)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_get_current ();
}

void
glk_put_char (unsigned char ch)
{
  ifp_check_interface ();
  glk_interface->glk_put_char (ch);
}

void
glk_put_char_stream (strid_t str, unsigned char ch)
{
  ifp_check_interface ();
  glk_interface->glk_put_char_stream (str, ch);
}

void
glk_put_string (char *s)
{
  ifp_check_interface ();
  glk_interface->glk_put_string (s);
}

void
glk_put_string_stream (strid_t str, char *s)
{
  ifp_check_interface ();
  glk_interface->glk_put_string_stream (str, s);
}

void
glk_put_buffer (char *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface->glk_put_buffer (buf, len);
}

void
glk_put_buffer_stream (strid_t str, char *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface->glk_put_buffer_stream (str, buf, len);
}

void
glk_set_style (glui32 styl)
{
  ifp_check_interface ();
  glk_interface->glk_set_style (styl);
}

void
glk_set_style_stream (strid_t str, glui32 styl)
{
  ifp_check_interface ();
  glk_interface->glk_set_style_stream (str, styl);
}

glsi32
glk_get_char_stream (strid_t str)
{
  ifp_check_interface ();
  return glk_interface->glk_get_char_stream (str);
}

glui32
glk_get_line_stream (strid_t str, char *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface->glk_get_line_stream (str, buf, len);
}

glui32
glk_get_buffer_stream (strid_t str, char *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface->glk_get_buffer_stream (str, buf, len);
}

void
glk_stylehint_set (glui32 wintype, glui32 styl, glui32 hint, glsi32 val)
{
  ifp_check_interface ();
  glk_interface->glk_stylehint_set (wintype, styl, hint, val);
}

void
glk_stylehint_clear (glui32 wintype, glui32 styl, glui32 hint)
{
  ifp_check_interface ();
  glk_interface->glk_stylehint_clear (wintype, styl, hint);
}

glui32
glk_style_distinguish (winid_t win, glui32 styl1, glui32 styl2)
{
  ifp_check_interface ();
  return glk_interface->glk_style_distinguish (win, styl1, styl2);
}

glui32
glk_style_measure (winid_t win, glui32 styl, glui32 hint, glui32 *result)
{
  ifp_check_interface ();
  return glk_interface->glk_style_measure (win, styl, hint, result);
}

frefid_t
glk_fileref_create_temp (glui32 usage, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_fileref_create_temp (usage, rock);
}

frefid_t
glk_fileref_create_by_name (glui32 usage, char *name, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_fileref_create_by_name (usage, name, rock);
}

frefid_t
glk_fileref_create_by_prompt (glui32 usage, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_fileref_create_by_prompt (usage, fmode, rock);
}

frefid_t
glk_fileref_create_from_fileref (glui32 usage, frefid_t fref, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_fileref_create_from_fileref (usage, fref, rock);
}

void
glk_fileref_destroy (frefid_t fref)
{
  ifp_check_interface ();
  glk_interface->glk_fileref_destroy (fref);
}

frefid_t
glk_fileref_iterate (frefid_t fref, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface->glk_fileref_iterate (fref, rockptr);
}

glui32
glk_fileref_get_rock (frefid_t fref)
{
  ifp_check_interface ();
  return glk_interface->glk_fileref_get_rock (fref);
}

void
glk_fileref_delete_file (frefid_t fref)
{
  ifp_check_interface ();
  glk_interface->glk_fileref_delete_file (fref);
}

glui32
glk_fileref_does_file_exist (frefid_t fref)
{
  ifp_check_interface ();
  return glk_interface->glk_fileref_does_file_exist (fref);
}

void
glk_select (event_t *event)
{
  ifp_check_interface ();
  glk_interface->glk_select (event);
}

void
glk_select_poll (event_t *event)
{
  ifp_check_interface ();
  glk_interface->glk_select_poll (event);
}

void
glk_request_timer_events (glui32 millisecs)
{
  ifp_check_interface ();
  glk_interface->glk_request_timer_events (millisecs);
}

void
glk_request_line_event (winid_t win, char *buf, glui32 maxlen, glui32 initlen)
{
  ifp_check_interface ();
  glk_interface->glk_request_line_event (win, buf, maxlen, initlen);
}

void
glk_request_char_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_request_char_event (win);
}

void
glk_request_mouse_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_request_mouse_event (win);
}

void
glk_cancel_line_event (winid_t win, event_t *event)
{
  ifp_check_interface ();
  glk_interface->glk_cancel_line_event (win, event);
}

void
glk_cancel_char_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_cancel_char_event (win);
}

void
glk_cancel_mouse_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_cancel_mouse_event (win);
}

glui32
glk_buffer_to_lower_case_uni (glui32 *buf, glui32 len, glui32 numchars)
{
  ifp_check_interface ();
  return glk_interface->glk_buffer_to_lower_case_uni (buf, len, numchars);
}

glui32
glk_buffer_to_upper_case_uni (glui32 *buf, glui32 len, glui32 numchars)
{
  ifp_check_interface ();
  return glk_interface->glk_buffer_to_upper_case_uni (buf, len, numchars);
}

glui32
glk_buffer_to_title_case_uni (glui32 *buf, glui32 len,
                              glui32 numchars, glui32 lowerrest)
{
  ifp_check_interface ();
  return glk_interface->glk_buffer_to_title_case_uni (buf, len,
                                                      numchars, lowerrest);
}

void
glk_put_char_uni (glui32 ch)
{
  ifp_check_interface ();
  glk_interface->glk_put_char_uni (ch);
}

void
glk_put_string_uni (glui32 *s)
{
  ifp_check_interface ();
  glk_interface->glk_put_string_uni (s);
}

void
glk_put_buffer_uni (glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface->glk_put_buffer_uni (buf, len);
}

void
glk_put_char_stream_uni (strid_t str, glui32 ch)
{
  ifp_check_interface ();
  glk_interface->glk_put_char_stream_uni (str, ch);
}

void
glk_put_string_stream_uni (strid_t str, glui32 *s)
{
  ifp_check_interface ();
  glk_interface->glk_put_string_stream_uni (str, s);
}

void
glk_put_buffer_stream_uni (strid_t str, glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface->glk_put_buffer_stream_uni (str, buf, len);
}

glsi32
glk_get_char_stream_uni (strid_t str)
{
  ifp_check_interface ();
  return glk_interface->glk_get_char_stream_uni (str);
}

glui32
glk_get_buffer_stream_uni (strid_t str, glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface->glk_get_buffer_stream_uni (str, buf, len);
}

glui32
glk_get_line_stream_uni (strid_t str, glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface->glk_get_line_stream_uni (str, buf, len);
}

strid_t
glk_stream_open_file_uni (frefid_t fileref, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_open_file_uni (fileref, fmode, rock);
}

strid_t
glk_stream_open_memory_uni (glui32 *buf, glui32 buflen,
                            glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_stream_open_memory_uni (buf, buflen, fmode, rock);
}

void
glk_request_char_event_uni (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_request_char_event_uni (win);
}

void
glk_request_line_event_uni (winid_t win, glui32 *buf,
                            glui32 maxlen, glui32 initlen)
{
  ifp_check_interface ();
  glk_interface->glk_request_line_event_uni (win, buf, maxlen, initlen);
}

glui32
glk_image_draw (winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
  ifp_check_interface ();
  return glk_interface->glk_image_draw (win, image, val1, val2);
}

glui32
glk_image_draw_scaled (winid_t win, glui32 image,
                       glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
  ifp_check_interface ();
  return glk_interface->glk_image_draw_scaled (win, image, val1, val2,
                                               width, height);
}

glui32
glk_image_get_info (glui32 image, glui32 *width, glui32 *height)
{
  ifp_check_interface ();
  return glk_interface->glk_image_get_info (image, width, height);
}

void
glk_window_flow_break (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_window_flow_break (win);
}

void
glk_window_erase_rect (winid_t win,
                       glsi32 left, glsi32 top, glui32 width, glui32 height)
{
  ifp_check_interface ();
  glk_interface->glk_window_erase_rect (win, left, top, width, height);
}

void
glk_window_fill_rect (winid_t win, glui32 color,
                      glsi32 left, glsi32 top, glui32 width, glui32 height)
{
  ifp_check_interface ();
  glk_interface->glk_window_fill_rect (win, color, left, top, width, height);
}

void
glk_window_set_background_color (winid_t win, glui32 color)
{
  ifp_check_interface ();
  glk_interface->glk_window_set_background_color (win, color);
}

schanid_t
glk_schannel_create (glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glk_schannel_create (rock);
}

void
glk_schannel_destroy (schanid_t chan)
{
  ifp_check_interface ();
  glk_interface->glk_schannel_destroy (chan);
}

schanid_t
glk_schannel_iterate (schanid_t chan, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface->glk_schannel_iterate (chan, rockptr);
}

glui32
glk_schannel_get_rock (schanid_t chan)
{
  ifp_check_interface ();
  return glk_interface->glk_schannel_get_rock (chan);
}

glui32
glk_schannel_play (schanid_t chan, glui32 snd)
{
  ifp_check_interface ();
  return glk_interface->glk_schannel_play (chan, snd);
}

glui32
glk_schannel_play_ext (schanid_t chan, glui32 snd, glui32 repeats,
                       glui32 notify)
{
  ifp_check_interface ();
  return glk_interface->glk_schannel_play_ext (chan, snd, repeats, notify);
}

void
glk_schannel_stop (schanid_t chan)
{
  ifp_check_interface ();
  glk_interface->glk_schannel_stop (chan);
}

void
glk_schannel_set_volume (schanid_t chan, glui32 vol)
{
  ifp_check_interface ();
  glk_interface->glk_schannel_set_volume (chan, vol);
}

void
glk_sound_load_hint (glui32 snd, glui32 flag)
{
  ifp_check_interface ();
  glk_interface->glk_sound_load_hint (snd, flag);
}

void
glk_set_hyperlink (glui32 linkval)
{
  ifp_check_interface ();
  glk_interface->glk_set_hyperlink (linkval);
}

void
glk_set_hyperlink_stream (strid_t str, glui32 linkval)
{
  ifp_check_interface ();
  glk_interface->glk_set_hyperlink_stream (str, linkval);
}

void
glk_request_hyperlink_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_request_hyperlink_event (win);
}

void
glk_cancel_hyperlink_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface->glk_cancel_hyperlink_event (win);
}

void
glkunix_set_base_file (char *filename)
{
  ifp_check_interface ();
  glk_interface->glkunix_set_base_file (filename);
}

strid_t
glkunix_stream_open_pathname (char *pathname, glui32 textmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->glkunix_stream_open_pathname (pathname,
                                                      textmode, rock);
}

void
gidispatch_set_object_registry (gidispatch_rock_t (*regi)
                                (void *obj, glui32 objclass),
                                void (*unregi) (void *obj, glui32 objclass,
                                                gidispatch_rock_t objrock))
{
  ifp_check_interface ();
  glk_interface->gidispatch_set_object_registry (regi, unregi);
}

gidispatch_rock_t
gidispatch_get_objrock (void *obj, glui32 objclass)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_get_objrock (obj, objclass);
}

void
gidispatch_set_retained_registry (gidispatch_rock_t (*regi)
                                  (void *array, glui32 len, char *typecode),
                                  void (*unregi) (void *array, glui32 len,
                                                  char *typecode,
                                                  gidispatch_rock_t objrock))
{
  ifp_check_interface ();
  glk_interface->gidispatch_set_retained_registry (regi, unregi);
}

void
gidispatch_call (glui32 funcnum, glui32 numargs, gluniversal_t *arglist)
{
  ifp_check_interface ();
  glk_interface->gidispatch_call (funcnum, numargs, arglist);
}

char *
gidispatch_prototype (glui32 funcnum)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_prototype (funcnum);
}

glui32
gidispatch_count_classes (void)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_count_classes ();
}

glui32
gidispatch_count_intconst (void)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_count_classes ();
}

gidispatch_intconst_t *
gidispatch_get_intconst (glui32 index_)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_get_intconst (index_);
}

glui32
gidispatch_count_functions (void)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_count_functions ();
}

gidispatch_function_t *
gidispatch_get_function (glui32 index_)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_get_function (index_);
}

gidispatch_function_t *
gidispatch_get_function_by_id (glui32 id)
{
  ifp_check_interface ();
  return glk_interface->gidispatch_get_function_by_id (id);
}

giblorb_err_t
giblorb_create_map (strid_t file, giblorb_map_t **newmap)
{
  ifp_check_interface ();
  return glk_interface->giblorb_create_map (file, newmap);
}

giblorb_err_t
giblorb_destroy_map (giblorb_map_t *map)
{
  ifp_check_interface ();
  return glk_interface->giblorb_destroy_map (map);
}

giblorb_err_t
giblorb_load_chunk_by_type (giblorb_map_t *map,
                            glui32 method, giblorb_result_t *res,
                            glui32 chunktype, glui32 count)
{
  ifp_check_interface ();
  return glk_interface->giblorb_load_chunk_by_type (map, method,
                                                    res, chunktype, count);
}

giblorb_err_t
giblorb_load_chunk_by_number (giblorb_map_t *map,
                              glui32 method, giblorb_result_t *res,
                              glui32 chunknum)
{
  ifp_check_interface ();
  return glk_interface->giblorb_load_chunk_by_number (map, method,
                                                      res, chunknum);
}

giblorb_err_t
giblorb_unload_chunk (giblorb_map_t *map, glui32 chunknum)
{
  ifp_check_interface ();
  return glk_interface->giblorb_unload_chunk (map, chunknum);
}

giblorb_err_t
giblorb_load_resource (giblorb_map_t *map,
                       glui32 method, giblorb_result_t *res,
                       glui32 usage, glui32 resnum)
{
  ifp_check_interface ();
  return glk_interface->giblorb_load_resource (map, method, res, usage, resnum);
}

giblorb_err_t
giblorb_count_resources (giblorb_map_t *map,
                         glui32 usage, glui32 *num, glui32 *min,
                         glui32 *max)
{
  ifp_check_interface ();
  return glk_interface->giblorb_count_resources (map, usage, num, min, max);
}

giblorb_err_t
giblorb_set_resource_map (strid_t file)
{
  ifp_check_interface ();
  return glk_interface->giblorb_set_resource_map (file);
}

giblorb_map_t *
giblorb_get_resource_map (void)
{
  ifp_check_interface ();
  return glk_interface->giblorb_get_resource_map ();
}


/*
 * Internal Glk functions.  We need to have these proxied for building
 * chaining plugins, which are both plugin and loader items.
 */
strid_t
gli_stream_open_pathname (char *pathname, int textmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface->gli_stream_open_pathname (pathname, textmode, rock);
}
