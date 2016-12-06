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
#include <setjmp.h>

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Plugin magic identifier, for safety purposes.  We slot this into each
 * new plugin created, and check when one comes back that it has this
 * magic number in it.
 */
static const unsigned int PLUGIN_MAGIC = 0x24ade36e;

/*
 * Internal definition of a plugin structure.  Client callers get to see
 * an opaque handle for these, and they pass them back and forth to functions
 * in this module.  This is the only module that should be concerned with
 * the details of what is in a plugin.
 *
 * Ideally, the list stuff lives outside of the plugin structure.  But for
 * now, to make life simple, we'll let it stay inside.
 *
 * Plugin states are as follows:
 *
 *  new             load            init                run
 * ------> Unloaded -----> Attached -----> Initialized -----> Running
 *          |    ^          |    |                               |
 *          |    |  unload  |    |                      return,  |
 *          |    +----------+    |                      glk_exit |
 *  destroy |    |               |   init failed                 v
 * <--------+    |               +--------------------------> Finished
 *               |                                               |
 *               |                  unload                       |
 *               +-----------------------------------------------+
 *
 * Attempts at other state transitions are errors.
 */
struct ifp_plugin
{
  unsigned int magic;

  /* Plugin state. */
  enum
  { PLUGIN_UNLOADED = 1,
    PLUGIN_ATTACHED, PLUGIN_INITIALIZED, PLUGIN_RUNNING, PLUGIN_FINISHED
  } state;

  /* Shared object handle and filename, and header data. */
  void *handle;
  char *filename;
  ifp_headerref_t ifpi_header;

  /* Analogs to _init and _fini, called on load and unload. */
  void (*ifpi_initializer) (void);
  void (*ifpi_finalizer) (void);

  /* Plugin's Glk and Libc interface attach and retrieve functions. */
  int (*ifpi_attach_glk_interface) (ifp_glk_interfaceref_t);
  ifp_glk_interfaceref_t (*ifpi_retrieve_glk_interface) (void);
  int (*ifpi_attach_libc_interface) (ifp_libc_interfaceref_t);
  ifp_libc_interfaceref_t (*ifpi_retrieve_libc_interface) (void);

  /* Assorted chaining support functions. */
  void (*ifpi_chain_set_plugin_self) (ifp_pluginref_t);
  ifp_pluginref_t (*ifpi_chain_return_plugin) (void);
  void (*ifpi_chain_accept_preferences) (ifp_prefref_t);
  void (*ifpi_chain_accept_plugin_path) (const char*);

  /* Plugin's Glk arguments list. */
  glkunix_argumentlist_t *ifpi_glkunix_arguments;

  /* Plugin main code startup and run functions */
  int (*ifpi_glkunix_startup_code) (glkunix_startup_t *);
  void (*ifpi_glk_main) (void);

  /* List next and prior element. */
  ifp_pluginref_t next;
  ifp_pluginref_t prior;
};

/* A longjump buffer for managing calls from a plugin to glk_exit(). */
static jmp_buf glk_exit_jmp_buffer;
static int glk_exit_is_handleable = FALSE;

/*
 * Forward declarations of the function that we use to override glk_exit
 * in the Glk interface.
 */
static void ifp_plugin_override_glk_exit (void);


/**
 * ifp_plugin_is_valid()
 *
 * Confirms that the address passed in refers to a plugin.  Returns TRUE
 * if the address is to a plugin, FALSE otherwise.
 */
int
ifp_plugin_is_valid (ifp_pluginref_t plugin)
{
  return plugin && plugin->magic == PLUGIN_MAGIC;
}


/*
 * ifp_plugin_set_next()
 * ifp_plugin_get_next()
 * ifp_plugin_set_prior()
 * ifp_plugin_get_prior()
 *
 * Plugin list building accessors and mutators.
 */
void
ifp_plugin_set_next (ifp_pluginref_t plugin, ifp_pluginref_t next)
{
  assert (ifp_plugin_is_valid (plugin));
  assert (!next || ifp_plugin_is_valid (next));

  plugin->next = next;
}

ifp_pluginref_t
ifp_plugin_get_next (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  return plugin->next;
}

