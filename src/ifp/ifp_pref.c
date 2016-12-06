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

/* Preferences set magic identifier, for safety purposes. */
static const unsigned int PREF_MAGIC = 0x291e8779;


/*
 * Internal definition of a preferences list structure.  The list is un-
 * sorted, and contains every preference registered with the module.  On
 * a request for a startdata set, the list is searched for entries that
 * match the plugin passed in, and the constructed startdata returned.
 */
struct ifp_prefs
{
  unsigned int magic;

  /* Engine name and version string, and option string to add. */
  char *engine_name;
  char *engine_version;
  char *preference;

  /* List next and prior element. */
  struct ifp_prefs *next;
  struct ifp_prefs *prior;
};

/* List of preferences, with tail pointer for easy additions to the end. */
static ifp_prefref_t ifp_prefs_head = NULL,
                     ifp_prefs_tail = NULL;

/* Read-only lock flag, set if we are working with a "foreign" data set. */
static int ifp_prefs_readonly = FALSE;


/*
 * ifp_pref_is_valid()
 *
 * Confirms that the address passed in refers to preferences.  Returns TRUE
 * if the address is valid, FALSE otherwise.
 */
static int
ifp_pref_is_valid (ifp_prefref_t entry)
{
  return entry && entry->magic == PREF_MAGIC;
}


/*
 * ifp_pref_use_foreign_data()
 *
 * Force use of an external preferences string list.  Typically, called
 * from the main program, which needs to pass preference data to chaining
 * plugins.  Calling this function locks the lists against updates, to
 * prevent accidental corruptions.
 *
 * This is a one-way operation, and destroys anything currently saved in
 * the local list.
 */
void
ifp_pref_use_foreign_data (ifp_prefref_t prefs_list)
{
  ifp_prefref_t entry;
  assert (!prefs_list || ifp_pref_is_valid (prefs_list));

  ifp_trace ("preferences: ifp_pref_use_foreign_data <-"
             " prefs_%p", ifp_trace_pointer (prefs_list));

  /* If the current list data is ours, destroy everything on it. */
  if (!ifp_prefs_readonly)
    {
      for (entry = ifp_prefs_head; entry; entry = ifp_prefs_head)
        {
          ifp_prefs_head = entry->next;

          ifp_free (entry->engine_name);
          ifp_free (entry->engine_version);
          ifp_free (entry->preference);
          memset (entry, 0xaa, sizeof (*entry));
          ifp_free (entry);
        }
    }

  /*
   * Reset the head and tail pointers to match the input list, then lock
   * the list readonly.
   */
  ifp_prefs_head = ifp_prefs_tail = prefs_list;
  for (entry = ifp_prefs_head; entry; entry = entry->next)
    ifp_prefs_tail = entry;

  ifp_prefs_readonly = TRUE;
}


/*
 * ifp_pref_get_local_data()
 *
 * Return a data item usable as foreign data for a secondary preferences
 * module.
 */
ifp_prefref_t
ifp_pref_get_local_data (void)
{
  ifp_trace ("preferences: ifp_pref_get_local_data <- void");

  return ifp_prefs_head;
}


/**
 * ifp_pref_list_arguments()
 *
 * Returns a list of options that plugin with the given name and version
 * understands.  The function will load all usable plugins in order to
 * return this information, and leave them loaded.  The list returned is
 * a structure of type glkunix_argumentlist_t.  If no plugin matches the
 * engine name and version passed in, the function returns NULL.
 */
glkunix_argumentlist_t *
ifp_pref_list_arguments (const char *engine_name,
                         const char *engine_version)
{
  ifp_pluginref_t plugin, match;
  assert (engine_name && engine_version);

  ifp_loader_search_plugins_path (ifp_manager_get_plugin_path ());

  match = NULL;
  for (plugin = ifp_loader_iterate_plugins (NULL);
       plugin; plugin = ifp_loader_iterate_plugins (plugin))
    {
      if (strcmp (ifp_plugin_engine_name (plugin), engine_name) == 0
          && strcmp (ifp_plugin_engine_version (plugin), engine_version) == 0)
        {
          match = plugin;
          break;
        }
    }

  return match ? ifp_plugin_get_arguments (match) : NULL;
}


/*
 * ifp_pref_match_entry()
 *
 * Match a preferences entry with a set of strings.  NULL is interpreted
 * in this match as a wildcard, and thus matches everything.  Engine name
 * and version are compared without case sensitivity; preference _is_ case-
 * sensitive.
 */
static int
ifp_pref_match_entry (ifp_prefref_t entry,
                      const char *engine_name, const char *engine_version,
                      const char *preference)
{
  return ((!engine_name
           || strcasecmp (engine_name, entry->engine_name) == 0)
          && (!engine_version
              || strcasecmp (engine_version, entry->engine_version) == 0)
          && (!preference
              || strcmp (preference, entry->preference) == 0));
}


