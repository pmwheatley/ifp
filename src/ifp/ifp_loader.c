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
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Temporary file template, DSO extension, and path separator. */
static const char *TMPFILE_TEMPLATE = "/tmp/ifp_so_XXXXXX",
                  *DSO_EXTENSION = ".so",
                  PATH_SEPARATOR = ':';

/* List of loaded plugins, and tail pointer for easy additions to the end. */
static ifp_pluginref_t ifp_plugins_head = NULL,
                       ifp_plugins_tail = NULL;


/*
 * ifp_loader_delete_plugin()
 *
 * Delete a plugin from the list of plugins.
 */
static void
ifp_loader_delete_plugin (ifp_pluginref_t plugin)
{
  ifp_pluginref_t next, prior;

  ifp_trace ("loader: ifp_loader_delete_plugin <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  prior = ifp_plugin_get_prior (plugin);
  next = ifp_plugin_get_next (plugin);

  if (next)
    ifp_plugin_set_prior (next, prior);
  if (prior)
    ifp_plugin_set_next (prior, next);

  /*
   * If this is the head or tail plugin, adjust the main list pointers to
   * accommodate the removal.
   */
  if (plugin == ifp_plugins_tail)
    ifp_plugins_tail = ifp_plugin_get_prior (plugin);
  if (plugin == ifp_plugins_head)
    ifp_plugins_head = ifp_plugin_get_next (plugin);
}


/*
 * ifp_loader_add_plugin()
 *
 * Add the given plugin to the tail of the main plugins list.
 */
static void
ifp_loader_add_plugin (ifp_pluginref_t plugin)
{
  ifp_trace ("loader: ifp_loader_add_plugin <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  if (!ifp_plugins_head)
    {
      assert (!ifp_plugins_tail);
      ifp_plugins_head = ifp_plugins_tail = plugin;
    }
  else
    {
      ifp_plugin_set_prior (plugin, ifp_plugins_tail);
      ifp_plugin_set_next (ifp_plugins_tail, plugin);
      ifp_plugins_tail = plugin;
    }
}


/**
 * ifp_loader_iterate_plugins()
 *
 * Iterator round the list of known plugins.  If current is NULL, returns
 * the first in the list, otherwise returns the next past current.
 */
ifp_pluginref_t
ifp_loader_iterate_plugins (ifp_pluginref_t current)
{
  assert (!current || ifp_plugin_is_valid (current));

  return current ? ifp_plugin_get_next (current) : ifp_plugins_head;
}


/**
 * ifp_loader_count_plugins()
 *
 * Return the number of plugins currently in the loader.
 */
int
ifp_loader_count_plugins (void)
{
  ifp_pluginref_t plugin;
  int count;

  count = 0;
  for (plugin = ifp_plugins_head;
       plugin; plugin = ifp_plugin_get_next (plugin))
    count++;

  return count;
}


/*
 * ifp_loader_clone_plugin()
 *
 * This routine is a kludge, put in to allow chaining plugins.  What normally
 * happens on dlopen is that the Linux shared object library, when asked to
 * open a .so file, will happily hand us back an existing handle if any
 * other part of the process has opened that file.  It checks inode number
 * and filesystem.  Unfortunately, we don't want that.  The IFP plugin
 * mechanism, when it comes to chaining, would very much like to have two,
 * or more, distinct loads of a .so happening at the same time (for example,
 * for a .gz.gz file).
 *
 * To work around this, we can actually make a physical file copy of the .so
 * file, then load this instead of the thing we were looking at originally.
 * So... if we are inside a chaining plugin, when we've found a plugin that
 * looks like it will accept the data, we'll need to clone that plugin in
 * the loader, and unload the original from the loader (otherwise the one
 * in the chaining plugin's loader and the one in the main loader are in fact
 * the same plugin, and it all goes horribly wrong).  To be on the safe side,
 * we also clone any plugin if we see it is a chaining plugin, otherwise
 * there is a danger that the plugin will find and corrupt itself in its
 * own contained loader.
 *
 * This sounds nasty, but since it applies only to chaining plugins and
 * to the selected final plugin they choose to handle their data, it's not
 * as bad as it might appear at first.
 *
 * TODO how on earth can we do this better?  Dlopen could really use
 * something like an RTLD_DONT_BE_A_SMARTYPANTS flag, but it doesn't have
 * one (yet).
 */
static ifp_pluginref_t
ifp_loader_clone_plugin (ifp_pluginref_t plugin)
{
  strid_t glk_stream;
  ifp_pluginref_t new_plugin;
  const char *filename;
  char buffer[4096], *tmpfilename;
  int tmpfile_, bytes, total_bytes;

  ifp_trace ("loader: ifp_loader_clone_plugin <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  /* Create a temporary file for the copy of the plugin file. */
  tmpfilename = ifp_malloc (strlen (TMPFILE_TEMPLATE) + 1);
  strcpy (tmpfilename, TMPFILE_TEMPLATE);
  tmpfile_ = mkstemp (tmpfilename);
  if (tmpfile_ == -1)
    {
      ifp_error ("loader: error creating temporary file '%s'", tmpfilename);
      unlink (tmpfilename);
      ifp_free (tmpfilename);
      return NULL;
    }

  /* Open the file containing our target plugin. */
  filename = ifp_plugin_get_filename (plugin);
  glk_stream = ifp_glkstream_open_pathname ((char *) filename, FALSE, 0);
  if (glk_stream == 0)
    {
      ifp_error ("loader: failed to open file '%s'", filename);
      return NULL;
    }

  /* Read from input and write to tmpfile, until no more data. */
  total_bytes = 0;
  bytes = glk_get_buffer_stream (glk_stream, buffer, sizeof (buffer));
  while (bytes != 0)
    {
      if (write (tmpfile_, buffer, bytes) != bytes)
        {
          ifp_error ("loader: write error on cloned plugin");

          glk_stream_close (glk_stream, NULL);
          close (tmpfile_);

          unlink (tmpfilename);
          ifp_free (tmpfilename);
          return NULL;
        }

      /* Sum up the bytes transferred, and get the next chunk. */
      total_bytes += bytes;
      bytes = glk_get_buffer_stream (glk_stream, buffer, sizeof (buffer));
    }

  ifp_trace ("loader: cloning plugin copied %d bytes", total_bytes);

  ifp_glkstream_close (glk_stream, NULL);
  close (tmpfile_);

  /*
   * Load the file copy into a plugin.  As it's a copy of a file that is
   * already loaded, it really should not fail to load...
   */
  new_plugin = ifp_plugin_new_load (tmpfilename);
  if (!new_plugin)
    {
      ifp_error ("loader: wholly unexpected error cloning a plugin");
      unlink (tmpfilename);
      ifp_free (tmpfilename);
      return NULL;
    }

  ifp_trace ("loader: cloned plugin is "
             " plugin_%p", ifp_trace_pointer (new_plugin));

  /*
   * Unlink the file now - we don't need the name again, as it's safely
   * loaded, and unlinking now saves us needing to clean up later.
   */
  unlink (tmpfilename);
  ifp_free (tmpfilename);

  return new_plugin;
}


/**
 * ifp_loader_replace_with_clone()
 *
 * Given a plugin, this function will clone it, and replace the listed
 * original with the clone.  If the original is not on the list, the clone
 * is still added.  If the clone fails, the function returns NULL.
 *
 * The original plugin is removed from the loader list, but not unloaded or
 * destroyed.  The caller will probably want to do that.
 */
ifp_pluginref_t
ifp_loader_replace_with_clone (ifp_pluginref_t plugin)
{
  ifp_pluginref_t clone;
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("loader:"
             " ifp_loader_replace_with_clone <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  clone = ifp_loader_clone_plugin (plugin);
  if (!clone)
    {
      ifp_error ("loader: failed to clone plugin for file '%s'",
                 ifp_plugin_get_filename (plugin));
      return NULL;
    }

  /* On our list, replace the plugin passed in with the new clone. */
  ifp_trace ("loader: new cloned plugin is "
             " plugin_%p", ifp_trace_pointer (clone));
  ifp_loader_delete_plugin (plugin);
  ifp_loader_add_plugin (clone);

  return clone;
}


/**
 * ifp_loader_load_plugin()
 *
 * Attempt to load the shared object file given as an IF plugin.  Return the
 * plugin if successful, and NULL if not, or if the plugin is a duplicate of
 * something already listed.
 */
ifp_pluginref_t
ifp_loader_load_plugin (const char *filename)
{
  ifp_pluginref_t plugin, cursor;

  ifp_trace ("loader: ifp_loader_load_plugin <- '%s'", filename);

  /* Search for this filename in already loaded plugins. */
  for (cursor = ifp_plugins_head;
       cursor; cursor = ifp_plugin_get_next (cursor))
    {
      if (strcmp (filename, ifp_plugin_get_filename (cursor)) == 0)
        {
          ifp_trace ("loader: file '%s' already listed", filename);
          return NULL;
        }
    }

  /* Create and load a new plugin for this file to inhabit. */
  plugin = ifp_plugin_new_load (filename);
  if (!plugin)
    {
      ifp_trace ("loader: file '%s' refused to load", filename);
      return NULL;
    }

  /*
   * Now see if this is a different file path, but still duplicates an
   * existing plugin.  If duplicate, refuse to add and return fail status.
   */
  for (cursor = ifp_plugins_head;
       cursor; cursor = ifp_plugin_get_next (cursor))
    {
      if (ifp_plugin_is_equal (plugin, cursor))
        {
          ifp_trace ("loader:"
                     " plugin_%p is a duplicate of plugin_%p",
                     ifp_trace_pointer (plugin), ifp_trace_pointer (cursor));
          ifp_plugin_unload (plugin);
          ifp_plugin_destroy (plugin);
          return NULL;
        }
    }

  /* Loaded okay, and distinct - add to the list and return. */
  ifp_loader_add_plugin (plugin);

  ifp_trace ("loader: return new plugin_%p", ifp_trace_pointer (plugin));
  return plugin;
}


/**
 * ifp_loader_filter_plugins_directory()
 * ifp_loader_search_plugins_directory()
 *
 * Find each available shared object file in a directory, and attempt to
 * load it as an IF plugin.
 */
static int
ifp_loader_filter_plugins_directory (const struct dirent *entry)
{
  const char *extension;

  /* Check the file name for a recognized DSO extension. */
  extension = strrchr (entry->d_name, '.');
  return extension && strcmp (extension, DSO_EXTENSION) == 0;
}

int
ifp_loader_search_plugins_directory (const char *directory_path)
{
  struct dirent **entries;
  int filenames, index_;
  int count;

  ifp_trace ("loader: ifp_loader_search_plugins_dir <- '%s'", directory_path);

  /* Scan the given directory, and accumulate entries of interest. */
  filenames = scandir (directory_path, &entries,
                       ifp_loader_filter_plugins_directory, alphasort);
  if (filenames == -1)
    {
      ifp_error ("loader:"
                 " error scanning directory '%s'", directory_path);
      return 0;
    }

  ifp_trace ("loader: searching directory '%s'", directory_path);

  /* Try to load each listed file as a plugin, and count successful loads. */
  count = 0;
  for (index_ = 0; index_ < filenames; index_++)
    {
      const char *filename;
      char *path;
      int allocation;

      filename = entries[index_]->d_name;
      ifp_trace ("loader: considering file '%s'", filename);

      allocation = strlen (directory_path) + strlen (filename) + 2;
      path = ifp_malloc (allocation);
      snprintf (path, allocation, "%s/%s", directory_path, filename);

      if (ifp_loader_load_plugin (path))
        count++;

      ifp_free (path);
    }

  /* Free each individual entry, then the entries array itself. */
  for (index_ = 0; index_ < filenames; index_++)
    free (entries[index_]);
  free (entries);

  ifp_trace ("loader: loaded %d plugins from directory", count);
  return count;
}


/**
 * ifp_loader_search_plugins_path()
 *
 * Given a ':'-separated path of directories, find each available shared
 * object in the directories file on that path, and attempt to load as a
 * plugin.
 */
int
ifp_loader_search_plugins_path (const char *load_path)
{
  char **elements;
  int count, index_, loaded;

  ifp_trace ("loader: ifp_loader_search_plugins_path <- '%s'", load_path);

  /* Split the path string on ':' characters. */
  count = ifp_split_string (load_path, PATH_SEPARATOR, &elements);

  /*
   * Search each directory given in the path for loadable plugins.  Keep
   * a count of successful plugin loads.
   */
  loaded = 0;
  for (index_ = 0; index_ < count; index_++)
    {
      const char *directory_path;

      directory_path = elements[index_];
      ifp_trace ("loader: considering directory '%s'", directory_path);

      if (strlen (directory_path) > 0)
        loaded += ifp_loader_search_plugins_directory (directory_path);
    }

  ifp_free_split_string (elements, count);

  ifp_trace ("loader: loaded %d plugins from path", count);
  return loaded;
}


/**
 * ifp_loader_forget_plugin()
 *
 * Remove from the list, unload, and destroy the given plugin.
 */
void
ifp_loader_forget_plugin (ifp_pluginref_t plugin)
{
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("loader: ifp_loader_forget_plugin <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  /*
   * If the plugin is active, something has gone wrong.  This function should
   * not be called until a running plugin has completed execution.
   */
  if (!ifp_plugin_is_unloadable (plugin))
    {
      ifp_error ("loader: attempt to reap an active plugin");
      return;
    }

  /*
   * Delete the plugin from the loader's list, unload it to finalize anything
   * in it, then finally destroy it.
   */
  ifp_loader_delete_plugin (plugin);
  ifp_plugin_unload (plugin);
  ifp_plugin_destroy (plugin);
}


/**
 * ifp_loader_forget_all_plugins()
 *
 * The other half of searching for plugins, this function unloads and
 * destroys all unused plugins.  Its expectation on being called is that
 * no plugin in the loader's list is active; they are all either quiescent
 * (that is, loaded but never initialized or run), or finished executing.
 */
int
ifp_loader_forget_all_plugins (void)
{
  ifp_pluginref_t plugin;
  int count;

  ifp_trace ("loader: ifp_loader_forget_all_plugins <- void");

  count = 0;
  for (plugin = ifp_plugins_head; plugin; plugin = ifp_plugins_head)
    {
      ifp_loader_forget_plugin (plugin);
      count++;
    }

  return count;
}
