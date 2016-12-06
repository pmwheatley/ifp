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

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Glk interface vectors.  There are two of these.  The first is private,
 * used by the redirector functions below to implement the real Glk interface
 * to the loaded Glk library.  The second is created for the express purpose
 * of being passed to a loaded plugin library as part of its initialization.
 *
 * The public one is a default Glk interface; clients may override functions
 * in it if required (for example, glk_exit), and that's why the private one
 * exists.  There is only one public interface, and it's static, so everyone
 * sees all changes made to it.
 *
 * On loading a Glk library, both are initialized to identical contents by
 * symbol lookups on the DSO containing the Glk implementation.
 */
static struct ifp_glk_interface glk_interface,
                                public_glk_interface;

/* Handle to the DSO supplying the Glk interface, NULL if none loaded. */
static void *ifp_glk_handle = NULL;

/* Cached address of the main() function in the Glk library. */
static void *ifp_glk_so_main = NULL;

/*
 * Tables mapping Glk symbol names to their locations in the Glk interface.
 * These tables are read by the loader, which looks up each symbol and then
 * stores it in the given location in the Glk interface.  The use of void*
 * is a bit of a type-safety issue, but as there's no real concept of type-
 * safety in DSO symbol lookup anyway, it's not really a problem here.
 */
typedef struct {
  const char *name;
  const void *function_addr;
} ifp_glk_binding_t;

/*
 * Core Glk functions, expected to be present in every Glk library.  If a
 * DSO lacks any of these, we'll refuse to load it.
 */
static const ifp_glk_binding_t CORE_GLK_FUNCTIONS[] = {

  /* Base Glk functions. */
  { "glk_exit", &glk_interface.glk_exit },
  { "glk_set_interrupt_handler", &glk_interface.glk_set_interrupt_handler },
  { "glk_tick", &glk_interface.glk_tick },

  { "glk_gestalt", &glk_interface.glk_gestalt },
  { "glk_gestalt_ext", &glk_interface.glk_gestalt_ext },

  { "glk_char_to_lower", &glk_interface.glk_char_to_lower },
  { "glk_char_to_upper", &glk_interface.glk_char_to_upper },

  { "glk_window_get_root", &glk_interface.glk_window_get_root },
  { "glk_window_open", &glk_interface.glk_window_open },
  { "glk_window_close", &glk_interface.glk_window_close },
  { "glk_window_get_size", &glk_interface.glk_window_get_size },
  { "glk_window_set_arrangement", &glk_interface.glk_window_set_arrangement },
  { "glk_window_get_arrangement", &glk_interface.glk_window_get_arrangement },
  { "glk_window_iterate", &glk_interface.glk_window_iterate },
  { "glk_window_get_rock", &glk_interface.glk_window_get_rock },
  { "glk_window_get_type", &glk_interface.glk_window_get_type },
  { "glk_window_get_parent", &glk_interface.glk_window_get_parent },
  { "glk_window_get_sibling", &glk_interface.glk_window_get_sibling },
  { "glk_window_clear", &glk_interface.glk_window_clear },
  { "glk_window_move_cursor", &glk_interface.glk_window_move_cursor },

  { "glk_window_get_stream", &glk_interface.glk_window_get_stream },
  { "glk_window_set_echo_stream", &glk_interface.glk_window_set_echo_stream },
  { "glk_window_get_echo_stream", &glk_interface.glk_window_get_echo_stream },
  { "glk_set_window", &glk_interface.glk_set_window },

  { "glk_stream_open_file", &glk_interface.glk_stream_open_file },
  { "glk_stream_open_memory", &glk_interface.glk_stream_open_memory },
  { "glk_stream_close", &glk_interface.glk_stream_close },
  { "glk_stream_iterate", &glk_interface.glk_stream_iterate },
  { "glk_stream_get_rock", &glk_interface.glk_stream_get_rock },
  { "glk_stream_set_position", &glk_interface.glk_stream_set_position },
  { "glk_stream_get_position", &glk_interface.glk_stream_get_position },
  { "glk_stream_set_current", &glk_interface.glk_stream_set_current },
  { "glk_stream_get_current", &glk_interface.glk_stream_get_current },

  { "glk_put_char", &glk_interface.glk_put_char },
  { "glk_put_char_stream", &glk_interface.glk_put_char_stream },
  { "glk_put_string", &glk_interface.glk_put_string },
  { "glk_put_string_stream", &glk_interface.glk_put_string_stream },
  { "glk_put_buffer", &glk_interface.glk_put_buffer },
  { "glk_put_buffer_stream", &glk_interface.glk_put_buffer_stream },
  { "glk_set_style", &glk_interface.glk_set_style },
  { "glk_set_style_stream", &glk_interface.glk_set_style_stream },

  { "glk_get_char_stream", &glk_interface.glk_get_char_stream },
  { "glk_get_line_stream", &glk_interface.glk_get_line_stream },
  { "glk_get_buffer_stream", &glk_interface.glk_get_buffer_stream },

  { "glk_stylehint_set", &glk_interface.glk_stylehint_set },
  { "glk_stylehint_clear", &glk_interface.glk_stylehint_clear },
  { "glk_style_distinguish", &glk_interface.glk_style_distinguish },
  { "glk_style_measure", &glk_interface.glk_style_measure },

  { "glk_fileref_create_temp", &glk_interface.glk_fileref_create_temp },
  { "glk_fileref_create_by_name", &glk_interface.glk_fileref_create_by_name },
  { "glk_fileref_create_by_prompt",
      &glk_interface.glk_fileref_create_by_prompt },
  { "glk_fileref_create_from_fileref",
      &glk_interface.glk_fileref_create_from_fileref },
  { "glk_fileref_destroy", &glk_interface.glk_fileref_destroy },
  { "glk_fileref_iterate", &glk_interface.glk_fileref_iterate },
  { "glk_fileref_get_rock", &glk_interface.glk_fileref_get_rock },
  { "glk_fileref_delete_file", &glk_interface.glk_fileref_delete_file },
  { "glk_fileref_does_file_exist", &glk_interface.glk_fileref_does_file_exist },

  { "glk_select", &glk_interface.glk_select },
  { "glk_select_poll", &glk_interface.glk_select_poll },

  { "glk_request_timer_events", &glk_interface.glk_request_timer_events },

  { "glk_request_line_event", &glk_interface.glk_request_line_event },
  { "glk_request_char_event", &glk_interface.glk_request_char_event },
  { "glk_request_mouse_event", &glk_interface.glk_request_mouse_event },

  { "glk_cancel_line_event", &glk_interface.glk_cancel_line_event },
  { "glk_cancel_char_event", &glk_interface.glk_cancel_char_event },
  { "glk_cancel_mouse_event", &glk_interface.glk_cancel_mouse_event },

  { "glkunix_set_base_file", &glk_interface.glkunix_set_base_file },
  { "glkunix_stream_open_pathname",
      &glk_interface.glkunix_stream_open_pathname },

  /* Dispatch-layer Glk functions. */
  { "gidispatch_set_object_registry",
      &glk_interface.gidispatch_set_object_registry },
  { "gidispatch_get_objrock", &glk_interface.gidispatch_get_objrock },
  { "gidispatch_set_retained_registry",
      &glk_interface.gidispatch_set_retained_registry },

  { "gidispatch_call", &glk_interface.gidispatch_call },
  { "gidispatch_prototype", &glk_interface.gidispatch_prototype },
  { "gidispatch_count_classes", &glk_interface.gidispatch_count_classes },
  { "gidispatch_count_intconst", &glk_interface.gidispatch_count_intconst },
  { "gidispatch_get_intconst", &glk_interface.gidispatch_get_intconst },
  { "gidispatch_count_functions", &glk_interface.gidispatch_count_functions },
  { "gidispatch_get_function", &glk_interface.gidispatch_get_function },
  { "gidispatch_get_function_by_id",
      &glk_interface.gidispatch_get_function_by_id },

  /* Blorb-layer Glk functions. */
  { "giblorb_create_map", &glk_interface.giblorb_create_map },
  { "giblorb_destroy_map", &glk_interface.giblorb_destroy_map },

  { "giblorb_load_chunk_by_type", &glk_interface.giblorb_load_chunk_by_type },
  { "giblorb_load_chunk_by_number",
      &glk_interface.giblorb_load_chunk_by_number },
  { "giblorb_unload_chunk", &glk_interface.giblorb_unload_chunk },

  { "giblorb_load_resource", &glk_interface.giblorb_load_resource },
  { "giblorb_count_resources", &glk_interface.giblorb_count_resources },

  { "giblorb_set_resource_map", &glk_interface.giblorb_set_resource_map },
  { "giblorb_get_resource_map", &glk_interface.giblorb_get_resource_map },

  /*
   * Glk internal functions intentionally omitted -- see below.
   * { "gli_stream_open_pathname", &glk_interface.gli_stream_open_pathname },
   */

  { NULL, NULL }
};

