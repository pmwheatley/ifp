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
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Fallback default Glk library lists -- X, terminal, and dumb. */
static const char *DEFAULT_XGLK_LIBRARY = "xglk",
                  *DEFAULT_TERMGLK_LIBRARY = "glkterm",
                  *DEFAULT_DUMBGLK_LIBRARY = "cheapglk";

/* Separator strings and characters, and Glk library DSO extension. */
static const char PATH_SEPARATOR = ':',
                  LIST_SEPARATOR = ',',
                  DIRECTORY_SEPARATOR = '/',
                  *DSO_EXTENSION = ".so";

/* Any Glk search list set by configuration. */
static char *ifp_glk_libraries = NULL;


/*
 * ifp_main_set_glk_libraries()
 *
 * Called from configuration to set the Glk library lists.  May be over-
 * ridden with $IFP_GLK_LIBRARIES or by -glk on the command line.
 */
void
ifp_main_set_glk_libraries (const char *glk_libraries)
{
  ifp_free (ifp_glk_libraries);

  if (glk_libraries)
    {
      ifp_glk_libraries = ifp_malloc (strlen (glk_libraries) + 1);
      strcpy (ifp_glk_libraries, glk_libraries);
    }
  else
    ifp_glk_libraries = NULL;
}


/*
 * ifp_main_get_glk_libraries()
 *
 * Get the Glk library list.  Setting NULL unsets the list.  If the
 * environment variable IFP_GLK_LIBRARIES is set, it overrides any set value.
 * If no value or IFP_GLK_LIBRARIES is set, get tries to return a default
 * for X and terminal use, depending on DISPLAY and TERM settings.
 */
static const char *
ifp_main_get_glk_libraries (void)
{
  const char *libraries;

  libraries = getenv ("IFP_GLK_LIBRARIES");
  if (libraries)
    ifp_trace ("main:"
               " ifp_main_get_glk_libraries return env '%s'", libraries);
  else
    {
      libraries = ifp_glk_libraries;
      if (libraries)
        ifp_trace ("main:"
                   " ifp_main_get_glk_libraries return set '%s'", libraries);
      else
        {
          if (getenv ("DISPLAY"))
            libraries = DEFAULT_XGLK_LIBRARY;
          else if (getenv ("TERM"))
            libraries = DEFAULT_TERMGLK_LIBRARY;
          else
            libraries = DEFAULT_DUMBGLK_LIBRARY;

          ifp_notice ("main: no %s variable set, using default '%s'",
                      "IFP_GLK_LIBRARIES", libraries);
        }
    }

  return libraries;
}


/*
 * ifp_main_glk_library_matches_filename()
 *
 * Return TRUE if the Glk library is a close enough match to the file name
 * passed in.  The match is either on all before the '.' separator (up to
 * the ".so.n.n.n"), all before the '.' separator with "lib" prepended, or
 * just all.
 */
static int
ifp_main_glk_library_matches_filename (const char *glk_library,
                                       const char *filename)
{
  const char *extension;
  int glk_library_length;

  if (strcmp (filename, glk_library) == 0)
    return TRUE;

  glk_library_length = strlen (glk_library);

  extension = strchr (filename, '.');
  if (extension
      && glk_library_length == extension - filename
      && strncmp (filename, glk_library, glk_library_length) == 0)
    return TRUE;

  if (extension
      && strncmp (filename, "lib", 3) == 0
      && glk_library_length == extension - filename - 3
      && strncmp (filename + 3, glk_library, glk_library_length) == 0)
    return TRUE;

  return FALSE;
}


/*
 * ifp_main_load_glk_from_directory()
 *
 * Search a directory for the matching Glk library, and return TRUE if any
 * match found and loaded.
 */
static int
ifp_main_filter_glk_directory (const struct dirent *entry)
{
  return strstr (entry->d_name, DSO_EXTENSION) != NULL;
}

static int
ifp_main_load_glk_from_directory (const char *glk_library,
                                  const char *directory_path)
{
  struct dirent **entries;
  int filenames, index_;
  int is_loaded;

  /* Scan the given directory, and accumulate entries of interest. */
  filenames = scandir (directory_path,
                       &entries, ifp_main_filter_glk_directory, NULL);
  if (filenames == -1)
    {
      ifp_error ("main:"
                 " error scanning directory '%s'", directory_path);
      return FALSE;
    }

  ifp_trace ("main: searching directory '%s'", directory_path);

  /*
   * Consider each file in the directory, searching for likely looking Glk
   * libraries to load.  Break the loop on a successful load.
   */
  is_loaded = FALSE;
  for (index_ = 0; index_ < filenames && !is_loaded; index_++)
    {
      const char *filename;

      filename = entries[index_]->d_name;
      ifp_trace ("main: considering file '%s'", filename);

      if (ifp_main_glk_library_matches_filename (glk_library, filename))
        {
          int allocation;
          char *path;

          allocation = strlen (directory_path) + strlen (filename) + 2;
          path = ifp_malloc (allocation);
          snprintf (path, allocation, "%s/%s", directory_path, filename);

          is_loaded = ifp_glk_load_interface (path);
          if (is_loaded)
            ifp_trace ("main: glk library loaded from %s", path);

          ifp_free (path);
        }
    }

  /* Free each individual entry, then the entries array itself. */
  for (index_ = 0; index_ < filenames; index_++)
    free (entries[index_]);
  free (entries);

  return is_loaded;
}