void
ifp_plugin_set_prior (ifp_pluginref_t plugin, ifp_pluginref_t prior)
{
  assert (ifp_plugin_is_valid (plugin));
  assert (!prior || ifp_plugin_is_valid (prior));

  plugin->prior = prior;
}

ifp_pluginref_t
ifp_plugin_get_prior (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  return plugin->prior;
}


/**
 * ifp_plugin_is_loaded()
 *
 * Returns TRUE if the given plugin is loaded, otherwise FALSE.
 */
int
ifp_plugin_is_loaded (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  return plugin->state != PLUGIN_UNLOADED;
}


/**
 * ifp_plugin_is_initializable()
 *
 * Returns TRUE if the given plugin is initializable, otherwise FALSE.
 */
int
ifp_plugin_is_initializable (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  return plugin->state == PLUGIN_ATTACHED;
}


/**
 * ifp_plugin_is_runnable()
 *
 * Returns TRUE if the given plugin is runnable, otherwise FALSE.
 */
int
ifp_plugin_is_runnable (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  return plugin->state == PLUGIN_INITIALIZED;
}


/*
 * ifp_plugin_get_header()
 *
 * Returns the header structure for the plugin.  The plugin should be loaded.
 */
ifp_headerref_t
ifp_plugin_get_header (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to get header of an unloaded plugin");
      return NULL;
    }

  return plugin->ifpi_header;
}


/**
 * ifp_plugin_get_arguments()
 *
 * Return the Glk arguments list array for the plugin.  The plugin should
 * be loaded.
 */
glkunix_argumentlist_t *
ifp_plugin_get_arguments (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to get arguments of an unloaded plugin");
      return NULL;
    }

  return plugin->ifpi_glkunix_arguments;
}


/**
 * ifp_plugin_get_filename()
 *
 * Returns the filename used to load the plugin.  The plugin should be
 * loaded.
 */
const char *
ifp_plugin_get_filename (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to get filename of an unloaded plugin");
      return NULL;
    }

  return plugin->filename;
}


/**
 * ifp_plugin_chain_set_self()
 *
 * Sets a chaining plugin up to know its own identity.  This call does
 * nothing if the plugin is not a chaining plugin.  The plugin should
 * be loaded.
 */
void
ifp_plugin_chain_set_self (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to set chain of an unloaded plugin");
      return;
    }

  if (plugin->ifpi_chain_set_plugin_self)
    {
      ifp_trace ("plugin: sending self-reference to"
                 " plugin_%p", ifp_trace_pointer (plugin));
      plugin->ifpi_chain_set_plugin_self (plugin);
    }
}


/**
 * ifp_plugin_can_chain()
 *
 * Returns TRUE if this plugin claims the capability to chain to other plugins.
 * It is an error to call this function with a plugin that is not loaded.
 */
int
ifp_plugin_can_chain (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to test chaining of an unloaded plugin");
      return FALSE;
    }

  return plugin->ifpi_chain_set_plugin_self != NULL;
}


/**
 * ifp_plugin_get_chain()
 *
 * Returns the plugin, if any, to which this plugin is chained.  A return
 * of NULL indicates no chain, or the last plugin in the chain.  The
 * plugin should be loaded.
 */
ifp_pluginref_t
ifp_plugin_get_chain (ifp_pluginref_t plugin)
{
  ifp_pluginref_t chain_plugin;
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin: ifp_plugin_get_chain <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to get chain of an unloaded plugin");
      return NULL;
    }

  /*
   * If we have a symbol for the chain function, call it.  If not, then the
   * plugin is not a chaining plugin, so return NULL.
   */
  if (plugin->ifpi_chain_return_plugin)
    chain_plugin = plugin->ifpi_chain_return_plugin ();
  else
    chain_plugin = NULL;

  ifp_trace ("plugin: ifp_plugin_get_chain returned"
             " plugin_%p", ifp_trace_pointer (chain_plugin));

  return chain_plugin;
}


/**
 * ifp_plugin_new()
 *
 * Returns a new, and empty, plugin.
 */
