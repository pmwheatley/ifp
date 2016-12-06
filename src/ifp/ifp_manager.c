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
#include <stdlib.h>
#include <string.h>

#include "ifp.h"
#include "ifp_internal.h"


/*
 * The environment variable used as the path for loading plugins.  This is
 * an approximate analog to LD_LIBRARY_PATH.
 */
static const char *PLUGIN_PATH = "IF_PLUGIN_PATH",
                  *DEFAULT_PLUGIN_PATH = "/usr/local/lib/ifp:/usr/lib/ifp";

/* The path setting we use when looking for plugins. */
static char *ifp_plugin_path = NULL;

/*
 * The current plugin that is between startup and main, and a record of
 * the startup data we fed it.
 */
static ifp_pluginref_t ifp_current_plugin = NULL;
static glkunix_startup_t *ifp_current_data = NULL;

/*
 * Flag for setting chained plugins to be cloned on load.  Chaining plugins
 * might want to set this flag so that their contained loaders don't mess
 * up their own operation - others should never have to bother with this.
 */
static int ifp_clone_selected_flag = FALSE;


/**
 * ifp_manager_build_timestamp()
 *
 * Returns the build timestamp for the library.
 */
const char *
ifp_manager_build_timestamp (void)
{
  return __DATE__ ", " __TIME__;
}


/*
 * ifp_manager_finalizer()
 *
 * Force unloading of any current plugin.  On receipt of a signal, we could
 * find that the current plugin owns some resources that it needs to release.
 *
 * This finalizer ensures that any currently set plugin runs its own
 * finalization.
 */
static void
ifp_manager_finalizer (void)
{
  ifp_trace ("manager: ifp_manager_finalizer <- void");

  if (ifp_current_plugin)
    {
      ifp_plugin_force_unload (ifp_current_plugin);
      ifp_current_plugin = NULL;
    }
}


/**
 * ifp_manager_clone_selected_plugins()
 *
 * If flag is TRUE, set the manager up to clone any plugin inside a chaining
 * plugin.  Cloning is a workround to bypass the behaviour of Linux's dlopen
 * in returning the same handle each time the same file is dlopen'ed.  For
 * IF plugins, a different handle is required on each load, so the plugin
 * manager can maintain separate, distinct instances of a plugin.
 */
void
ifp_manager_clone_selected_plugins (int flag)
{
  ifp_clone_selected_flag = (flag != 0);
  ifp_trace ("manager: ifp_clone_selected_flag %s", flag ? "set" : "cleared");
}


/**
 * ifp_manager_cloning_selected()
 *
 * Returns the value of the cloning flag.  If the flag is not set, and the
 * function detects that the code is running inside a chaining plugin, it
 * returns TRUE anyway.
 */
int
ifp_manager_cloning_selected (void)
{
  /*
   * If the cloning flag is not set, but we seem to be inside a chaining
   * plugin anyway, force a return of TRUE.  Otherwise, return the set flag.
   */
  if (!ifp_clone_selected_flag && ifp_self_inside_plugin ())
    {
      ifp_trace ("manager: cloning forced, since ifp_self_inside_plugin ()");
      return TRUE;
    }

  return ifp_clone_selected_flag;
}


/**
 * ifp_manager_set_plugin_path()
 * ifp_manager_get_plugin_path()
 *
 * Set and get the plugin search path.  Setting NULL unsets the path.  If the
 * environment variable IF_PLUGIN_PATH is set, it overrides any set value.
 * If no value or IF_PLUGIN_PATH is set, get returns a default.
 */
void
ifp_manager_set_plugin_path (const char *new_path)
{
  ifp_free (ifp_plugin_path);

  /* If the new path is a string, copy it, otherwise set NULL. */
  if (new_path)
    {
      ifp_trace ("manager: ifp_manager_set_plugin_path set '%s'", new_path);

      ifp_plugin_path = ifp_malloc (strlen (new_path) + 1);
      strcpy (ifp_plugin_path, new_path);
    }
  else
    {
      ifp_trace ("manager: ifp_manager_set_plugin_path cleared path");
      ifp_plugin_path = NULL;
    }
}

const char *
ifp_manager_get_plugin_path (void)
{
  const char *path;

  path = getenv (PLUGIN_PATH);
  if (path)
    ifp_trace ("manager:"
               " ifp_manager_get_plugin_path return env '%s'", path);
  else
    {
      path = ifp_plugin_path;
      if (path)
        ifp_trace ("manager:"
                   " ifp_manager_get_plugin_path return set '%s'", path);
      else
        {
          path = DEFAULT_PLUGIN_PATH;
          ifp_error ("manager:"
                     " no %s variable set, using '%s'", PLUGIN_PATH, path);
        }
    }

  return path;
}