/*
 * ifp_main_load_glk_from_path()
 *
 * Search each of a ':'-separated path of directories for a Glk library
 * matching the specification passed in.  Return TRUE if one of the
 * directories offered a matching Glk library.
 */
static int
ifp_main_load_glk_from_path (const char *glk_library, const char *search_path)
{
  char **elements;
  int count, index_, is_loaded;

  /* Split the path string on ':' characters. */
  count = ifp_split_string (search_path, PATH_SEPARATOR, &elements);

  ifp_trace ("main: searching path '%s'", search_path);

  /* Search each directory given in the path for Glk libraries plugins. */
  is_loaded = FALSE;
  for (index_ = 0; index_ < count; index_++)
    {
      const char *directory_path;

      directory_path = elements[index_];
      ifp_trace ("main: considering directory '%s'", directory_path);

      if (strlen (directory_path) > 0
          && ifp_main_load_glk_from_directory (glk_library, directory_path))
        {
          is_loaded = TRUE;
          break;
        }
    }

  ifp_free_split_string (elements, count);

  return is_loaded;
}


/*
 * ifp_main_load_glk()
 *
 * Try to load a Glk library from the list given.  If a listed library
 * contains a '/', treat as a full path to the library, otherwise search
 * the standard plugins path for a matching Glk loadable library.
 */
static int
ifp_main_load_glk (const char *glk_libraries)
{
  char **elements;
  int count, index_, is_loaded;
  const char *search_path;

  ifp_trace ("main: ifp_main_load_glk <- '%s'", glk_libraries);

  /* Split the Glk libraries list string on ',' characters. */
  count = ifp_split_string (glk_libraries, LIST_SEPARATOR, &elements);

  /* Search individually for each listed Glk library. */
  search_path = NULL;
  is_loaded = FALSE;
  for (index_ = 0; index_ < count; index_++)
    {
      const char *glk_library;

      glk_library = elements[index_];
      ifp_trace ("main: searching for library '%s'", glk_library);

      if (strlen (glk_library) > 0)
        {
          /*
           * If this looks like a full directory path, try to load directly.
           * If not, search the path for a suitable match.
           */
          if (strchr (glk_library, DIRECTORY_SEPARATOR))
            {
              if (ifp_glk_verify_dso (glk_library)
                  && ifp_glk_load_interface (glk_library))
                {
                  is_loaded = TRUE;
                  ifp_trace ("main:"
                             " glk library loaded from '%s'", glk_library);
                  break;
                }
            }
          else
            {
              if (!search_path)
                search_path = ifp_manager_get_plugin_path ();

              if (ifp_main_load_glk_from_path (glk_library, search_path))
                {
                  is_loaded = TRUE;
                  ifp_trace ("main:"
                             " glk library loaded from search path");
                  break;
                }
            }
         }
    }

  ifp_free_split_string (elements, count);

  if (!is_loaded)
    ifp_trace ("main: no glk library loaded");

  return is_loaded;
}


/*
 * main()
 *
 * Program main entry point.  Selects and loads the appropriate Glk library
 * DSO, then calls its contained main().
 */
int
main (int argc, char *argv[])
{
  const char *glk_libraries;
  char **main_argv;
  int main_argc, status;
  int (*so_main) (int, char *[]);

  /* Set up any initial configuration file conditions. */
  ifp_config_read ();

  /*
   * Choose Glk; allow -glk option, if first, to select directly.  If -glk
   * is given, construct a new argc and argv with -glk <value> removed.  If
   * not, just use the normal argc and argv.
   */
  if (argc > 2 && strcmp (argv[1], "-glk") == 0)
    {
      glk_libraries = argv[2];

      main_argc = argc - 2;
      main_argv = ifp_malloc ((main_argc + 1) * sizeof (*main_argv));
      main_argv[0] = argv[0];
      memcpy (main_argv + 1, argv + 3, (main_argc - 1) * sizeof (*main_argv));
      main_argv[main_argc] = NULL;
    }
  else
    {
      glk_libraries = ifp_main_get_glk_libraries ();
      main_argc = argc;
      main_argv = argv;
    }

  if (!ifp_main_load_glk (glk_libraries))
    {
      ifp_error ("main: no glk library was loaded while searching for '%s'",
                 glk_libraries);

      ifp_notice ("main: check your settings for %s and either %s or"
                  " the argument passed with '%s', and retry",
                  "IF_PLUGIN_PATH", "IFP_GLK_LIBRARIES", "-glk");

      if (main_argv != argv)
        ifp_free (main_argv);
      return EXIT_FAILURE;
    }

  so_main = ifp_glk_get_main ();
  if (!so_main)
    ifp_fatal ("main: glk loader returned no main() function");

  ifp_trace ("main: calling glk main function");
  status = so_main (main_argc, main_argv);
  ifp_trace ("main: glk main function returned status %d", status);

  if (main_argv != argv)
    ifp_free (main_argv);

  ifp_glk_unload_interface ();
  return status;
}