ifp_pluginref_t
ifp_plugin_new (void)
{
  ifp_pluginref_t plugin;

  ifp_trace ("plugin: ifp_plugin_new <- void");

  plugin = ifp_malloc (sizeof (*plugin));
  memset (plugin, 0, sizeof (*plugin));
  plugin->magic = PLUGIN_MAGIC;
  plugin->state = PLUGIN_UNLOADED;

  ifp_trace ("plugin: new plugin is"
             " plugin_%p", ifp_trace_pointer (plugin));

  return plugin;
}


/**
 * ifp_plugin_destroy()
 *
 * Empty and destroy a plugin.  It is an error to try to destroy a plugin
 * that is not unloaded.
 */
void
ifp_plugin_destroy (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin: ifp_plugin_destroy <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state != PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to destroy a loaded plugin");
      return;
    }

  ifp_trace ("plugin: destroying plugin_%p", ifp_trace_pointer (plugin));
  memset (plugin, 0xaa, sizeof (*plugin));
  ifp_free (plugin);
}


/**
 * ifp_plugin_is_unloadable()
 *
 * Query whether the plugin is in a state that permits a caller to unload it.
 * A plugin may not be unloaded if it has been initialized but not yet run
 * to completion, or if it is currently running.
 */
int
ifp_plugin_is_unloadable (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  return plugin->state == PLUGIN_UNLOADED
         || plugin->state == PLUGIN_ATTACHED
         || plugin->state == PLUGIN_FINISHED;
}


/**
 * ifp_plugin_unload()
 *
 * Unload a specified plugin.  It is an error to unload a plugin if it has
 * been initialized but not yet run to completion, or if it is currently
 * running.
 */
void
ifp_plugin_unload (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin: ifp_plugin_unload <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state == PLUGIN_INITIALIZED || plugin->state == PLUGIN_RUNNING)
    {
      ifp_error ("plugin: attempt to unload active plugin");
      return;
    }

  if (plugin->state == PLUGIN_ATTACHED || plugin->state == PLUGIN_FINISHED)
    {
      if (plugin->ifpi_finalizer)
        {
          ifp_trace ("plugin: finalizing"
                     " plugin_%p", ifp_trace_pointer (plugin));
          plugin->ifpi_finalizer ();
        }

      ifp_dlclose (plugin->handle);
      ifp_free (plugin->filename);

      memset (plugin, 0, sizeof (*plugin));
      plugin->magic = PLUGIN_MAGIC;
      plugin->state = PLUGIN_UNLOADED;

      ifp_trace ("plugin: unloaded plugin_%p", ifp_trace_pointer (plugin));
    }
}


/*
 * ifp_plugin_force_unload
 *
 * Force unloading of a plugin even if active.  Used on abnormal termination,
 * to get the plugin's finalization to occur.
 */
