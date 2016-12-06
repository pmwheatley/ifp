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
#include <signal.h>
#include <limits.h>
#include <string.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Forward declaration for the signal handler function. */
static void ifp_finalizer_signal_catcher (int signum);

/* Array of prior signal handler functions, to call when we intercept them. */
static void (*prior_handlers[NSIG]) (int);


/*
 * ifp_install_handler()
 *
 * Install a new signal handler.  If we find that we have intercepted an
 * existing handler, note it so we can be sure to call it when we see that
 * signal.
 */
static void
ifp_install_handler (int signum, void (*handler) (int))
{
  struct sigaction sa, old_sa;

  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = handler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction (signum, &sa, &old_sa) == -1)
    {
      ifp_error ("finalizer: failed to install signal %d handler", signum);
      return;
    }

  if (!(old_sa.sa_handler == SIG_DFL || old_sa.sa_handler == SIG_IGN))
    {
      ifp_trace ("finalizer: saving existing signal %d handler", signum);
      assert (signum >= 0 && signum < NSIG);
      prior_handlers[signum] = old_sa.sa_handler;
    }
}


/*
 * ifp_register_finalizer()
 *
 * Register a finalizer function to be called on program exit.  This is the
 * weak version, and is the symbol that is resolved to outside of shared
 * library builds.
 *
 * This function behaves as atexit(), with the additional behaviour that it
 * adds some signal handers so that on catching a normally fatal signal,
 * exit() is called so that atexit() functions run.
 */
__attribute__ ((weak))
void
ifp_register_finalizer (void (*finalizer) (void))
{
  static int initialized = FALSE;
  static const int SIGNALS[] = { SIGHUP, SIGINT, SIGTERM, 0 };
  assert (finalizer);

  ifp_trace ("finalizer: ifp_register_finalizer [main version] <-"
             " function_%p", ifp_trace_pointer (finalizer));

  if (!initialized)
    {
      int index_;

      for (index_ = 0; SIGNALS[index_] != 0; index_++)
        ifp_install_handler (SIGNALS[index_], ifp_finalizer_signal_catcher);

      ifp_trace ("finalizer: installed finalizer signal handlers");
      initialized = TRUE;
    }

  atexit (finalizer);
}


/*
 * ifp_finalizer_signal_catcher()
 *
 * This function is set up to catch fatal program signals.  Its job is to
 * call exit(), and thus ensure that our registered atexit() functions get
 * called before the program truly exits.  Just before it does this, it will
 * also call any prior signal handler that was registered for the given
 * signal before we registered ours.
 */
static void
ifp_finalizer_signal_catcher (int signum)
{
  ifp_trace ("finalizer: caught signal number %d", signum);

  assert (signum >= 0 && signum < NSIG);
  if (prior_handlers[signum])
    {
      ifp_trace ("finalizer: calling prior handler for signal %d", signum);
      prior_handlers[signum] (signum);
    }

  /*
   * Call exit(), to execute everything we've seen come through here as a
   * registered finalizer, and thus passed on to atexit()...
   */
  ifp_trace ("finalizer: calling exit");
  exit (1);
}