/*
 * Extended Glk functions.  A Glk library may choose not to implement these,
 * in which case their functions won't be in the loaded DSO.  A caller should
 * check with glk_gestalt() before trying to call one.  If absent, then,
 * these functions are redirected to local error handling stub.
 */
static const ifp_glk_binding_t EXTENDED_GLK_FUNCTIONS[] = {

  { "glk_buffer_to_lower_case_uni",
      &glk_interface.glk_buffer_to_lower_case_uni },
  { "glk_buffer_to_upper_case_uni",
      &glk_interface.glk_buffer_to_upper_case_uni },
  { "glk_buffer_to_title_case_uni",
      &glk_interface.glk_buffer_to_title_case_uni },

  { "glk_put_char_uni", &glk_interface.glk_put_char_uni },
  { "glk_put_string_uni", &glk_interface.glk_put_string_uni },
  { "glk_put_buffer_uni", &glk_interface.glk_put_buffer_uni },
  { "glk_put_char_stream_uni", &glk_interface.glk_put_char_stream_uni },
  { "glk_put_string_stream_uni", &glk_interface.glk_put_string_stream_uni },
  { "glk_put_buffer_stream_uni", &glk_interface.glk_put_buffer_stream_uni },

  { "glk_get_char_stream_uni", &glk_interface.glk_get_char_stream_uni },
  { "glk_get_buffer_stream_uni", &glk_interface.glk_get_buffer_stream_uni },
  { "glk_get_line_stream_uni", &glk_interface.glk_get_line_stream_uni },

  { "glk_stream_open_file_uni", &glk_interface.glk_stream_open_file_uni },
  { "glk_stream_open_memory_uni", &glk_interface.glk_stream_open_memory_uni },

  { "glk_request_char_event_uni", &glk_interface.glk_request_char_event_uni },
  { "glk_request_line_event_uni", &glk_interface.glk_request_line_event_uni },

  { "glk_image_draw", &glk_interface.glk_image_draw },
  { "glk_image_draw_scaled", &glk_interface.glk_image_draw_scaled },
  { "glk_image_get_info", &glk_interface.glk_image_get_info },

  { "glk_window_flow_break", &glk_interface.glk_window_flow_break },

  { "glk_window_erase_rect", &glk_interface.glk_window_erase_rect },
  { "glk_window_fill_rect", &glk_interface.glk_window_fill_rect },
  { "glk_window_set_background_color",
      &glk_interface.glk_window_set_background_color },

  { "glk_schannel_create", &glk_interface.glk_schannel_create },
  { "glk_schannel_destroy", &glk_interface.glk_schannel_destroy },
  { "glk_schannel_iterate", &glk_interface.glk_schannel_iterate },
  { "glk_schannel_get_rock", &glk_interface.glk_schannel_get_rock },

  { "glk_schannel_play", &glk_interface.glk_schannel_play },
  { "glk_schannel_play_ext", &glk_interface.glk_schannel_play_ext },
  { "glk_schannel_stop", &glk_interface.glk_schannel_stop },
  { "glk_schannel_set_volume", &glk_interface.glk_schannel_set_volume },

  { "glk_sound_load_hint", &glk_interface.glk_sound_load_hint },

  { "glk_set_hyperlink", &glk_interface.glk_set_hyperlink },
  { "glk_set_hyperlink_stream", &glk_interface.glk_set_hyperlink_stream },
  { "glk_request_hyperlink_event", &glk_interface.glk_request_hyperlink_event },
  { "glk_cancel_hyperlink_event", &glk_interface.glk_cancel_hyperlink_event },

  { NULL, NULL }
};