void
ifp_plugin_force_unload (ifp_pluginref_t plugin)
{
  ifp_trace ("plugin: ifp_plugin_force_unload <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state != PLUGIN_UNLOADED)
    {
      if (plugin->ifpi_finalizer)
        {
          ifp_trace ("plugin: finalizing"
                     " plugin_%p", ifp_trace_pointer (plugin));
          plugin->ifpi_finalizer ();
        }

      ifp_dlclose (plugin->handle);
      ifp_free (plugin->filename);

      memset (plugin, 0, sizeof (*plugin));
      plugin->magic = PLUGIN_MAGIC;
      plugin->state = PLUGIN_UNLOADED;

      ifp_trace ("plugin: unloaded plugin_%p", ifp_trace_pointer (plugin));
    }
}


/**
 * ifp_plugin_load()
 *
 * Try to load the given file as an IF plugin.  If successful, return TRUE,
 * otherwise, return FALSE.  The plugin passed in must not be loaded.
 */
int
ifp_plugin_load (ifp_pluginref_t plugin, const char *filename)
{
  void *handle;
  ifp_headerref_t header;
  void *initializer, *finalizer,
       *glkunix_arguments_, *attach_glk_interface,
       *retrieve_glk_interface, *attach_libc_interface,
       *retrieve_libc_interface, *chain_set_plugin_self,
       *chain_return_plugin, *chain_accept_preferences,
       *chain_accept_plugin_path, *glkunix_startup_code_, *glk_main_;
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin: ifp_plugin_load <-"
             " plugin_%p '%s'", ifp_trace_pointer (plugin), filename);

  if (plugin->state != PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to load a loaded plugin");
      return FALSE;
    }

  handle = ifp_dlopen (filename);
  if (!handle)
    {
      ifp_trace ("plugin: dlopen failed: %s", ifp_dlerror ());
      return FALSE;
    }

  /* Find the plugin header, and check its version. */
  header = ifp_dlsym (handle, "ifpi_header");
  if (!header)
    {
      ifp_trace ("plugin: no ifpi_header: %s", ifp_dlerror ());
      ifp_dlclose (handle);
      return FALSE;
    }

  if (header->version != IFP_HEADER_VERSION)
    {
      ifp_error ("plugin: %s: invalid plugin header version", filename);
      ifp_dlclose (handle);
      return FALSE;
    }

  /*
   * Retrieve the functions we're expected to call on loading and unloading,
   * analogs to _init and _fini.  Either or both could be NULL, though as
   * they're defined in libifppi's finalizer.c, they usually won't be.
   */
  initializer = ifp_dlsym (handle, "ifpi_initializer");
  finalizer = ifp_dlsym (handle, "ifpi_finalizer");

  /*
   * If the plugin has a symbol in it that indicates that it contains main
   * IFP library functions, then it is probably a chaining plugin.   In this
   * case, note its other functions to control passing chaining data back
   * and forth; otherwise, set these to NULL.
   */
  chain_set_plugin_self = ifp_dlsym (handle, "ifpi_chain_set_plugin_self");
  if (chain_set_plugin_self)
    {
      chain_return_plugin = ifp_dlsym (handle, "ifpi_chain_return_plugin");
      if (!chain_return_plugin)
        {
          ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
          ifp_dlclose (handle);
          return FALSE;
        }

      chain_accept_preferences = ifp_dlsym (handle,
                                            "ifpi_chain_accept_preferences");
      if (!chain_accept_preferences)
        {
          ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
          ifp_dlclose (handle);
          return FALSE;
        }

      chain_accept_plugin_path = ifp_dlsym (handle,
                                            "ifpi_chain_accept_plugin_path");
      if (!chain_accept_plugin_path)
        {
          ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
          ifp_dlclose (handle);
          return FALSE;
        }
    }
  else
    {
      chain_return_plugin = NULL;
      chain_accept_preferences = NULL;
      chain_accept_plugin_path = NULL;
    }

  /* Get addresses of the functions that set and get Glk interface. */
  attach_glk_interface = ifp_dlsym (handle, "ifpi_attach_glk_interface");
  if (!attach_glk_interface)
    {
      ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
      ifp_dlclose (handle);
      return FALSE;
    }
  retrieve_glk_interface = ifp_dlsym (handle, "ifpi_retrieve_glk_interface");
  if (!retrieve_glk_interface)
    {
      ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
      ifp_dlclose (handle);
      return FALSE;
    }

  /* Get addresses of the functions that set and get Libc interface. */
  attach_libc_interface = ifp_dlsym (handle, "ifpi_attach_libc_interface");
  if (!attach_libc_interface)
    {
      ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
      ifp_dlclose (handle);
      return FALSE;
    }
  retrieve_libc_interface = ifp_dlsym (handle, "ifpi_retrieve_libc_interface");
  if (!retrieve_libc_interface)
    {
      ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
      ifp_dlclose (handle);
      return FALSE;
    }

  /*
   * Get the Glk arguments list.  Try for an ifpi_-prefixed variety first,
   * then back off to the main Glk symbol.
   */
  glkunix_arguments_ = ifp_dlsym (handle, "ifpi_glkunix_arguments");
  if (!glkunix_arguments_)
    {
      glkunix_arguments_ = ifp_dlsym (handle, "glkunix_arguments");
      if (!glkunix_arguments_)
        {
          ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
          ifp_dlclose (handle);
          return FALSE;
        }
    }

  /*
   * Get the rest of the control functions.  Try for ifpi_-prefixed varieties
   * first, then back off to reading the main Glk control function symbols.
   * This allows an override if necessary.
   */
  glkunix_startup_code_ = ifp_dlsym (handle, "ifpi_glkunix_startup_code");
  if (!glkunix_startup_code_)
    {
      glkunix_startup_code_ = ifp_dlsym (handle, "glkunix_startup_code");
      if (!glkunix_startup_code_)
        {
          ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
          ifp_dlclose (handle);
          return FALSE;
        }
    }
  glk_main_ = ifp_dlsym (handle, "ifpi_glk_main");
  if (!glk_main_)
    {
      glk_main_ = ifp_dlsym (handle, "glk_main");
      if (!glk_main_)
        {
          ifp_error ("plugin: %s: %s", filename, ifp_dlerror ());
          ifp_dlclose (handle);
          return FALSE;
        }
    }

  /* Copy details to the assigned plugin entry. */
  plugin->state = PLUGIN_ATTACHED;
  plugin->handle = handle;
  plugin->filename = ifp_malloc (strlen (filename) + 1);
  strcpy (plugin->filename, filename);
  plugin->ifpi_header = header;
  plugin->ifpi_initializer = initializer;
  plugin->ifpi_finalizer = finalizer;
  plugin->ifpi_attach_glk_interface = attach_glk_interface;
  plugin->ifpi_retrieve_glk_interface = retrieve_glk_interface;
  plugin->ifpi_attach_libc_interface = attach_libc_interface;
  plugin->ifpi_retrieve_libc_interface = retrieve_libc_interface;
  plugin->ifpi_chain_set_plugin_self = chain_set_plugin_self;
  plugin->ifpi_chain_return_plugin = chain_return_plugin;
  plugin->ifpi_chain_accept_preferences = chain_accept_preferences;
  plugin->ifpi_chain_accept_plugin_path = chain_accept_plugin_path;
  plugin->ifpi_glkunix_arguments = glkunix_arguments_;
  plugin->ifpi_glkunix_startup_code = glkunix_startup_code_;
  plugin->ifpi_glk_main = glk_main_;

  /* Finally, if the plugin has an initializer, call it. */
  if (plugin->ifpi_initializer)
    {
      ifp_trace ("plugin: initializing plugin_%p", ifp_trace_pointer (plugin));
      plugin->ifpi_initializer ();
    }

  ifp_trace ("plugin: loaded plugin_%p [%s-%s]",
             ifp_trace_pointer (plugin),
             header->engine_name, header->engine_version);

  return TRUE;
}


