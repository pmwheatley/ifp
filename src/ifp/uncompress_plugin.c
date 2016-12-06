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
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Temporary file template. */
static const char *TMPFILE_TEMPLATE = "/tmp/ifp_uncompress_XXXXXX";

/*
 * The name of the temporary file we currently own.  NULL implies no
 * pending temporary file.
 */
static char *ifp_tmpfile_name = NULL;

/*
 * A state variable for checking that calls come in sequence, and that we
 * don't get activated twice.  This helps to check for areas where the
 * loader or manager fail to unload all instances of a plugin correctly.
 */
static enum
{ READY = 0, IN_STARTUP, INITIALIZED, IN_MAIN, DEAD }
ifp_plugin_state = READY;

/*
 * This is a filter plugin.  It accepts gzip'ed, compressed, packed, or
 * bzip2'ed files, expands them into a temporary location using the appropriate
 * tool, the scans for any other plugin (or even maybe another copy of this
 * one...) able to accept the contents.  If one is prepared to accept the
 * expanded contents, that plugin is chained to this one.
 *
 * File types are identified from the first three bytes in the file.  For
 * bzip2'ed files, the contents are "BZh".  For others, only the first two
 * bytes are used, with the first being 0x1f, and the second being 0x8b
 * for gzip, 0x9d for plain old compress, 0xa0 for Huffman compressed, and
 * 0x1e for crufty old packed files.  Gunzip handles all formats for us
 * except bzip2'ed files, which we handle with bunzip2.
 */
struct ifp_header ifpi_header = {
  .version = IFP_HEADER_VERSION,
  .build_timestamp = __DATE__ ", " __TIME__,

  .engine_type = "Uncompress",
  .engine_name = "Uncompress",
  .engine_version = "0.0.5",

  .acceptor_offset = 0,
  .acceptor_length = 3,
  .acceptor_pattern = "^(42 5a 68|1f (8b|9d|a0|1e) ..)$",

  .author_name = "Simon Baldwin",
  .author_email = "simon_baldwin@yahoo.com",

  .engine_description =
    "This plugin accepts gzipped files (.gz), compressed files (.Z),"
    " packed files (.z), or bzipped files (.bz2), and plays any"
    " playable Interactive Fiction game in the uncompressed output"
    " file.\n",
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
 * ifp_uncompress_cleanup()
 *
 * Small handler function we register with the finalizer, called whenever
 * the plugin is unloaded.  Normally, we'd have finished running our
 * chained plugin, so would have nothing to do.  If however we find that
 * we still own the temporary file, now is the time to delete it; this
 * happens if the main program terminates unexpectedly.
 */
static void
ifp_uncompress_cleanup (void)
{
  ifp_trace ("uncompress: ifp_uncompress_cleanup <- void");

  /*
   * If it looks like we still hold the temporary file, this is our last
   * chance to remove it.  While we're about it, free filename malloced memory.
   */
  if (ifp_tmpfile_name)
    {
      ifp_trace ("uncompress: unlinking '%s'", ifp_tmpfile_name);
      unlink (ifp_tmpfile_name);

      ifp_free (ifp_tmpfile_name);
      ifp_tmpfile_name = NULL;
    }

  ifp_chain_set_chained_plugin (NULL);
  ifp_plugin_state = DEAD;
}


/*
 * ifp_uncompress_uncompress_file()
 *
 * Given two file descriptors, expand the data from the first into the
 * second using the appropriate expander tool (gzip or bzip2 with the
 * -d option).
 */
static int
ifp_uncompress_uncompress_file (int infile, int outfile)
{
  char header[3];
  int pid, status;
  enum { NONE = 0, GZIP, BZIP2 } helper = NONE;

  ifp_trace ("uncompress:"
             " ifp_uncompress_uncompress_file <- %d %d", infile, outfile);

  /* Re-read the acceptor for the file, then decide between gzip and bzip2. */
  if (lseek (infile, 0, SEEK_SET) == -1
      || read (infile, header, sizeof (header)) != sizeof (header)
      || lseek (infile, 0, SEEK_SET) == -1)
    {
      ifp_error ("uncompress: unable to read header data in file");
      return FALSE;
    }

  if (ifp_recognizer_match_binary
          (header, sizeof (header), "^1f (8b|9d|a0|1e) ..$"))
    helper = GZIP;

  else if (ifp_recognizer_match_binary
              (header, sizeof (header), "^42 5a 68$"))
    helper = BZIP2;

  else
    {
      ifp_error ("uncompress: unanticipated magic data in file");
      return FALSE;
    }

  pid = fork ();
  if (pid == -1)
    {
      ifp_error ("uncompress: unable to start a child process");
      return FALSE;

    }

  if (pid == 0)
    {
      /*
       * Child process.  Duplicate file descriptors, then exec the appropriate
       * helper program.
       */
      if (dup2 (infile, 0) == -1 || dup2 (outfile, 1) == -1)
        {
          ifp_error ("uncompress: unable to dup stdin or stdout");
          exit (127);
        }

      switch (helper)
        {
        case GZIP:
          execlp ("gzip", "gzip", "-dc", NULL);
          ifp_error ("uncompress: unable to execute the 'gzip' program");
          break;

        case BZIP2:
          execlp ("bzip2", "bzip2", "-dc", NULL);
          ifp_error ("uncompress: unable to execute the 'bzip2' program");
          break;

        default:
          ifp_fatal ("uncompress: invalid helper program requested");
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
            case GZIP:
              ifp_error ("uncompress: error waiting for the 'gzip' program");
              break;

            case BZIP2:
              ifp_error ("uncompress: error waiting for the 'bzip2' program");
              break;

            default:
              ifp_fatal ("uncompress: invalid helper program requested");
            }
          return FALSE;
        }
    }