/*
 * We handle the Glk internal function we want specially, and it's omitted
 * from the table above.  Instead, if the library doesn't offer what we want,
 * we insert this fallback version in its place.
 */
extern strid_t fallback_gli_stream_open_pathname (char *pathname,
                                                  int textmode, glui32 rock);

/* Function used to handle calls to missing Glk functions. */
static int fallback_glk_function_handler (void);

/*
 * Function pointer definition for convenience below.  The table mapping
 * symbol names uses void*; here we introduce a function pointer, though
 * it's still not type-safe (void (*)(void)).
 */
typedef void (*function_ptr_t) (void);


/*
 * ifp_glk_load_interface()
 *
 * Open the given Glk DSO library, and set up the Glk interface vectors with
 * symbols resolved from it.  The function returns TRUE if the DSO library
 * could be loaded, FALSE otherwise.
 *
 * It is an error for this function to be called in a chaining plugin;
 * plugins always receive their Glk interface from the main binary.
 */
int
ifp_glk_load_interface (const char *filename)
{
  void *handle;
  const ifp_glk_binding_t *entry;
  function_ptr_t stream_open_pathname, so_main;
  glui32 version;
  assert (filename);

  ifp_trace ("glkloader: ifp_glk_load_interface <- '%s'", filename);

  if (ifp_self_inside_plugin ())
    ifp_fatal ("glkloader: attempt to load glk inside a chaining plugin");

  if (ifp_glk_handle)
    {
      ifp_error ("glkloader: interface already loaded and initialized");
      return FALSE;
    }

  handle = ifp_dlopen (filename);
  if (!handle)
    {
      ifp_trace ("glkloader: dlopen failed: %s", ifp_dlerror ());
      return FALSE;
    }

  if (ifp_dlsym (handle, "ifpi_attach_stubs_interface") != NULL)
    {
      ifp_trace ("glkloader: glk library appears to be a legacy build");
      ifp_dlclose (handle);
      return FALSE;
    }

  /* Using the table, retrieve core Glk function symbols from the library. */
  ifp_trace ("glkloader: retrieving core glk functions from the library");
  for (entry = CORE_GLK_FUNCTIONS; entry->name; entry++)
    {
      function_ptr_t function_addr;

      function_addr = ifp_dlsym (handle, entry->name);
      if (!function_addr)
        {
          ifp_error ("glkloader: %s: %s", filename, ifp_dlerror ());
          ifp_dlclose (handle);

          memset (&glk_interface, 0, sizeof (glk_interface));
          return FALSE;
        }

      *(function_ptr_t*) (entry->function_addr) = function_addr;
    }

  /* Handle gli_stream_open_pathname() separately; it's permitted to fail. */
  stream_open_pathname = ifp_dlsym (handle, "gli_stream_open_pathname");
  if (stream_open_pathname)
    {
      /* Note: double-shuffle suppresses gcc's warning about void to fptr. */
      void *function_addr = &glk_interface.gli_stream_open_pathname;
      *(function_ptr_t*) function_addr = stream_open_pathname;
    }
  else
    glk_interface.gli_stream_open_pathname = fallback_gli_stream_open_pathname;

  /* Now retrieve extended Glk function symbols from the library. */
  ifp_trace ("glkloader: retrieving extended glk functions from the library");
  for (entry = EXTENDED_GLK_FUNCTIONS; entry->name; entry++)
    {
      function_ptr_t function_addr;

      function_addr = ifp_dlsym (handle, entry->name);
      if (!function_addr)
        function_addr = (function_ptr_t) fallback_glk_function_handler;

      *(function_ptr_t*) (entry->function_addr) = function_addr;
    }

  /* Find the address of "main" in the loaded library. */
  so_main = ifp_dlsym (handle, "main");
  if (!so_main)
    {
      ifp_error ("glkloader: %s: %s", filename, ifp_dlerror ());
      ifp_dlclose (handle);

      memset (&glk_interface, 0, sizeof (glk_interface));
      return FALSE;
    }

  /* Verify that this is a supported Glk version. */
  version = glk_interface.glk_gestalt (gestalt_Version, 0);
  if (!(version == GLK_VERSION_0_6_1 || version == GLK_VERSION_0_7_0))
    {
      ifp_trace ("glkloader: loaded glk version is not supported");
      ifp_dlclose (handle);

      memset (&glk_interface, 0, sizeof (glk_interface));
      return FALSE;
    }

  /* Load successful; set version and copy to the public interface. */
  glk_interface.version = IFP_GLK_VERSION;
  memcpy (&public_glk_interface, &glk_interface, sizeof (glk_interface));

  ifp_glk_handle = handle;
  ifp_glk_so_main = so_main;

  ifp_trace ("glkloader: glk library loaded successfully");
  return TRUE;
}