/**
 * ifp_plugin_new_load()
 *
 * Convenience function to create a new plugin already loaded with a shared
 * object file.  If the plugin can't be loaded, the function returns NULL.
 */
ifp_pluginref_t
ifp_plugin_new_load (const char *filename)
{
  ifp_pluginref_t plugin;

  ifp_trace ("plugin: ifp_plugin_new_load <- '%s'", filename);

  plugin = ifp_plugin_new ();
  if (!ifp_plugin_load (plugin, filename))
    {
      ifp_plugin_destroy (plugin);
      return NULL;
    }

  return plugin;
}


/**
 * ifp_plugin_attach_glk_interface()
 * ifp_plugin_retrieve_glk_interface()
 *
 * Set and get the Glk interface for a plugin.  The plugin must be loaded.
 */
int
ifp_plugin_attach_glk_interface (ifp_pluginref_t plugin,
                                 ifp_glk_interfaceref_t glk_interface)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin:"
             " ifp_plugin_attach_glk_interface <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to set Glk on an unloaded plugin");
      return FALSE;
    }

  /* Override the Glk interface's glk_exit with our own. */
  if (glk_interface)
    glk_interface->glk_exit = ifp_plugin_override_glk_exit;

  return plugin->ifpi_attach_glk_interface (glk_interface);
}

ifp_glk_interfaceref_t
ifp_plugin_retrieve_glk_interface (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin:"
             " ifp_plugin_retrieve_glk_interface <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to get Glk on an unloaded plugin");
      return NULL;
    }

  return plugin->ifpi_retrieve_glk_interface ();
}


/**
 * ifp_plugin_attach_libc_interface()
 * ifp_plugin_retrieve_libc_interface()
 *
 * Set and get the Libc interface for a plugin.  The plugin must be loaded.
 */