/**
 * ifp_manager_reset_glk_library_partial()
 *
 * Reset enough functions in the Glk library to allow file functions,
 * without actually closing windows.  The aim of this function is to remove
 * callbacks in Glk to plugin code that is going to be unloaded, so that
 * the Glk operations that IFP makes before running the next game plugin
 * do not run into trouble with unresolvable memory references.  The
 * function will not disturb the windows; for that, a full reset is
 * necessary.  This function is intended to be called just before unloading
 * a plugin after running its glk_main() (or glkunix_startup_code(), if
 * this failed).
 */
int
ifp_manager_reset_glk_library_partial (void)
{
  ifp_trace ("manager: ifp_manager_reset_glk_library_partial <- void");

  if (ifp_current_plugin)
    {
      assert (ifp_current_data);
      ifp_error ("manager: a plugin is already active");
      return FALSE;
    }

  /*
   * Stop any Glk timer events, and clear any current Glk interrupt handler
   * callback.  Set dispatch-layer object registry and retained registry
   * to NULL.
   */
  glk_request_timer_events (0);
  glk_set_interrupt_handler (NULL);

  gidispatch_set_retained_registry (NULL, NULL);
  gidispatch_set_object_registry (NULL, NULL);

  ifp_trace ("manager: ifp_manager_reset_glk_library_partial finished");
  return TRUE;
}


/**
 * ifp_manager_reset_glk_library_full()
 *
 * Reset the Glk library to its initial state - close windows, files, and
 * so on.  This function is intended to be called just before starting to
 * use a plugin (that is, just before initializing it).
 */
int
ifp_manager_reset_glk_library_full (void)
{
  winid_t glk_window;
  strid_t glk_stream;
  frefid_t glk_fileref;
  schanid_t glk_schannel;
  glui32 glk_style, glk_stylehint;

  ifp_trace ("manager: ifp_manager_reset_glk_library_full <- void");

  if (ifp_current_plugin)
    {
      assert (ifp_current_data);
      ifp_error ("manager: a plugin is already active");
      return FALSE;
    }

  /* Clear Glk callbacks using the partial Glk reset function. */
  ifp_manager_reset_glk_library_partial ();

  /*
   * TODO Reset the Blorb layer.  At present, this is not possible because
   * the Xglk library doesn't permit outsiders to clear its idea of a
   * resource map, once set with giblorb_set_resource_map().
   */

  /*
   * Close all remaining open windows.  Closing the root should close all
   * remaining windows right away, but we loop just in case.
   */
  for (glk_window = glk_window_get_root ();
       glk_window; glk_window = glk_window_get_root ())
    {
      ifp_trace ("manager: closing glk"
                 " window_%p", ifp_trace_pointer (glk_window));
      glk_window_close (glk_window, NULL);
    }

  /* Close remaining streams and dispose of filerefs. */
  for (glk_stream = glk_stream_iterate (NULL, NULL);
       glk_stream; glk_stream = glk_stream_iterate (NULL, NULL))
    {
      ifp_trace ("manager: closing glk"
                 " stream_%p", ifp_trace_pointer (glk_stream));
      glk_stream_close (glk_stream, NULL);
    }
  for (glk_fileref = glk_fileref_iterate (NULL, NULL);
       glk_fileref; glk_fileref = glk_fileref_iterate (NULL, NULL))
    {
      ifp_trace ("manager: destroying glk"
                 " fileref_%p", ifp_trace_pointer (glk_fileref));
      glk_fileref_destroy (glk_fileref);
    }

  /* Destroy any remaining sound channels. */
  for (glk_schannel = glk_schannel_iterate (NULL, NULL);
       glk_schannel; glk_schannel = glk_schannel_iterate (NULL, NULL))
    {
      ifp_trace ("manager: destroying glk"
                 " schannel_%p", ifp_trace_pointer (glk_schannel));
      glk_schannel_destroy (glk_schannel_iterate (NULL, NULL));
    }

  /*
   * Clear all stylehints set up for windows.  This shows up a memory leak
   * in at least one Glk implementation, Xglk, and perhaps others.  In
   * Xglk 0.4.11, gli_styles_compute() openly and brazenly mallocs() and
   * then loses references to memory.
   *
   * TODO Is this use of style_NUMSTYLES and stylehint_NUMHINTS valid?  This
   * code make the assumption that styles and hints number from zero to NUM*
   * sequentially.  They do, but there's no definition that they must in all
   * Glk implementations.  Glk does allow these constant values to be
   * interrogated through the dispatch layer, but... we use constants such as
   * this all over the place anyway, assuming that they have the same values
   * on all Glk implementations.  And, the gi_dispa.c code advertises itself
   * as "...linked into every Glk library, without change", and defines its
   * intconstant_table values in hardcoded ways anyway.
   *
   * Where a Glk library deviates from the currently implemented and accepted
   * values, it'll have troubles with this code, and likely troubles elsewhere
   * too.  Glk is, I think, trying to foster the illusion that magic numbers
   * aren't all that magic, but they still are really.
   */
  ifp_trace ("manager: clearing glk window style hints");

  for (glk_style = 0; glk_style < style_NUMSTYLES; glk_style++)
    {
      for (glk_stylehint = 0;
           glk_stylehint < stylehint_NUMHINTS; glk_stylehint++)
        {
          glk_stylehint_clear (wintype_AllTypes, glk_style, glk_stylehint);
        }
    }

  ifp_trace ("manager: ifp_manager_reset_glk_library_full finished");
  return TRUE;
}