/*
 * ifp_glk_verify_dso()
 *
 * Test the give Glk DSO library to see if it looks like it should be
 * loadable, reporting all problems found errors.  This function repeats the
 * loading checks of ifp_glk_load_interface() for client code that needs
 * an error-reporting mode.  Returns TRUE unless the DSO can be certain
 * to fail on loading.
 *
 * It is an error for this function to be called in a chaining plugin;
 * plugins always receive their Glk interface from the main binary.
 */
int
ifp_glk_verify_dso (const char *filename)
{
  void *handle;
  glui32 (*glk_gestalt_) (glui32, glui32), version;
  assert (filename);

  ifp_trace ("glkloader: ifp_glk_verify_dso <- '%s'", filename);

  if (ifp_self_inside_plugin ())
    ifp_fatal ("glkloader: attempt to verify glk inside a chaining plugin");

  handle = ifp_dlopen (filename);
  if (!handle)
    {
      ifp_error ("glkloader: dlopen failed: %s", ifp_dlerror ());
      return FALSE;
    }

  if (ifp_dlsym (handle, "ifpi_attach_stubs_interface") != NULL)
    {
      ifp_error ("glkloader: %s:"
                 " old-style Glk plugins are not supported -- sorry", filename);
      ifp_dlclose (handle);
      return FALSE;
    }

  glk_gestalt_ = ifp_dlsym (handle, "glk_gestalt");
  if (!glk_gestalt_)
    {
      ifp_trace ("glkloader: %s: %s", filename, ifp_dlerror ());
      ifp_dlclose (handle);
      return FALSE;
    }

  version = glk_gestalt_ (gestalt_Version, 0);
  if (!(version == GLK_VERSION_0_6_1 || version == GLK_VERSION_0_7_0))
    {
      ifp_error ("glkloader: %s:"
                 " Glk version %lu.%lu.%lu is not supported", filename,
                 version >> 16, (version >> 8) & 0xff, version & 0xff);
      ifp_dlclose (handle);
      return FALSE;
    }

  ifp_dlclose (handle);
  return TRUE;
}


/*
 * ifp_glk_get_main()
 *
 * Return the address of the main() function within the current loaded
 * Glk DSO, or NULL if none loaded.
 *
 * It is an error for this function to be called in a chaining plugin;
 * plugins always receive their Glk interface from the main binary.
 */
void *
ifp_glk_get_main (void)
{
  ifp_trace ("glkloader: ifp_glk_get_main <- void");

  if (ifp_self_inside_plugin ())
    ifp_fatal ("glkloader: attempt to unload glk inside a chaining plugin");

  return ifp_glk_so_main;
}


/*
 * ifp_glk_unload_interface()
 *
 * Close any currently open Glk DSO library, and clear the Glk interface
 * vectors of any symbols.  This function should only be called once all
 * other cleanup is complete; that is, the very last thing before _exit().
 *
 * It is an error for this function to be called in a chaining plugin;
 * plugins always receive their Glk interface from the main binary.
 *
 * Note that many, if not all, Glk "main" functions call exit() rather
 * than return from main(), so in most cases this function is never reached.
 * That's not a problem -- exit() will clean all of this up anyway.
 */
void
ifp_glk_unload_interface (void)
{
  ifp_trace ("glkloader: ifp_glk_unload_interface <- void");

  if (ifp_self_inside_plugin ())
    ifp_fatal ("glkloader: attempt to unload glk inside a chaining plugin");

  if (ifp_glk_handle)
    {
      ifp_trace ("glkloader: unloading the glk library");
      ifp_dlclose (ifp_glk_handle);
      ifp_glk_handle = NULL;

      ifp_glk_so_main = NULL;

      memset (&glk_interface, 0, sizeof (glk_interface));
      memset (&public_glk_interface, 0, sizeof (public_glk_interface));
    }
}


/*
 * ifp_glk_get_interface()
 *
 * Return the public Glk interface vector.  If running inside a chaining
 * plugin, this function will return the Glk interface handed to the chaining
 * plugin on plugin load.  This allows the Glk interface to be passed
 * forwards to chained plugins.  Otherwise, the function returns the main
 * program Glk interface, listing the callable addresses of the real Glk
 * routines.
 */
ifp_glk_interfaceref_t
ifp_glk_get_interface (void)
{
  ifp_trace ("glkloader: ifp_glk_get_interface <- void");

  if (ifp_self_inside_plugin ())
    {
      ifp_trace ("glkloader: returning a previously-attached glk interface");
      return ifp_plugin_retrieve_glk_interface (ifp_self ());
    }
  else
    {
      if (public_glk_interface.version == 0)
        ifp_fatal ("glkloader: no interface loaded or initialized");

      ifp_trace ("glkloader: returning the main glk interface");
      return &public_glk_interface;
    }
}


/*
 * fallback_glk_function_handler()
 *
 * If client code calls an extended Glk function that the loaded Glk library
 * doesn't implement, the call arrives here.  This function tries to carry
 * on running, returning zero as perhaps the best shot at doing no damage.
 * Whether the program can continue, though, may be suspect.
 */
static int
fallback_glk_function_handler (void)
{
  ifp_error ("glkloader: call received for an unimplemented Glk function");
  ifp_notice ("glkloader: trying to continue running anyway...");

  return 0;
}


/*
 * ifp_check_interface()
 *
 * Ensure there is a Glk interface to work with.  We're responsible here for
 * making sure that there's a full Glk library function set loaded before
 * attempting a Glk call.  If we're in a chaining plugin, the private Glk
 * interface won't be set.  If we lack a Glk interface, print a message and
 * abort.
 */
static void
ifp_check_interface (void)
{
  if (glk_interface.version == 0)
    ifp_fatal ("glkloader: no interface loaded or initialized");
}


/*
 * glk_*()
 *
 * Glk redirector functions.  Calls to Glk functions that are not in a
 * plugin bind to these definitions.  When called, these redirectors forward
 * the call to the Glk library.
 *
 * The symbols are defined to be weak so that when built into a chaining
 * plugin these Glk functions are superceded by the Glk proxy.
 */
