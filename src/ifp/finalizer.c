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
#include <signal.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Finalizer list entry definition, and array of entries, initially NULL. */
struct ifp_finalizer
{
  void (*finalizer) (void);
  struct ifp_finalizer *next;
}
;
typedef struct ifp_finalizer *ifp_finalizerref_t;
static ifp_finalizerref_t ifp_finalizer_list = NULL;


/*
 * ifp_register_finalizer()
 *
 * Register a finalizer function to be called when the library is unloaded.
 * This is the strong function.  If the code that wants to register a
 * finalization function is running in a shared object, it will call this
 * version.
 *
 * Finalization in a shared object is provided by the ifpi_finalizer()
 * function, IFP's analog to _fini.  It's always called when a shared library
 * unloads.
 * 
 * Chaining plugins link to this version of the function, ensuring that they
 * clean up after themselves on all forms of unload.  A finalizer function
 * behaves as atexit(), but activates when a library is unloaded, rather than
 * on process exit.
 */
void
ifp_register_finalizer (void (*finalizer) (void))
{
  ifp_finalizerref_t entry;

  entry = malloc (sizeof (*entry));
  if (!entry)
    {
      fprintf (stderr,
               "IFP plugin library error: finalizer: out of system memory");
      raise (SIGABRT);
      _exit (1);
    }

  entry->finalizer = finalizer;
  entry->next = ifp_finalizer_list;
  ifp_finalizer_list = entry;
}


/*
 * ifpi_initializer()
 *
 * Currently there's no action taken on shared library load.
 */
void
ifpi_initializer (void)
{
}


/*
 * ifpi_finalizer()
 *
 * Called whenever a shared object is unloaded.  This checks for registered
 * finalizers, and runs any listed.
 */
void
ifpi_finalizer (void)
{
  ifp_finalizerref_t entry;

  /*
   * Run cleanup functions in reverse order from their order of registry.
   * Running in reverse matches expected atexit() behaviour, and occurs
   * courtesy of having added new items to the head of the list.
   */
  for (entry = ifp_finalizer_list; entry; entry = ifp_finalizer_list)
    {
      entry->finalizer ();

      ifp_finalizer_list = entry->next;
      free (entry);
    }
}