/**
 * ifp_pref_unregister()
 *
 * Remove a set of preferences from the module.  The engine name and/or
 * version may be NULL, signifying that the entry is a wildcard.  If the
 * preference is also NULL, then all preferences matching the given engine
 * name and version will be deleted.
 */
void
ifp_pref_unregister (const char *engine_name,
                     const char *engine_version, const char *preference)
{
  ifp_prefref_t entry, next;

  ifp_trace ("preferences: ifp_pref_unregister <- '%s' '%s' '%s'",
             engine_name ? engine_name : "*",
             engine_version ? engine_version : "*",
             preference ? preference : "*");

  if (ifp_prefs_readonly)
    {
      ifp_error ("preferences: this list is readonly");
      return;
    }

  /* Search for entry matches in the list, and delete any found. */
  for (entry = ifp_prefs_head; entry; entry = next)
    {
      next = entry->next;

      if (ifp_pref_match_entry (entry,
                                engine_name, engine_version, preference))
        {
          ifp_trace ("preferences: match found, deleting"
                     " prefs_%p", ifp_trace_pointer (entry));

          if (entry->next)
            entry->next->prior = entry->prior;
          if (entry->prior)
            entry->prior->next = entry->next;

          if (entry == ifp_prefs_tail)
            ifp_prefs_tail = entry->prior;
          if (entry == ifp_prefs_head)
            ifp_prefs_head = entry->next;

          ifp_free (entry->engine_name);
          ifp_free (entry->engine_version);
          ifp_free (entry->preference);
          memset (entry, 0xaa, sizeof (*entry));
          ifp_free (entry);
        }
    }
}


/**
 * ifp_pref_register()
 *
 * Register a preference with the module.  The engine name and/or version
 * may be NULL, signifying that the entry is a wildcard, and will match with
 * any names/versions of engines on requests for startdata.  The preference
 * may not be NULL - this makes no sense.
 */
int
ifp_pref_register (const char *engine_name,
                   const char *engine_version, const char *preference)
{
  ifp_prefref_t entry, new_entry;
  char *name_copy, *version_copy, *preference_copy;

  ifp_trace ("preferences: ifp_pref_register <- '%s' '%s' '%s'",
             engine_name ? engine_name : "*",
             engine_version ? engine_version : "*",
             preference ? preference : "(null)");

  if (ifp_prefs_readonly)
    {
      ifp_error ("preferences: this list is readonly");
      return FALSE;
    }
  if (!preference)
    {
      ifp_error ("preferences: registered preference can't be NULL");
      return FALSE;
    }

  /* See if this entry is already present in the set. */
  for (entry = ifp_prefs_head; entry; entry = entry->next)
    {
      if (ifp_pref_match_entry (entry,
                                engine_name, engine_version, preference))
        {
          ifp_trace ("preferences: entry matches"
                     " prefs_%p", ifp_trace_pointer (entry));
          return FALSE;
        }
    }

  /* Create a new entry and populate with data copies. */
  new_entry = ifp_malloc (sizeof (*new_entry));
  name_copy = version_copy = NULL;
  if (engine_name)
    {
      name_copy = ifp_malloc (strlen (engine_name) + 1);
      strcpy (name_copy, engine_name);
    }
  if (engine_version)
    {
      version_copy = ifp_malloc (strlen (engine_version) + 1);
      strcpy (version_copy, engine_version);
    }
  preference_copy = ifp_malloc (strlen (preference) + 1);
  strcpy (preference_copy, preference);

  new_entry->magic = PREF_MAGIC;
  new_entry->engine_name = name_copy;
  new_entry->engine_version = version_copy;
  new_entry->preference = preference_copy;
  new_entry->next = NULL;
  new_entry->prior = NULL;

  /* Add this preference set to the list. */
  if (!ifp_prefs_head)
    {
      assert (!ifp_prefs_tail);
      ifp_prefs_head = ifp_prefs_tail = new_entry;
    }
  else
    {
      new_entry->prior = ifp_prefs_tail;
      ifp_prefs_tail->next = new_entry;
      ifp_prefs_tail = new_entry;
    }

  ifp_trace ("preferences: new entry returned is"
             " prefs_%p", ifp_trace_pointer (new_entry));
  return TRUE;
}


/*
 * ifp_pref_match_plugin()
 *
 * Match a preferences entry to a particular plugin.
 */