int
ifp_plugin_attach_libc_interface (ifp_pluginref_t plugin,
                                  ifp_libc_interfaceref_t libc_interface)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin:"
             " ifp_plugin_attach_libc_interface <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to set Libc on an unloaded plugin");
      return FALSE;
    }

  return plugin->ifpi_attach_libc_interface (libc_interface);
}

ifp_libc_interfaceref_t
ifp_plugin_retrieve_libc_interface (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin:"
             " ifp_plugin_retrieve_libc_interface <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state == PLUGIN_UNLOADED)
    {
      ifp_error ("plugin: attempt to get Libc on an unloaded plugin");
      return NULL;
    }

  return plugin->ifpi_retrieve_libc_interface ();
}


/**
 * ifp_plugin_initialize()
 * synonym: ifp_plugin_glkunix_startup_code()
 *
 * Execute the interpreter glkunix_startup_code in a plugin engine.  Return
 * TRUE if the the plugin's glkunix_startup_code() returned a non-zero value,
 * and FALSE if it returned zero.  Also, return FALSE if the plugin's
 * glkunix_startup_code() terminated with a call to glk_exit().  The plugin
 * must be loaded.
 */
int
ifp_plugin_initialize (ifp_pluginref_t plugin, glkunix_startup_t *data)
{
  int index_, status;
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin: ifp_plugin_initialize <-"
             " plugin_%p", ifp_trace_pointer (plugin));
  for (index_ = 0; index_ < data->argc; index_++)
    ifp_trace ("plugin:   data->argv[%d] = '%s'", index_, data->argv[index_]);

  if (plugin->state != PLUGIN_ATTACHED)
    {
      ifp_error ("plugin: attempt to reinitialize a plugin");
      return FALSE;
    }

  /*
   * When we get a call to glk_exit, and intercept it, there's no way to tell
   * which plugin it came from.  So, for now, we can only let one plugin be
   * active at a time...
   */
  if (glk_exit_is_handleable)
    {
      ifp_error ("plugin: attempt at multiple simultaneous plugins");
      return FALSE;
    }

  /*
   * If this looks like a chaining plugin, send it our list of registered
   * preferences, so it can build startup data as we would, and our plugin
   * search path.
   */
  if (plugin->ifpi_chain_accept_preferences)
    {
      ifp_trace ("plugin: sending prefs to chaining plugin"
                 " plugin_%p", ifp_trace_pointer (plugin));
      plugin->ifpi_chain_accept_preferences (ifp_pref_get_local_data ());
    }

  if (plugin->ifpi_chain_accept_plugin_path)
    {
      ifp_trace ("plugin:"
                 " sending plugin path to chaining plugin"
                 " plugin_%p", ifp_trace_pointer (plugin));
      plugin->ifpi_chain_accept_plugin_path (ifp_manager_get_plugin_path ());
    }

  /* Use setjmp to catch plugin calls to glk_exit(). */
  if (setjmp (glk_exit_jmp_buffer) == 0)
    {
      glk_exit_is_handleable = TRUE;
      ifp_trace ("plugin: setjmp for plugin_%p", ifp_trace_pointer (plugin));

      ifp_trace ("plugin: calling plugin's glkunix_startup_code");
      status = plugin->ifpi_glkunix_startup_code (data);
      ifp_trace ("plugin: plugin's glkunix_startup_code returned normally");
    }
  else
    {
      ifp_trace ("plugin: plugin's glkunix_startup_code called glk_exit");
      status = FALSE;
    }

  memset (&glk_exit_jmp_buffer, 0, sizeof (glk_exit_jmp_buffer));
  glk_exit_is_handleable = FALSE;

  if (status)
    {
      plugin->state = PLUGIN_INITIALIZED;
      ifp_trace ("plugin: plugin's glkunix_startup_code succeeded");
    }
  else
    {
      plugin->state = PLUGIN_FINISHED;
      ifp_trace ("plugin: plugin's glkunix_startup_code failed");
    }

  return status;
}

int
ifp_plugin_glkunix_startup_code (ifp_pluginref_t plugin,
                                 glkunix_startup_t *data)
{
  return ifp_plugin_initialize (plugin, data);
}


