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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Temporary file template. */
static const char *TMPFILE_TEMPLATE = "/tmp/ifp_unarchive_XXXXXX";

/*
 * The name of the temporary directory into which we extract archive data,
 * and the name of the actual game file that the chained plugin is working on.
 */
static char *ifp_tmpdir_name = NULL,
            *ifp_gamefile_name = NULL;

/*
 * A state variable for checking that calls come in sequence, and that we
 * don't get activated twice.  This helps to check for areas where the
 * loader or manager fail to unload all instances of a plugin correctly.
 */
static enum
{ READY = 0, IN_STARTUP, INITIALIZED, IN_MAIN, DEAD }
ifp_plugin_state = READY;

/*
 * This is a filter plugin.  It accepts tar, cpio, and pkzip archives, makes
 * a temporary directory, extracts the archive into that directory, and then
 * searches for the first file in the archive that is runnable as a game.
 * It then chains the relevant plugin for that file.
 *
 * The first-file-to-match thing is a little arbitrary.  In practice, then,
 * it's best if the archive contains only one real runnable game, the rest
 * of the files being supporting information (Alan games are like this,
 * being composed of game.acd, and game.dat - a Zip archive containing just
 * these two files would be a good target for this plugin, and indeed, many
 * Alan games seem to come pretty much like this).
 */
struct ifp_header ifpi_header = {
  .version = IFP_HEADER_VERSION,
  .build_timestamp = __DATE__ ", " __TIME__,

  .engine_type = "Unarchive",
  .engine_name = "Unarchive",
  .engine_version = "0.0.5",

  .acceptor_offset = 0,
  .acceptor_length = 262,
  .acceptor_pattern = "^(30 37 30 37 30 3[127] .*|"    /* ASCII cpio */
                      "c7 71 .*|"                      /* Non-ASCII cpio */
                      ".* 75 73 74 61 72|"             /* Tar ("ustar") */
                      "50 4b .*|"                      /* Pkzip */
                      "21 3c 61 72 63 68 3e 0a .*)$",  /* Ar ("!<arch>\n") */

  .author_name = "Simon Baldwin",
  .author_email = "simon_baldwin@yahoo.com",

  .engine_description =
    "This plugin accepts tar, cpio, pkzip, and ar archives (by convention,"
    " .tar, .cpio, .zip, or .a files), and plays the first Interactive"
    " Fiction game file it finds in the extracted archive contents.\n",
  .engine_copyright =
    "Copyright (C) 2001-2007  Simon Baldwin (simon_baldwin@yahoo.com)\n"
    "This program is free software; you can redistribute it and/or"
    " modify it under the terms of the GNU General Public License"
    " as published by the Free Software Foundation; either version 2"
    " of the License, or (at your option) any later version.\n"
};

glkunix_argumentlist_t ifpi_glkunix_arguments[] = {
  {.name = NULL, .argtype = glkunix_arg_End, .desc = NULL}
};


/*
 * ifp_unarchive_rm_rf()
 *
 * Make an attempt to recursively remove a directory.  In practice it's not
 * a really good effort - we just try to run the 'rm -rf' command on it.
 * If it fails, we take scant notice, but at least we tried.
 */
static void
ifp_unarchive_rm_rf (const char *directory)
{
  char command[64];

  ifp_trace ("unarchive: ifp_unarchive_rm_rf <- '%s'", directory);

  if (strncmp (directory, "/tmp/", 5) != 0)
    ifp_fatal ("unarchive:"
               " cowardly refusal to delete outside /tmp, '%s'", directory);

  snprintf (command, sizeof (command), "/bin/rm -rf %s", directory);
  if (system (command) != 0)
    {
      ifp_error ("unarchive:"
                 " temporary directory may not be deleted, '%s'", directory);
    }
}


/*
 * ifp_unarchive_cleanup()
 *
 * Small handler function we register with the finalizer, called whenever
 * the plugin is unloaded.  Normally, we'd have finished running our
 * chained plugin, so would have nothing to do.  If however we find that
 * we still own the temporary directory, now is the time to delete it; this
 * happens if the main program terminates unexpectedly.
 */
static void
ifp_unarchive_cleanup (void)
{
  ifp_trace ("unarchive: ifp_unarchive_cleanup <- void");

  /*
   * If it looks like we still hold the temporary directory, this is our last
   * chance to remove it.  While we're about it, free filename malloced memory.
   */
  if (ifp_tmpdir_name)
    {
      ifp_trace ("unarchive: deleting '%s'", ifp_tmpdir_name);
      ifp_unarchive_rm_rf (ifp_tmpdir_name);

      ifp_free (ifp_tmpdir_name);
      ifp_tmpdir_name = NULL;
    }

  ifp_free (ifp_gamefile_name);
  ifp_gamefile_name = NULL;

  ifp_chain_set_chained_plugin (NULL);
  ifp_plugin_state = DEAD;
}