__attribute__ ((weak))
void
glk_exit (void)
{
  ifp_check_interface ();
  glk_interface.glk_exit ();
}

__attribute__ ((weak))
void
glk_set_interrupt_handler (void (*func) (void))
{
  ifp_check_interface ();
  glk_interface.glk_set_interrupt_handler (func);
}

__attribute__ ((weak))
void
glk_tick (void)
{
  ifp_check_interface ();
  glk_interface.glk_tick ();
}

__attribute__ ((weak))
glui32
glk_gestalt (glui32 sel, glui32 val)
{
  ifp_check_interface ();
  return glk_interface.glk_gestalt (sel, val);
}

__attribute__ ((weak))
glui32
glk_gestalt_ext (glui32 sel, glui32 val, glui32 *arr, glui32 arrlen)
{
  ifp_check_interface ();
  return glk_interface.glk_gestalt_ext (sel, val, arr, arrlen);
}

__attribute__ ((weak))
unsigned char
glk_char_to_lower (unsigned char ch)
{
  ifp_check_interface ();
  return glk_interface.glk_char_to_lower (ch);
}

__attribute__ ((weak))
unsigned char
glk_char_to_upper (unsigned char ch)
{
  ifp_check_interface ();
  return glk_interface.glk_char_to_upper (ch);
}

__attribute__ ((weak))
winid_t
glk_window_get_root (void)
{
  ifp_check_interface ();
  return glk_interface.glk_window_get_root ();
}

__attribute__ ((weak))
winid_t
glk_window_open (winid_t split, glui32 method, glui32 size,
                 glui32 wintype, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_window_open (split, method, size, wintype, rock);
}

__attribute__ ((weak))
void
glk_window_close (winid_t win, stream_result_t *result)
{
  ifp_check_interface ();
  glk_interface.glk_window_close (win, result);
}

__attribute__ ((weak))
void
glk_window_get_size (winid_t win, glui32 *widthptr, glui32 *heightptr)
{
  ifp_check_interface ();
  glk_interface.glk_window_get_size (win, widthptr, heightptr);
}

__attribute__ ((weak))
void
glk_window_set_arrangement (winid_t win, glui32 method,
                            glui32 size, winid_t keywin)
{
  ifp_check_interface ();
  glk_interface.glk_window_set_arrangement (win, method, size, keywin);
}

__attribute__ ((weak))
void
glk_window_get_arrangement (winid_t win, glui32 *methodptr,
                            glui32 *sizeptr, winid_t *keywinptr)
{
  ifp_check_interface ();
  glk_interface.glk_window_get_arrangement (win, methodptr, sizeptr,
                                             keywinptr);
}

__attribute__ ((weak))
winid_t
glk_window_iterate (winid_t win, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface.glk_window_iterate (win, rockptr);
}

__attribute__ ((weak))
glui32
glk_window_get_rock (winid_t win)
{
  ifp_check_interface ();
  return glk_interface.glk_window_get_rock (win);
}

__attribute__ ((weak))
glui32
glk_window_get_type (winid_t win)
{
  ifp_check_interface ();
  return glk_interface.glk_window_get_type (win);
}

__attribute__ ((weak))
winid_t
glk_window_get_parent (winid_t win)
{
  ifp_check_interface ();
  return glk_interface.glk_window_get_parent (win);
}

__attribute__ ((weak))
winid_t
glk_window_get_sibling (winid_t win)
{
  ifp_check_interface ();
  return glk_interface.glk_window_get_sibling (win);
}

__attribute__ ((weak))
void
glk_window_clear (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_window_clear (win);
}

__attribute__ ((weak))
void
glk_window_move_cursor (winid_t win, glui32 xpos, glui32 ypos)
{
  ifp_check_interface ();
  glk_interface.glk_window_move_cursor (win, xpos, ypos);
}

__attribute__ ((weak))
strid_t
glk_window_get_stream (winid_t win)
{
  ifp_check_interface ();
  return glk_interface.glk_window_get_stream (win);
}

__attribute__ ((weak))
void
glk_window_set_echo_stream (winid_t win, strid_t str)
{
  ifp_check_interface ();
  glk_interface.glk_window_set_echo_stream (win, str);
}

__attribute__ ((weak))
strid_t
glk_window_get_echo_stream (winid_t win)
{
  ifp_check_interface ();
  return glk_interface.glk_window_get_echo_stream (win);
}

__attribute__ ((weak))
void
glk_set_window (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_set_window (win);
}

__attribute__ ((weak))
strid_t
glk_stream_open_file (frefid_t fileref, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_open_file (fileref, fmode, rock);
}

__attribute__ ((weak))
strid_t
glk_stream_open_memory (char *buf, glui32 buflen, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_open_memory (buf, buflen, fmode, rock);
}

__attribute__ ((weak))
void
glk_stream_close (strid_t str, stream_result_t *result)
{
  ifp_check_interface ();
  glk_interface.glk_stream_close (str, result);
}

__attribute__ ((weak))
strid_t
glk_stream_iterate (strid_t str, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_iterate (str, rockptr);
}

__attribute__ ((weak))
glui32
glk_stream_get_rock (strid_t str)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_get_rock (str);
}

__attribute__ ((weak))
void
glk_stream_set_position (strid_t str, glsi32 pos, glui32 seekmode)
{
  ifp_check_interface ();
  glk_interface.glk_stream_set_position (str, pos, seekmode);
}

__attribute__ ((weak))
glui32
glk_stream_get_position (strid_t str)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_get_position (str);
}

__attribute__ ((weak))
void
glk_stream_set_current (strid_t str)
{
  ifp_check_interface ();
  glk_interface.glk_stream_set_current (str);
}

__attribute__ ((weak))
strid_t
glk_stream_get_current (void)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_get_current ();
}