static int
ifp_pref_match_plugin (ifp_prefref_t entry, ifp_pluginref_t plugin)
{
  /*
   * Return true if a string match, or a wildcard match on NULL, for either
   * the engine name or engine version.
   */
  return ((!entry->engine_name
           || strcasecmp (entry->engine_name,
                          ifp_plugin_engine_name (plugin)) == 0)
          && (!entry->engine_version
              || strcasecmp (entry->engine_version,
                             ifp_plugin_engine_version (plugin)) == 0));
}


/**
 * ifp_pref_create_startup_data()
 *
 * Create a Glk startdata array for a given plugin and filename.  The array
 * is composed of all matching preference option strings, in list order,
 * followed by the filename, and a sentinel NULL.
 */
glkunix_startup_t *
ifp_pref_create_startup_data (ifp_pluginref_t plugin,
                              const char *filename)
{
  ifp_prefref_t entry;
  int count, index_;
  char **args;
  glkunix_startup_t *result;
  assert (ifp_plugin_is_valid (plugin) && filename);

  ifp_trace ("preferences: ifp_pref_create_startup_data <-"
             " plugin_%p '%s'", ifp_trace_pointer (plugin), filename);

  /* Count the matching entries to size the return array. */
  count = 0;
  for (entry = ifp_prefs_head; entry; entry = entry->next)
    {
      if (ifp_pref_match_plugin (entry, plugin))
        count++;
    }

  /*
   * Size the return data array to be this, plus three (the "main" program
   * name at the start of the array, the file name at the end of the array,
   * and the NULL array terminator).
   */
  args = ifp_malloc (sizeof (char *) * (count + 3));
  ifp_trace ("preferences: array size is %d entries", count + 3);

  /*
   * Set up argv[0].  In general, we expect the plugin to ignore this, or
   * maybe use it in error message printouts.  For a modicum of accuracy,
   * we'll set it to the plugin's name.
   */
  index_ = 0;
  args[index_] = ifp_malloc (strlen (ifp_plugin_engine_name (plugin)) + 1);
  strcpy (args[index_++], ifp_plugin_engine_name (plugin));
  ifp_trace ("preferences: argv[%d] = '%s'", index_ - 1, args[index_ - 1]);

  /* Set up argv[1..n] with a copy of each preference string. */
  assert (index_ == 1);
  for (entry = ifp_prefs_head; entry; entry = entry->next)
    {
      if (ifp_pref_match_plugin (entry, plugin))
        {
          args[index_] = ifp_malloc (strlen (entry->preference) + 1);
          strcpy (args[index_++], entry->preference);
          ifp_trace ("preferences:"
                     " argv[%d] = '%s'", index_ - 1, args[index_ - 1]);
        }
    }

  /* Add a copy of the filename, and then the sentinel NULL. */
  assert (index_ - 1 == count);
  args[index_] = ifp_malloc (strlen (filename) + 1);
  strcpy (args[index_++], filename);
  ifp_trace ("preferences: argv[%d] = '%s'", index_ - 1, args[index_ - 1]);

  assert (index_ - 2 == count);
  args[index_++] = NULL;
  ifp_trace ("preferences: argv[%d] = (nil)", index_ - 1);

  /* Create the return startdata, and populate it. */
  result = ifp_malloc (sizeof (glkunix_startup_t));
  result->argc = count + 2;
  result->argv = args;

  ifp_trace ("preferences: returned startup_%p", ifp_trace_pointer (result));
  return result;
}


/**
 * ifp_pref_create_startup_data_url()
 *
 * Create a Glk startdata array for a given plugin and url.
 */
glkunix_startup_t *
ifp_pref_create_startup_data_url (ifp_pluginref_t plugin,
                                  ifp_urlref_t url)
{
  const char *data_file;
  assert (ifp_plugin_is_valid (plugin) && ifp_url_is_valid (url));

  ifp_trace ("preferences: ifp_pref_create_startup_data_url <-"
             " plugin_%p url_%p",
             ifp_trace_pointer (plugin), ifp_trace_pointer (url));

  data_file = ifp_url_get_data_file (url);
  if (!data_file)
    {
      ifp_error ("preferences: unresolved URL passed in");
      return NULL;
    }

  return ifp_pref_create_startup_data (plugin, data_file);
}


/**
 * ifp_pref_forget_startup_data()
 *
 * Destroy Glk startdata constructed by ifp_pref_create_startup_data[_url].
 */
void
ifp_pref_forget_startup_data (glkunix_startup_t *data)
{
  int index_;
  assert (data);

  ifp_trace ("preferences: ifp_pref_forget_startup_data <-"
             " startup_%p", ifp_trace_pointer (data));

  for (index_ = 0; index_ < data->argc; index_++)
    ifp_free (data->argv[index_]);

  ifp_free (data->argv);
  ifp_free (data);
}