/*
 * ifp_unarchive_extract()
 *
 * Given an opened file descriptor, the name of the archive file opened on
 * that descriptor, and a directory, cd to the directory and extract the
 * archive there using the appropriate helper.
 */
static int
ifp_unarchive_extract (int infile,
                       const char *archive, const char *directory)
{
  char header[262];
  int pid, status;
  enum { NONE = 0, ASCII_CPIO, CPIO, TAR, UNZIP, AR } helper = NONE;

  ifp_trace ("unarchive: ifp_unarchive_extract <- %d '%s' '%s'",
             infile, archive, directory);

  /* Re-read the acceptor for the file, then decide on a helper program. */
  if (lseek (infile, 0, SEEK_SET) == -1
      || read (infile, header, sizeof (header)) != sizeof (header)
      || lseek (infile, 0, SEEK_SET) == -1)
    {
      ifp_error ("unarchive: unable to read header data in file");
      return FALSE;
    }

  if (ifp_recognizer_match_binary
          (header, sizeof (header), "^30 37 30 37 30 3[127] .*$"))
    helper = ASCII_CPIO;

  else if (ifp_recognizer_match_binary
              (header, sizeof (header), "^c7 71 .*$"))
    helper = CPIO;

  else if (ifp_recognizer_match_binary
              (header, sizeof (header), "^.* 75 73 74 61 72$"))
    helper = TAR;

  else if (ifp_recognizer_match_binary
              (header, sizeof (header), "^50 4b .*$"))
    helper = UNZIP;

  else if (ifp_recognizer_match_binary
              (header, sizeof (header), "^21 3c 61 72 63 68 3e 0a .*$"))
    helper = AR;

  else
    {
      ifp_error ("unarchive: unanticipated magic data in file");
      return FALSE;
    }

  pid = fork ();
  if (pid == -1)
    {
      ifp_error ("unarchive: unable to start a child process");
      return FALSE;

    }

  if (pid == 0)
    {
      /*
       * Child process.  Duplicate file descriptors and chdir if required,
       * then exec the appropriate helper program.
       */
      if (helper == ASCII_CPIO || helper == CPIO || helper == TAR)
        {
          if (dup2 (infile, 0) == -1)
            {
              ifp_error ("unarchive: unable to dup stdin");
              exit (127);
            }
        }

      if (helper == ASCII_CPIO || helper == CPIO || helper == AR)
        {
          if (chdir (directory) == -1)
            {
              ifp_error ("unarchive: unable to chdir");
              exit (127);
            }
        }

      switch (helper)
        {
        case ASCII_CPIO:
          execlp ("cpio", "cpio", "--quiet", "-ic", NULL);
          ifp_error ("unarchive: unable to execute the 'cpio' program");
          break;

        case CPIO:
          execlp ("cpio", "cpio", "--quiet", "-i", NULL);
          ifp_error ("unarchive: unable to execute the 'cpio' program");
          break;

        case TAR:
          execlp ("tar", "tar", "-C", directory, "-xf", "-", NULL);
          ifp_error ("unarchive: unable to execute the 'tar' program");
          break;

        case UNZIP:
          execlp ("unzip", "unzip", "-d", directory, "-bLoqq", archive, NULL);
          ifp_error ("unarchive: unable to execute the 'unzip' program");
          break;

        case AR:
          execlp ("ar", "ar", "-x", archive, NULL);
          ifp_error ("unarchive: unable to execute the 'ar' program");
          break;

        default:
          ifp_fatal ("unarchive: invalid helper program requested");
          break;
        }
      exit (127);
    }

  /* Parent process.  Wait for child, then handle its termination. */
  while (waitpid (pid, &status, 0) == -1)
    {
      if (errno != EINTR)
        {
          switch (helper)
            {
            case ASCII_CPIO:
            case CPIO:
              ifp_error ("unarchive: error waiting for the 'cpio' program");
              break;

            case TAR:
              ifp_error ("unarchive: error waiting for the 'tar' program");
              break;

            case UNZIP:
              ifp_error ("unarchive: error waiting for the 'unzip' program");
              break;

            case AR:
              ifp_error ("unarchive: error waiting for the 'ar' program");
              break;

            default:
              ifp_fatal ("unarchive: invalid helper program requested");
            }
          return FALSE;
        }
    }

  if (WIFEXITED (status))
    {
      /*
       * Unzip defines an unorthodox set of exit status codes.  Here, we'll
       * try to map these onto success, warnings, and errors.
       */
      if (helper == UNZIP)
        {
          switch (WEXITSTATUS (status))
            {
            case 0:
            case 1:
              break;
            case 2:
            case 9:
            case 11:
            case 80:
            case 81:
            case 82:
              ifp_notice ("unarchive:"
                          " 'unzip' returned status %d", WEXITSTATUS (status));
              ifp_notice ("unarchive: continuing anyway...");
              break;
            default:
              ifp_error ("unarchive:"
                         " extraction failed, %d", WEXITSTATUS (status));
              return FALSE;
            }
        }
      else
        {
          if (WEXITSTATUS (status) != 0)
            {
              ifp_error ("unarchive:"
                         " extraction failed, %d", WEXITSTATUS (status));
              return FALSE;
            }
        }
    }
  else if (WIFSIGNALED (status))
    {
      ifp_error ("unarchive:"
                 " helper program caught signal, %d", WTERMSIG (status));
      return FALSE;
    }

  ifp_trace ("unarchive: ifp_unarchive_extract succeeded");
  return TRUE;
}