__attribute__ ((weak))
void
glk_put_char (unsigned char ch)
{
  ifp_check_interface ();
  glk_interface.glk_put_char (ch);
}

__attribute__ ((weak))
void
glk_put_char_stream (strid_t str, unsigned char ch)
{
  ifp_check_interface ();
  glk_interface.glk_put_char_stream (str, ch);
}

__attribute__ ((weak))
void
glk_put_string (char *s)
{
  ifp_check_interface ();
  glk_interface.glk_put_string (s);
}

__attribute__ ((weak))
void
glk_put_string_stream (strid_t str, char *s)
{
  ifp_check_interface ();
  glk_interface.glk_put_string_stream (str, s);
}

__attribute__ ((weak))
void
glk_put_buffer (char *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface.glk_put_buffer (buf, len);
}

__attribute__ ((weak))
void
glk_put_buffer_stream (strid_t str, char *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface.glk_put_buffer_stream (str, buf, len);
}

__attribute__ ((weak))
void
glk_set_style (glui32 styl)
{
  ifp_check_interface ();
  glk_interface.glk_set_style (styl);
}

__attribute__ ((weak))
void
glk_set_style_stream (strid_t str, glui32 styl)
{
  ifp_check_interface ();
  glk_interface.glk_set_style_stream (str, styl);
}

__attribute__ ((weak))
glsi32
glk_get_char_stream (strid_t str)
{
  ifp_check_interface ();
  return glk_interface.glk_get_char_stream (str);
}

__attribute__ ((weak))
glui32
glk_get_line_stream (strid_t str, char *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface.glk_get_line_stream (str, buf, len);
}

__attribute__ ((weak))
glui32
glk_get_buffer_stream (strid_t str, char *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface.glk_get_buffer_stream (str, buf, len);
}

__attribute__ ((weak))
void
glk_stylehint_set (glui32 wintype, glui32 styl, glui32 hint, glsi32 val)
{
  ifp_check_interface ();
  glk_interface.glk_stylehint_set (wintype, styl, hint, val);
}

__attribute__ ((weak))
void
glk_stylehint_clear (glui32 wintype, glui32 styl, glui32 hint)
{
  ifp_check_interface ();
  glk_interface.glk_stylehint_clear (wintype, styl, hint);
}

__attribute__ ((weak))
glui32
glk_style_distinguish (winid_t win, glui32 styl1, glui32 styl2)
{
  ifp_check_interface ();
  return glk_interface.glk_style_distinguish (win, styl1, styl2);
}

__attribute__ ((weak))
glui32
glk_style_measure (winid_t win, glui32 styl, glui32 hint, glui32 *result)
{
  ifp_check_interface ();
  return glk_interface.glk_style_measure (win, styl, hint, result);
}

__attribute__ ((weak))
frefid_t
glk_fileref_create_temp (glui32 usage, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_fileref_create_temp (usage, rock);
}

__attribute__ ((weak))
frefid_t
glk_fileref_create_by_name (glui32 usage, char *name, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_fileref_create_by_name (usage, name, rock);
}

__attribute__ ((weak))
frefid_t
glk_fileref_create_by_prompt (glui32 usage, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_fileref_create_by_prompt (usage, fmode, rock);
}

__attribute__ ((weak))
frefid_t
glk_fileref_create_from_fileref (glui32 usage, frefid_t fref, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_fileref_create_from_fileref (usage, fref, rock);
}

__attribute__ ((weak))
void
glk_fileref_destroy (frefid_t fref)
{
  ifp_check_interface ();
  glk_interface.glk_fileref_destroy (fref);
}

__attribute__ ((weak))
frefid_t
glk_fileref_iterate (frefid_t fref, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface.glk_fileref_iterate (fref, rockptr);
}

__attribute__ ((weak))
glui32
glk_fileref_get_rock (frefid_t fref)
{
  ifp_check_interface ();
  return glk_interface.glk_fileref_get_rock (fref);
}

__attribute__ ((weak))
void
glk_fileref_delete_file (frefid_t fref)
{
  ifp_check_interface ();
  glk_interface.glk_fileref_delete_file (fref);
}

__attribute__ ((weak))
glui32
glk_fileref_does_file_exist (frefid_t fref)
{
  ifp_check_interface ();
  return glk_interface.glk_fileref_does_file_exist (fref);
}

__attribute__ ((weak))
void
glk_select (event_t *event)
{
  ifp_check_interface ();
  glk_interface.glk_select (event);
}

__attribute__ ((weak))
void
glk_select_poll (event_t *event)
{
  ifp_check_interface ();
  glk_interface.glk_select_poll (event);
}

__attribute__ ((weak))
void
glk_request_timer_events (glui32 millisecs)
{
  ifp_check_interface ();
  glk_interface.glk_request_timer_events (millisecs);
}

__attribute__ ((weak))
void
glk_request_line_event (winid_t win, char *buf, glui32 maxlen, glui32 initlen)
{
  ifp_check_interface ();
  glk_interface.glk_request_line_event (win, buf, maxlen, initlen);
}

__attribute__ ((weak))
void
glk_request_char_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_request_char_event (win);
}

__attribute__ ((weak))
void
glk_request_mouse_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_request_mouse_event (win);
}

__attribute__ ((weak))
void
glk_cancel_line_event (winid_t win, event_t *event)
{
  ifp_check_interface ();
  glk_interface.glk_cancel_line_event (win, event);
}

__attribute__ ((weak))
void
glk_cancel_char_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_cancel_char_event (win);
}

__attribute__ ((weak))
void
glk_cancel_mouse_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_cancel_mouse_event (win);
}

__attribute__ ((weak))
glui32
glk_buffer_to_lower_case_uni (glui32 *buf, glui32 len, glui32 numchars)
{
  ifp_check_interface ();
  return glk_interface.glk_buffer_to_lower_case_uni (buf, len, numchars);
}