  if (WIFEXITED (status))
    {
      switch (helper)
        {
        case GZIP:
          if (WEXITSTATUS (status) == 2)
            {
              ifp_notice ("uncompress:"
                          " extraction problem, %d", WEXITSTATUS (status));
              ifp_notice ("uncompress: continuing anyway...");
            }
          else if (WEXITSTATUS (status) != 0)
            {
              ifp_error ("uncompress:"
                         " uncompression failed, %d", WEXITSTATUS (status));
              return FALSE;
            }
          break;

        case BZIP2:
          if (WEXITSTATUS (status) != 0)
            {
              ifp_error ("uncompress: uncompression failed, %d",
                         WEXITSTATUS (status));
              return FALSE;
            }
          break;

        default:
          ifp_fatal ("uncompress: invalid helper program requested");
          break;
        }
    }
  else if (WIFSIGNALED (status))
    {
      ifp_error ("uncompress:"
                 " helper program caught signal, %d", WTERMSIG (status));
      return FALSE;
    }

  ifp_trace ("uncompress: ifp_uncompress_uncompress_file succeeded");
  return TRUE;
}


/*
 * ifpi_glkunix_startup_code()
 *
 * Called by the main program with the details of the file to uncompress.
 * We'll expand it into a temporary location, then see if we can find
 * another plugin to handle the uncompressed data.
 */