/**
 * ifp_manager_collect_plugin_garbage()
 *
 * Collect any garbage that a plugin may have left behind after it exited.
 * This will free unfree'd malloc'ed memory, and close any files that the
 * plugin may have left open on glk_exit().
 *
 * Like ifp_reset_glk_library, this function is intended to be called just
 * before starting to use a new use a plugin.  It should not be called
 * while any plugin is active.
 */
int
ifp_manager_collect_plugin_garbage (void)
{
  ifp_trace ("manager: ifp_manager_collect_plugin_garbage <- void");

  if (ifp_current_plugin)
    {
      assert (ifp_current_data);
      ifp_error ("manager: a plugin is already active");
      return FALSE;
    }

  /*
   * Using the Libc interception modules, close unclosed files, and free any
   * unfree'd memory.
   */
  ifp_file_open_files_cleanup ();
  ifp_memory_malloc_garbage_collect ();
  return TRUE;
}


/*
 * ifp_manager_fill_buffer()
 *
 * Load a memory buffer from a Glk stream, given offset and length.  Return
 * TRUE if success, otherwise FALSE.
 */
static int
ifp_manager_fill_buffer (strid_t glk_stream,
                         int offset, int length, char *buffer)
{
  glk_stream_set_position (glk_stream, offset, seekmode_Start);
  if (glk_stream_get_position (glk_stream) != (glui32) offset)
    {
      ifp_trace ("manager: couldn't seek to acceptor");
      return FALSE;
    }

  if (glk_get_buffer_stream (glk_stream, buffer, length) != (glui32) length)
    {
      ifp_trace ("manager: couldn't read acceptor");
      return FALSE;
    }

  return TRUE;
}


/*
 * ifp_manager_attach_plugin()
 *
 * If the plugin chains, set its self-reference.  For both chaining and
 * ordinary plugins, attach the Glk and Libc interfaces to a given plugin.
 * Return FALSE if either interface fails to take.
 */