__attribute__ ((weak))
glui32
glk_buffer_to_upper_case_uni (glui32 *buf, glui32 len, glui32 numchars)
{
  ifp_check_interface ();
  return glk_interface.glk_buffer_to_upper_case_uni (buf, len, numchars);
}

__attribute__ ((weak))
glui32
glk_buffer_to_title_case_uni (glui32 *buf,
                              glui32 len, glui32 numchars, glui32 lowerrest)
{
  ifp_check_interface ();
  return glk_interface.glk_buffer_to_title_case_uni (buf,
                                                     len, numchars, lowerrest);
}

__attribute__ ((weak))
void
glk_put_char_uni (glui32 ch)
{
  ifp_check_interface ();
  glk_interface.glk_put_char_uni (ch);
}

__attribute__ ((weak))
void
glk_put_string_uni (glui32 *s)
{
  ifp_check_interface ();
  glk_interface.glk_put_string_uni (s);
}

__attribute__ ((weak))
void
glk_put_buffer_uni (glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface.glk_put_buffer_uni (buf, len);
}

__attribute__ ((weak))
void
glk_put_char_stream_uni (strid_t str, glui32 ch)
{
  ifp_check_interface ();
  glk_interface.glk_put_char_stream_uni (str, ch);
}

__attribute__ ((weak))
void
glk_put_string_stream_uni (strid_t str, glui32 *s)
{
  ifp_check_interface ();
  glk_interface.glk_put_string_stream_uni (str, s);
}

__attribute__ ((weak))
void
glk_put_buffer_stream_uni (strid_t str, glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  glk_interface.glk_put_buffer_stream_uni (str, buf, len);
}

__attribute__ ((weak))
glsi32
glk_get_char_stream_uni (strid_t str)
{
  ifp_check_interface ();
  return glk_interface.glk_get_char_stream_uni (str);
}

__attribute__ ((weak))
glui32
glk_get_buffer_stream_uni (strid_t str, glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface.glk_get_buffer_stream_uni (str, buf, len);
}

__attribute__ ((weak))
glui32
glk_get_line_stream_uni (strid_t str, glui32 *buf, glui32 len)
{
  ifp_check_interface ();
  return glk_interface.glk_get_line_stream_uni (str, buf, len);
}

__attribute__ ((weak))
strid_t
glk_stream_open_file_uni (frefid_t fileref, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_open_file_uni (fileref, fmode, rock);
}

__attribute__ ((weak))
strid_t
glk_stream_open_memory_uni (glui32 *buf,
                            glui32 buflen, glui32 fmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_stream_open_memory_uni (buf, buflen, fmode, rock);
}

__attribute__ ((weak))
void
glk_request_char_event_uni (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_request_char_event_uni (win);
}

__attribute__ ((weak))
void
glk_request_line_event_uni (winid_t win,
                            glui32 *buf, glui32 maxlen, glui32 initlen)
{
  ifp_check_interface ();
  glk_interface.glk_request_line_event_uni (win, buf, maxlen, initlen);
}

__attribute__ ((weak))
glui32
glk_image_draw (winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
  ifp_check_interface ();
  return glk_interface.glk_image_draw (win, image, val1, val2);
}

__attribute__ ((weak))
glui32
glk_image_draw_scaled (winid_t win, glui32 image,
                       glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
  ifp_check_interface ();
  return glk_interface.glk_image_draw_scaled (win, image, val1, val2,
                                               width, height);
}

__attribute__ ((weak))
glui32
glk_image_get_info (glui32 image, glui32 *width, glui32 *height)
{
  ifp_check_interface ();
  return glk_interface.glk_image_get_info (image, width, height);
}

__attribute__ ((weak))
void
glk_window_flow_break (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_window_flow_break (win);
}

__attribute__ ((weak))
void
glk_window_erase_rect (winid_t win,
                       glsi32 left, glsi32 top, glui32 width, glui32 height)
{
  ifp_check_interface ();
  glk_interface.glk_window_erase_rect (win, left, top, width, height);
}

__attribute__ ((weak))
void
glk_window_fill_rect (winid_t win, glui32 color,
                      glsi32 left, glsi32 top, glui32 width, glui32 height)
{
  ifp_check_interface ();
  glk_interface.glk_window_fill_rect (win, color, left, top, width, height);
}

__attribute__ ((weak))
void
glk_window_set_background_color (winid_t win, glui32 color)
{
  ifp_check_interface ();
  glk_interface.glk_window_set_background_color (win, color);
}

__attribute__ ((weak))
schanid_t
glk_schannel_create (glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glk_schannel_create (rock);
}

__attribute__ ((weak))
void
glk_schannel_destroy (schanid_t chan)
{
  ifp_check_interface ();
  glk_interface.glk_schannel_destroy (chan);
}

__attribute__ ((weak))
schanid_t
glk_schannel_iterate (schanid_t chan, glui32 *rockptr)
{
  ifp_check_interface ();
  return glk_interface.glk_schannel_iterate (chan, rockptr);
}

__attribute__ ((weak))
glui32
glk_schannel_get_rock (schanid_t chan)
{
  ifp_check_interface ();
  return glk_interface.glk_schannel_get_rock (chan);
}

__attribute__ ((weak))
glui32
glk_schannel_play (schanid_t chan, glui32 snd)
{
  ifp_check_interface ();
  return glk_interface.glk_schannel_play (chan, snd);
}

__attribute__ ((weak))
glui32
glk_schannel_play_ext (schanid_t chan, glui32 snd, glui32 repeats,
                       glui32 notify)
{
  ifp_check_interface ();
  return glk_interface.glk_schannel_play_ext (chan, snd, repeats, notify);
}

__attribute__ ((weak))
void
glk_schannel_stop (schanid_t chan)
{
  ifp_check_interface ();
  glk_interface.glk_schannel_stop (chan);
}