int
ifpi_glkunix_startup_code (glkunix_startup_t *data)
{
  const char *infilename;
  char *tmpfilename;
  int tmpfile_, infile;
  ifp_pluginref_t plugin;
  assert (data);

  ifp_trace ("uncompress: ifpi_glkunix_startup_code <-"
             " startup_%p", ifp_trace_pointer (data));

  assert (ifp_plugin_state == READY);
  ifp_plugin_state = IN_STARTUP;

  /* If we already hold a chained plugin, something is horribly wrong. */
  if (ifp_chain_get_chained_plugin ())
    {
      ifp_error ("uncompress: already busy with a chained plugin");
      ifp_plugin_state = DEAD;
      return FALSE;
    }

  ifp_register_finalizer (ifp_uncompress_cleanup);

  /* Get the file we've been asked to uncompress. */
  infilename = data->argv[data->argc - 1];

  tmpfilename = ifp_malloc (strlen (TMPFILE_TEMPLATE) + 1);
  strcpy (tmpfilename, TMPFILE_TEMPLATE);
  tmpfile_ = mkstemp (tmpfilename);
  if (tmpfile_ == -1)
    {
      ifp_error ("uncompress:"
                 " error creating temporary file '%s'", tmpfilename);
      unlink (tmpfilename);
      ifp_free (tmpfilename);
      ifp_plugin_state = DEAD;
      return FALSE;
    }
  ifp_trace ("uncompress: temporary file is '%s'", tmpfilename);

  /* Open the input data file, then pass to the archive extractor function. */
  infile = open (infilename, O_RDONLY);
  if (infile == -1)
    {
      ifp_error ("uncompress: error opening file '%s'", infilename);
      close (tmpfile_);
      unlink (tmpfilename);
      ifp_free (tmpfilename);
      ifp_plugin_state = DEAD;
      return FALSE;
    }

  if (!ifp_uncompress_uncompress_file (infile, tmpfile_))
    {
      ifp_error ("uncompress: unable to uncompress input file");
      close (infile);
      close (tmpfile_);
      unlink (tmpfilename);
      ifp_free (tmpfilename);
      ifp_plugin_state = DEAD;
      return FALSE;
    }

  close (infile);
  close (tmpfile_);

  /* Try to locate a plugin that can handle the result of uncompressing. */
  plugin = ifp_manager_locate_plugin (tmpfilename);
  if (!plugin)
    {
      ifp_notice ("uncompress:"
                  " no plugin found for the contents of '%s'", infilename);
      unlink (tmpfilename);
      ifp_free (tmpfilename);

      /*
       * No plugin found for the file, so empty our loader instance.  It's
       * important to do this, because otherwise we'll finish up with
       * multiple active handles to plugin shared objects.
       */
      ifp_loader_forget_all_plugins ();

      ifp_plugin_state = DEAD;
      return FALSE;
    }

  ifp_trace ("uncompress:"
             " using chain plugin_%p, tmpfile '%s'",
             ifp_trace_pointer (plugin), tmpfilename);

  ifp_chain_set_chained_plugin (plugin);
  ifp_tmpfile_name = tmpfilename;
  ifp_plugin_state = INITIALIZED;

  return TRUE;
}


/*
 * ifpi_glk_main()
 *
 * Provided we are chaining a plugin, call its glk_main.  When done, unload
 * it, since nobody else will, unlink temporary files and free any filename
 * memory, and wait to be unloaded ourselves.
 */
void
ifpi_glk_main (void)
{
  ifp_pluginref_t plugin;

  ifp_trace ("uncompress: ifpi_glk_main <- void");

  assert (ifp_plugin_state == INITIALIZED);
  ifp_plugin_state = IN_MAIN;

  /* If no chained plugin to run for us, something nasty is going on. */
  plugin = ifp_chain_get_chained_plugin ();
  if (!plugin)
    {
      ifp_error ("uncompress: no chained plugin set");
      ifp_plugin_state = DEAD;
      return;
    }

  ifp_trace ("uncompress:"
             " calling the manager run on chain"
             " plugin_%p", ifp_trace_pointer (plugin));
  ifp_manager_run_plugin (plugin);

  /* Take the plugin out of our loader instance, and clear our note of it. */
  ifp_trace ("uncompress:"
             " forgetting chain plugin_%p", ifp_trace_pointer (plugin));
  ifp_loader_forget_plugin (plugin);
  ifp_chain_set_chained_plugin (NULL);

  ifp_loader_forget_all_plugins ();

  unlink (ifp_tmpfile_name);
  ifp_free (ifp_tmpfile_name);
  ifp_tmpfile_name = NULL;

  ifp_trace ("uncompress: returning from ifpi_glk_main");
  ifp_plugin_state = DEAD;
}