/*
 * ifp_unarchive_filter_directory()
 * ifp_unarchive_scan_directory()
 *
 * Scan the given directory for the first file that a plugin recognizes.
 * Return the plugin that accepts it, or NULL if none.  The function also
 * returns the file that the game lives in, so that the string form of
 * it won't be freed until the chained plugin has completed.
 */
static int
ifp_unarchive_filter_directory (const struct dirent *entry)
{
  return !(strcmp (entry->d_name, ".") == 0
           || strcmp (entry->d_name, "..") == 0);
}

static ifp_pluginref_t
ifp_unarchive_scan_directory (const char *directory_path, char **gamefile)
{
  struct dirent **entries;
  int filenames, index_;
  ifp_pluginref_t result;
  char *path;

  ifp_trace ("unarchive:"
             " ifp_unarchive_scan_directory <- '%s'", directory_path);

  /* Scan the given directory, and accumulate entries of interest. */
  filenames = scandir (directory_path,
                       &entries, ifp_unarchive_filter_directory, NULL);
  if (filenames == -1)
    {
      ifp_error ("unarchive:"
                 " error scanning directory '%s'", directory_path);
      return NULL;
    }

  result = NULL;
  path = NULL;

  /* Search the directory files for anything a plugin will accept. */
  for (index_ = 0; index_ < filenames; index_++)
    {
      const char *filename;
      int allocation;
      ifp_pluginref_t plugin;

      filename = entries[index_]->d_name;
      ifp_trace ("unarchive: considering file '%s'", filename);

      allocation = strlen (directory_path) + strlen (filename) + 2;
      path = ifp_malloc (allocation);
      snprintf (path, allocation, "%s/%s", directory_path, filename);

      /* If a plugin will handle this file, return it. */
      plugin = ifp_manager_locate_plugin (path);
      if (plugin)
        {
          ifp_trace ("unarchive: chaining to plugin"
                     " plugin_%p", ifp_trace_pointer (plugin));
          result = plugin;
          break;
        }

      ifp_trace ("unarchive: no plugin accepted this file");
      ifp_free (path);
      path = NULL;
    }

  /* Free each individual entry, then the entries array itself. */
  for (index_ = 0; index_ < filenames; index_++)
    free (entries[index_]);
  free (entries);

  if (!result)
    {
      /*
       * At this point we might consider a recursive directory search, but
       * it's probably not a worthwhile effort for the present.
       */
      ifp_trace ("unarchive: no plugin matched any directory file");
      return NULL;
    }

  *gamefile = path;
  return result;
}


/*
 * ifpi_glkunix_startup_code()
 *
 * Called by the main program with the details of the archive to open and
 * extract.  We'll extract it into a directory in /tmp, then search the
 * directory for the first file that another plugin will run.
 */