__attribute__ ((weak))
void
glk_schannel_set_volume (schanid_t chan, glui32 vol)
{
  ifp_check_interface ();
  glk_interface.glk_schannel_set_volume (chan, vol);
}

__attribute__ ((weak))
void
glk_sound_load_hint (glui32 snd, glui32 flag)
{
  ifp_check_interface ();
  glk_interface.glk_sound_load_hint (snd, flag);
}

__attribute__ ((weak))
void
glk_set_hyperlink (glui32 linkval)
{
  ifp_check_interface ();
  glk_interface.glk_set_hyperlink (linkval);
}

__attribute__ ((weak))
void
glk_set_hyperlink_stream (strid_t str, glui32 linkval)
{
  ifp_check_interface ();
  glk_interface.glk_set_hyperlink_stream (str, linkval);
}

__attribute__ ((weak))
void
glk_request_hyperlink_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_request_hyperlink_event (win);
}

__attribute__ ((weak))
void
glk_cancel_hyperlink_event (winid_t win)
{
  ifp_check_interface ();
  glk_interface.glk_cancel_hyperlink_event (win);
}

__attribute__ ((weak))
void
glkunix_set_base_file (char *filename)
{
  ifp_check_interface ();
  glk_interface.glkunix_set_base_file (filename);
}

__attribute__ ((weak))
strid_t
glkunix_stream_open_pathname (char *pathname, glui32 textmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.glkunix_stream_open_pathname (pathname,
                                                      textmode, rock);
}

__attribute__ ((weak))
void
gidispatch_set_object_registry (gidispatch_rock_t (*regi)
                                (void *obj, glui32 objclass),
                                void (*unregi) (void *obj, glui32 objclass,
                                                gidispatch_rock_t objrock))
{
  ifp_check_interface ();
  glk_interface.gidispatch_set_object_registry (regi, unregi);
}

__attribute__ ((weak))
gidispatch_rock_t
gidispatch_get_objrock (void *obj, glui32 objclass)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_get_objrock (obj, objclass);
}

__attribute__ ((weak))
void
gidispatch_set_retained_registry (gidispatch_rock_t (*regi)
                                  (void *array, glui32 len, char *typecode),
                                  void (*unregi) (void *array, glui32 len,
                                                  char *typecode,
                                                  gidispatch_rock_t objrock))
{
  ifp_check_interface ();
  glk_interface.gidispatch_set_retained_registry (regi, unregi);
}

__attribute__ ((weak))
void
gidispatch_call (glui32 funcnum, glui32 numargs, gluniversal_t *arglist)
{
  ifp_check_interface ();
  glk_interface.gidispatch_call (funcnum, numargs, arglist);
}

__attribute__ ((weak))
char *
gidispatch_prototype (glui32 funcnum)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_prototype (funcnum);
}

__attribute__ ((weak))
glui32
gidispatch_count_classes (void)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_count_classes ();
}

__attribute__ ((weak))
glui32
gidispatch_count_intconst (void)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_count_classes ();
}

__attribute__ ((weak))
gidispatch_intconst_t *
gidispatch_get_intconst (glui32 index_)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_get_intconst (index_);
}

__attribute__ ((weak))
glui32
gidispatch_count_functions (void)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_count_functions ();
}

__attribute__ ((weak))
gidispatch_function_t *
gidispatch_get_function (glui32 index_)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_get_function (index_);
}

__attribute__ ((weak))
gidispatch_function_t *
gidispatch_get_function_by_id (glui32 id)
{
  ifp_check_interface ();
  return glk_interface.gidispatch_get_function_by_id (id);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_create_map (strid_t file, giblorb_map_t **newmap)
{
  ifp_check_interface ();
  return glk_interface.giblorb_create_map (file, newmap);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_destroy_map (giblorb_map_t *map)
{
  ifp_check_interface ();
  return glk_interface.giblorb_destroy_map (map);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_load_chunk_by_type (giblorb_map_t *map,
                            glui32 method, giblorb_result_t *res,
                            glui32 chunktype, glui32 count)
{
  ifp_check_interface ();
  return glk_interface.giblorb_load_chunk_by_type (map, method,
                                                    res, chunktype, count);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_load_chunk_by_number (giblorb_map_t *map,
                              glui32 method, giblorb_result_t *res,
                              glui32 chunknum)
{
  ifp_check_interface ();
  return glk_interface.giblorb_load_chunk_by_number (map, method,
                                                      res, chunknum);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_unload_chunk (giblorb_map_t *map, glui32 chunknum)
{
  ifp_check_interface ();
  return glk_interface.giblorb_unload_chunk (map, chunknum);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_load_resource (giblorb_map_t *map,
                       glui32 method, giblorb_result_t *res,
                       glui32 usage, glui32 resnum)
{
  ifp_check_interface ();
  return glk_interface.giblorb_load_resource (map, method, res, usage, resnum);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_count_resources (giblorb_map_t *map,
                         glui32 usage, glui32 *num, glui32 *min,
                         glui32 *max)
{
  ifp_check_interface ();
  return glk_interface.giblorb_count_resources (map, usage, num, min, max);
}

__attribute__ ((weak))
giblorb_err_t
giblorb_set_resource_map (strid_t file)
{
  ifp_check_interface ();
  return glk_interface.giblorb_set_resource_map (file);
}

__attribute__ ((weak))
giblorb_map_t *
giblorb_get_resource_map (void)
{
  ifp_check_interface ();
  return glk_interface.giblorb_get_resource_map ();
}

/* Internal Glk functions. */
strid_t
gli_stream_open_pathname (char *pathname, int textmode, glui32 rock);

__attribute__ ((weak))
strid_t
gli_stream_open_pathname (char *pathname, int textmode, glui32 rock)
{
  ifp_check_interface ();
  return glk_interface.gli_stream_open_pathname (pathname, textmode, rock);
}