/**
 * ifp_plugin_run()
 * synonym: ifp_plugin_glk_main()
 *
 * Execute the interpreter glk_main in a plugin engine.  The plugin must
 * have been initialized successfully first.  The function calls the plugin's
 * glk_main, and returns when glk_main exits, or when it calls glk_exit().
 * It uses a longjump to capture calls that glk_main makes to glk_exit().
 * Once it has been run, an interpreter plugin may not be run a second time;
 * to run the same interpreter again, it is necessary to unload and reload
 * the plugin.
 */
void
ifp_plugin_run (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin: ifp_plugin_run <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state != PLUGIN_INITIALIZED)
    {
      ifp_error ("plugin: attempt to run an uninitialized plugin");
      return;
    }

  /* Again, permit only one call at a time into plugin code. */
  if (glk_exit_is_handleable)
    {
      ifp_error ("plugin: attempt at multiple simultaneous plugins");
      return;
    }

  /* Use setjmp to catch plugin calls to glk_exit(). */
  if (setjmp (glk_exit_jmp_buffer) == 0)
    {
      glk_exit_is_handleable = TRUE;
      ifp_trace ("plugin: setjmp for plugin_%p", ifp_trace_pointer (plugin));

      plugin->state = PLUGIN_RUNNING;
      ifp_trace ("plugin: calling plugin's glk_main");
      plugin->ifpi_glk_main ();
      ifp_trace ("plugin: plugin's glk_main returned normally");
    }
  else
    ifp_trace ("plugin: plugin's glk_main called glk_exit");

  memset (&glk_exit_jmp_buffer, 0, sizeof (glk_exit_jmp_buffer));
  glk_exit_is_handleable = FALSE;

  /*
   * If this looks like a chaining plugin, clear the plugin search path we
   * sent it, to free manager malloc'ed memory.  The equivalent isn't
   * strictly necessary for preferences, but we'll do it anyway.
   */
  if (plugin->ifpi_chain_accept_preferences)
    {
      ifp_trace ("plugin:"
                 " clearing any preferences in chaining"
                 " plugin_%p", ifp_trace_pointer (plugin));
      plugin->ifpi_chain_accept_preferences (NULL);
    }

  if (plugin->ifpi_chain_accept_plugin_path)
    {
      ifp_trace ("plugin:"
                 " clearing plugin path in chaining"
                 " plugin_%p", ifp_trace_pointer (plugin));
      plugin->ifpi_chain_accept_plugin_path (NULL);
    }

  /*
   * The plugin's glk_main exited, or the plugin called glk_exit().  Either
   * way, we have finished with it.
   */
  plugin->state = PLUGIN_FINISHED;
}

void
ifp_plugin_glk_main (ifp_pluginref_t plugin)
{
  ifp_plugin_run (plugin);
}


/*
 * ifp_plugin_override_glk_exit()
 *
 * Override routine for plugin calls to glk_exit().  We can't have this
 * go to the real glk_exit(), since that stops the whole program.  So here
 * we grab it, and use a longjump to terminate the plugin's glk_main()
 * call without terminating the whole program.
 */
static void
ifp_plugin_override_glk_exit (void)
{
  ifp_trace ("plugin: ifp_plugin_override_glk_exit <- void");

  assert (glk_exit_is_handleable);
  longjmp (glk_exit_jmp_buffer, 1);
  ifp_fatal ("plugin: return from the dead");
}


/**
 * ifp_plugin_cancel()
 *
 * This function cancels a running plugin.  It uses a longjump to simulate
 * the plugin's interpreter having called glk_exit(), so the effect is to
 * instantly stop a running plugin.  The function is intended for use in
 * event-based programs, and can be called, for example, by a 'stop' button
 * on a GUI.
 *
 * The function does not return on success.  Instead, IFP behaves as if
 * ifp_plugin_run() has returned.  On failure (say, a non-running plugin
 * reference is passed in), the function will return.
 */
void
ifp_plugin_cancel (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("plugin: ifp_plugin_cancel <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (plugin->state != PLUGIN_RUNNING)
    {
      ifp_error ("plugin: attempt to cancel a non-running plugin");
      return;
    }

  assert (glk_exit_is_handleable);
  longjmp (glk_exit_jmp_buffer, 1);
  ifp_fatal ("plugin: return from the dead");
}