int
ifpi_glkunix_startup_code (glkunix_startup_t *data)
{
  const char *archive;
  char *tmpdirname, *gamefile;
  int infile;
  ifp_pluginref_t plugin;
  assert (data);

  ifp_trace ("unarchive: ifpi_glkunix_startup_code <-"
             " startup_%p", ifp_trace_pointer (data));

  assert (ifp_plugin_state == READY);
  ifp_plugin_state = IN_STARTUP;

  /* If we already hold a chained plugin, something is horribly wrong. */
  if (ifp_chain_get_chained_plugin ())
    {
      ifp_error ("unarchive: already busy with a chained plugin");
      ifp_plugin_state = DEAD;
      return FALSE;
    }

  ifp_register_finalizer (ifp_unarchive_cleanup);

  /* Get the file we've been asked to extract from. */
  archive = data->argv[data->argc - 1];

  /* Create a temporary directory to expand files into. */
  tmpdirname = ifp_malloc (strlen (TMPFILE_TEMPLATE) + 1);
  strcpy (tmpdirname, TMPFILE_TEMPLATE);
  if (!mkdtemp (tmpdirname))
    {
      ifp_error ("unarchive:"
                 " error creating temporary directory '%s'", tmpdirname);
      ifp_free (tmpdirname);
      ifp_plugin_state = DEAD;
      return FALSE;
    }

  ifp_trace ("unarchive: temporary directory will be '%s'", tmpdirname);

  /*
   * Open the input data file, then pass both the opened file descriptor and
   * the path to the input data file to the archive extractor function.
   */
  infile = open (archive, O_RDONLY);
  if (infile == -1)
    {
      ifp_error ("unarchive: error opening file '%s'", archive);
      ifp_unarchive_rm_rf (tmpdirname);
      ifp_free (tmpdirname);
      ifp_plugin_state = DEAD;
      return FALSE;
    }

  if (!ifp_unarchive_extract (infile, archive, tmpdirname))
    {
      ifp_error ("unarchive: unable to uncompress input file");
      close (infile);
      ifp_unarchive_rm_rf (tmpdirname);
      ifp_free (tmpdirname);
      ifp_plugin_state = DEAD;
      return FALSE;
    }

  close (infile);

  /* Scan the directory we extracted into for games that a plugin accepts. */
  plugin = ifp_unarchive_scan_directory (tmpdirname, &gamefile);
  if (!plugin)
    {
      ifp_notice ("unarchive:"
                  " no plugin found for the contents of '%s'", tmpdirname);
      ifp_unarchive_rm_rf (tmpdirname);
      ifp_free (tmpdirname);

      /*
       * No plugin found for the file, so empty our loader instance.  It's
       * important to do this, because otherwise we'll finish up with
       * multiple active handles to plugin shared objects.
       */
      ifp_loader_forget_all_plugins ();

      ifp_plugin_state = DEAD;
      return FALSE;
    }

  ifp_trace ("unarchive:"
             " using chain plugin_%p, tmpdir '%s', game '%s'",
             ifp_trace_pointer (plugin), tmpdirname, gamefile);

  ifp_chain_set_chained_plugin (plugin);
  ifp_tmpdir_name = tmpdirname;
  ifp_gamefile_name = gamefile;
  ifp_plugin_state = INITIALIZED;

  return TRUE;
}


/*
 * ifpi_glk_main()
 *
 * Provided we are chaining a plugin, call its glk_main.  When done, unload
 * it, since nobody else will, delete the temporary directory and free any
 * filename memory, and wait to be unloaded ourselves.
 */
void
ifpi_glk_main (void)
{
  ifp_pluginref_t plugin;

  ifp_trace ("unarchive: ifpi_glk_main <- void");

  assert (ifp_plugin_state == INITIALIZED);
  ifp_plugin_state = IN_MAIN;

  /* If no chained plugin to run for us, something nasty is going on. */
  plugin = ifp_chain_get_chained_plugin ();
  if (!plugin)
    {
      ifp_error ("unarchive: no chained plugin set");
      ifp_plugin_state = DEAD;
      return;
    }

  ifp_trace ("unarchive:"
             " calling the manager run on chain"
             " plugin_%p", ifp_trace_pointer (plugin));
  ifp_manager_run_plugin (plugin);

  /* Take the plugin out of our loader instance, and clear our note of it. */
  ifp_trace ("unarchive:"
             " forgetting chain plugin_%p", ifp_trace_pointer (plugin));
  ifp_loader_forget_plugin (plugin);
  ifp_chain_set_chained_plugin (NULL);

  ifp_loader_forget_all_plugins ();

  ifp_free (ifp_gamefile_name);
  ifp_gamefile_name = NULL;

  /* Only delete the temporary directory if cleanup didn't run. */
  if (ifp_tmpdir_name)
    {
      ifp_unarchive_rm_rf (ifp_tmpdir_name);
      ifp_free (ifp_tmpdir_name);
      ifp_tmpdir_name = NULL;
    }

  ifp_trace ("unarchive: returning from ifpi_glk_main");
  ifp_plugin_state = DEAD;
}