static int
ifp_manager_attach_plugin (ifp_pluginref_t plugin)
{
  int is_attached;

  ifp_trace ("manager: ifp_manager_attach_plugin <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  /*
   * Setting the plugin's self-reference lets it write error messages
   * prefixed with the plugin name and version.
   */
  if (ifp_plugin_can_chain (plugin))
    ifp_plugin_chain_set_self (plugin);

  if (ifp_plugin_attach_libc_interface (plugin, ifp_libc_get_interface ()))
    {
      if (ifp_plugin_attach_glk_interface (plugin, ifp_glk_get_interface ()))
        {
          ifp_trace ("manager: plugin accepted both interfaces");
          is_attached = TRUE;
        }
      else
        {
          ifp_error ("manager: plugin %s-%s refused the Glk interface",
                     ifp_plugin_engine_name (plugin),
                     ifp_plugin_engine_version (plugin));
          is_attached = FALSE;
        }
    }
  else
    {
      ifp_error ("manager: plugin %s-%s refused the Libc interface",
                 ifp_plugin_engine_name (plugin),
                 ifp_plugin_engine_version (plugin));
      is_attached = FALSE;
    }

  if (!is_attached)
    {
      ifp_plugin_attach_libc_interface (plugin, NULL);
      ifp_plugin_attach_glk_interface (plugin, NULL);

      if (ifp_plugin_can_chain (plugin))
        ifp_plugin_chain_set_self (NULL);
    }

  return is_attached;
}


/*
 * ifp_manager_test_plugin_strid()
 *
 * Access the acceptor fields of the header for plugin given, and then check
 * the data in the stream with the recognizer to see if it looks as if this
 * plugin will accept the input.
 */
static int
ifp_manager_test_plugin_strid (ifp_pluginref_t plugin, strid_t glk_stream)
{
  char *buffer;
  int length, offset;
  const char *pattern;

  ifp_trace ("manager: ifp_manager_test_plugin_strid <-"
             " plugin_%p stream_%p",
             ifp_trace_pointer (plugin), ifp_trace_pointer (glk_stream));

  length = ifp_plugin_acceptor_length (plugin);
  offset = ifp_plugin_acceptor_offset (plugin);
  pattern = ifp_plugin_acceptor_pattern (plugin);

  /*
   * If the acceptor length is 0, or the acceptor pattern is NULL, the plugin
   * doesn't do plain data.  This would be unusual - as far as I know, all
   * Blorb types have a corresponding plain data file format as well.
   */
  if (length == 0 || !pattern)
    {
      ifp_trace ("manager: plugin_%p refused plain data",
                 ifp_trace_pointer (plugin));
      return FALSE;
    }

  if (length <= 0 || offset < 0 || strlen (pattern) == 0)
    {
      ifp_error ("manager: plugin %s-%s has invalid acceptor",
                 ifp_plugin_engine_name (plugin),
                 ifp_plugin_engine_version (plugin));
      return FALSE;
    }

  /*
   * Create a buffer large enough for the acceptor, and fill it from the
   * given stream.
   */
  buffer = ifp_malloc (length);
  if (!ifp_manager_fill_buffer (glk_stream, offset, length, buffer))
    {
      ifp_trace ("manager: couldn't seek or read acceptor for plugin %s-%s",
                 ifp_plugin_engine_name (plugin),
                 ifp_plugin_engine_version (plugin));
      ifp_free (buffer);
      return FALSE;
    }

  /* Check the buffer against the regular expression acceptor. */
  if (ifp_recognizer_match_binary (buffer, length, pattern))
    {
      /* Attach plugin interfaces, and use this plugin if successful. */
      if (ifp_manager_attach_plugin (plugin))
        {
          ifp_trace ("manager: plugin_%p accepted the file",
                     ifp_trace_pointer (plugin));
          ifp_free (buffer);
          return TRUE;
        }
    }

  ifp_trace ("manager: plugin_%p rejected the file",
             ifp_trace_pointer (plugin));
  ifp_free (buffer);
  return FALSE;
}


/*
 * ifp_manager_test_plugin_blorb()
 *
 * Access the Blorb type, if any of the header for plugin given, and then
 * compare the type from the header with the one found in the Blorb input
 * file.
 */
static int
ifp_manager_test_plugin_blorb (ifp_pluginref_t plugin, glui32 blorb_type)
{
  char *blorb_string;
  const char *pattern;

  ifp_trace ("manager: ifp_manager_test_plugin_blorb <-"
             " plugin_%p %lu", ifp_trace_pointer (plugin), blorb_type);

  pattern = ifp_plugin_blorb_pattern (plugin);

  if (!pattern)
    {
      ifp_trace ("manager: plugin_%p does not do Blorb",
                 ifp_trace_pointer (plugin));
      return FALSE;
    }

  if (strlen (pattern) == 0)
    {
      ifp_error ("manager: plugin %s-%s has invalid Blorb type",
                 ifp_plugin_engine_name (plugin),
                 ifp_plugin_engine_version (plugin));
      return FALSE;
    }

  /*
   * Convert the Blorb type passed in to a string, and compare it to the
   * plugin's Blorb type.  Although at present all Blorb types are character,
   * I'm going to use a binary match, since a) I don't know for sure that
   * Blorb types will always map to ASCII, or at least ASCII that regex
   * matching can handle, and b) it makes the pattern stuff symmetrical (and
   * symmetrically awkward) with the ordinary file data matching.
   */
  blorb_string = ifp_blorb_id_to_string (blorb_type);
  if (ifp_recognizer_match_binary (blorb_string, sizeof (blorb_type), pattern))
    {
      /* Attach plugin interfaces, and use this plugin if successful. */
      if (ifp_manager_attach_plugin (plugin))
        {
          ifp_trace ("manager: plugin_%p accepted the file",
                     ifp_trace_pointer (plugin));
          ifp_free (blorb_string);
          return TRUE;
        }
    }

  ifp_trace ("manager: plugin_%p rejected the file",
             ifp_trace_pointer (plugin));
  ifp_free (blorb_string);
  return FALSE;
}


/*
 * ifp_manager_locate_plugin_strid()
 *
 * First, if the stream data is Blorb, set up for a Blorb search.  Otherwise,
 * use the native types the plugin acceptor advertises.
 *
 * For each plugin, compare the Blorb type, or acceptor signature against
 * the relevant part of the data.  Note the first match found, and complain
 * if any other matches are also found.
 */
static ifp_pluginref_t
ifp_manager_locate_plugin_strid (strid_t glk_stream)
{
  int is_blorb;
  glui32 blorb_type;
  ifp_pluginref_t plugin, result;

  ifp_trace ("manager: ifp_manager_locate_plugin_strid <-"
             " stream_%p", ifp_trace_pointer (glk_stream));

  is_blorb = FALSE;
  if (ifp_blorb_is_file_blorb (glk_stream))
    {
      ifp_trace ("manager: input file is Blorb format");

      /*
       * This is a Blorb data file.  Extract the chunk type of the first
       * executable chunk.  If none found, then return NULL.  This implies
       * that no plugin's normal acceptor can use Blorb.
       */
      if (!ifp_blorb_first_exec_type (glk_stream, &blorb_type))
        {
          ifp_trace ("manager: no executable in Blorb");
          return NULL;
        }

      is_blorb = TRUE;
    }

  /* Search loaded plugins for one that can accept the data we have. */
  result = NULL;
  for (plugin = ifp_loader_iterate_plugins (NULL);
       plugin; plugin = ifp_loader_iterate_plugins (plugin))
    {
      int accepted;

      ifp_trace ("manager: trying the file on"
                 " plugin_%p", ifp_trace_pointer (plugin));
      if (is_blorb)
        accepted = ifp_manager_test_plugin_blorb (plugin, blorb_type);
      else
        accepted = ifp_manager_test_plugin_strid (plugin, glk_stream);

      if (accepted)
        {
          if (result)
            {
              ifp_error ("manager:"
                         " plugins %s-%s and %s-%s both accepted the file",
                         ifp_plugin_engine_name (result),
                         ifp_plugin_engine_version (result),
                         ifp_plugin_engine_name (plugin),
                         ifp_plugin_engine_version (plugin));
              ifp_notice ("manager: using first found");
            }
          else
            result = plugin;
        }
    }

  if (result)
    ifp_trace ("manager: file accepted by"
               " plugin_%p", ifp_trace_pointer (result));
  else
    ifp_trace ("manager: no plugin accepted the file");

  return result;
}


/**
 * ifp_manager_locate_plugin()
 *
 * This is the central function of plugin recognition.  It refreshes the
 * loader with all plugins available, then iterates round all available
 * plugins, searching for one willing to accept the input data file.  Any
 * plugin returned has already been initialized, and may be run by the
 * ifp_manager_run_plugin() function.  If no plugin accepts the input
 * data file, the function returns NULL.
 */
ifp_pluginref_t
ifp_manager_locate_plugin (const char *filename)
{
  static int initialized = FALSE;
  const char *plugin_path;
  strid_t glk_stream;
  ifp_pluginref_t result;
  glkunix_startup_t *data;
  assert (filename);

  ifp_trace ("manager: ifp_manager_locate_plugin <- '%s'", filename);

  if (ifp_current_plugin)
    {
      assert (ifp_current_data);
      ifp_error ("manager: a plugin is already active");
      return NULL;
    }

  plugin_path = ifp_manager_get_plugin_path ();
  ifp_loader_search_plugins_path (plugin_path);

  if (ifp_loader_count_plugins () == 0)
    {
      ifp_error ("manager: no plugins found on path '%s'", plugin_path);
      return NULL;
    }

  glk_stream = ifp_glkstream_open_pathname ((char *) filename, FALSE, 0);
  if (glk_stream == 0)
    {
      ifp_error ("manager: failed to open file '%s'", filename);
      return NULL;
    }

  result = ifp_manager_locate_plugin_strid (glk_stream);
  ifp_glkstream_close (glk_stream, NULL);

  if (!result)
    {
      ifp_trace ("manager: returning no usable plugin");
      return NULL;
    }

  /*
   * A plugin's acceptor matched, so see if we need to clone it.  We have to
   * clone if the flag is set (easy).  We also have to clone if we are about
   * to select a chaining plugin, whether the flag says so or not.  This is
   * because the loader code in the chaining plugin will otherwise find itself,
   * and thereby corrupt its own heap space.  This is all very confusing.  It
   * might almost just be simpler, though slightly less efficient, to just
   * clone all selected plugins as a blanket policy.
   */
  if (ifp_manager_cloning_selected () || ifp_plugin_can_chain (result))
    {
      ifp_pluginref_t clone;

      clone = ifp_loader_replace_with_clone (result);
      if (!clone)
        {
          /*
           * We still have the original, but if we let it go out, all hell
           * will probably break loose.  Sigh.
           */
          ifp_error ("manager: failed to clone plugin");
          ifp_loader_forget_plugin (result);
          return NULL;
        }

      ifp_trace ("manager: attaching interfaces for clone"
                 " plugin_%p", ifp_trace_pointer (clone));
      if (!ifp_manager_attach_plugin (clone))
        {
          ifp_error ("manager: failed to attach a clone plugin");
          ifp_loader_forget_plugin (result);
          ifp_loader_forget_plugin (clone);
          return NULL;
        }

      ifp_loader_forget_plugin (result);
      result = clone;
    }

  /*
   * As we're about to run plugin code for the first time, this is a good
   * point to reset Glk to a clean state.  We don't know that the startup
   * code will try to use Glk, but some might well.  We should also garbage-
   * collect for any prior plugin at this point.  We don't do this right
   * after calling the plugin's glk_main because unloading the library may
   * require that stuff malloc'ed on loading is still present (a glibc
   * requirement?).  It's also nice to leave the last game screen displayed.
   */
  ifp_manager_reset_glk_library_full ();
  ifp_manager_collect_plugin_garbage ();

  data = ifp_pref_create_startup_data (result, filename);
  if (!ifp_plugin_glkunix_startup_code (result, data))
    {
      /*
       * Oops - not usable after all.  Arguably, there may be another plugin
       * that could accept the data remaining in the list, and we might want
       * to retry with the remaining plugins.  However, we don't know the
       * state of Glk after we run a startup function, so to be safe we'll
       * fail things here.
       */
      ifp_trace ("manager: startup failed for"
                 " plugin_%p", ifp_trace_pointer (result));
      ifp_pref_forget_startup_data (data);
      ifp_manager_reset_glk_library_partial ();
      ifp_loader_forget_plugin (result);
      return NULL;
    }

  ifp_current_plugin = result;
  ifp_current_data = data;

  /* If this is the first call, register our finalizer for the plugin. */
  if (!initialized)
    {
      ifp_register_finalizer (ifp_manager_finalizer);
      initialized = TRUE;
    }

  ifp_trace ("manager: returning plugin_%p", ifp_trace_pointer (result));
  return result;
}


/**
 * ifp_manager_locate_plugin_url()
 *
 * Open the file in the given url, which must be resolved, then search for
 * a plugin that will accept it.  Any plugin returned is initialized, and
 * may be run with the ifp_manager_run_plugin() function.  If no plugin
 * accepts the url, the function returns NULL.
 */
ifp_pluginref_t
ifp_manager_locate_plugin_url (ifp_urlref_t url)
{
  const char *data_file;
  assert (ifp_url_is_valid (url));

  ifp_trace ("manager: ifp_manager_locate_plugin_url <-"
             " url_%p", ifp_trace_pointer (url));

  data_file = ifp_url_get_data_file (url);
  if (!data_file)
    {
      ifp_error ("manager: unresolved URL passed in");
      return NULL;
    }

  return ifp_manager_locate_plugin (data_file);
}


/**
 * ifp_manager_run_plugin()
 *
 * Execute the glk_main() function of the plugin.  The given plugin must
 * match the current plugin (the one last initialized from this module).
 */
void
ifp_manager_run_plugin (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("manager: ifp_manager_run_plugin <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (!ifp_current_plugin)
    {
      ifp_error ("manager: there is no current plugin");
      return;
    }
  else if (plugin != ifp_current_plugin)
    {
      ifp_error ("manager: plugin is not the current one");
      return;
    }

  ifp_plugin_glk_main (plugin);

  ifp_pref_forget_startup_data (ifp_current_data);
  ifp_current_data = NULL;
  ifp_current_plugin = NULL;

  ifp_manager_reset_glk_library_partial ();
}
