/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2006-2007  Simon Baldwin (simon_baldwin@yahoo.com)
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

#define _GNU_SOURCE       /* For RTLD_DEFAULT and RTLD_NEXT from dlfcn.h. */
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>

#include <glk.h>
#include <ifp.h>

#include "protos.h"


/* Refresh progress every .5 seconds (100mS * 5). */
static const int TIMEOUT_BASELINE = 100, URL_PAUSE_TIMEOUT = 5;

/* Pointers to the real libc sigaction and local libifppi sigaction stub. */
typedef int (*sigaction_funcptr_t)
            (int, const struct sigaction *, struct sigaction *);
static sigaction_funcptr_t real_sigaction, local_sigaction;

/*
 * Copy of the sigaction struct on receiving a call to __wrap_sigaction(),
 * and a note of whether it needs restoring on cleanup or not.
 */
struct sigaction original_sigaction;
static int is_sigaction_restore_required = FALSE;

/* Nugatory declaration to silence gcc compiler warning. */
int __wrap_sigaction (int signum,
                      const struct sigaction *act, struct sigaction *oldact);


/*
 * __wrap_sigaction()
 *
 * This is a wrapper function for sigaction, activated by using the --wrap
 * option to ld.  It's a nasty piece of subversion.  Here's the deal.  The
 * main IFP library contains a SIGIO driven URL resolver.  It wants to
 * maintain complete control of SIGIO, for safety.  IFP ensures this by
 * trapping and erroring calls to sigaction() made from any plugin.
 *
 * However... in Gamebox, we want to be able to use our own SIGIO driven
 * URL resolver, from our contained libifp.  To have that happen, we need
 * to be able to call the real sigaction() to set up our libifp's SIGIO
 * handler in the system.  What we do, then, is to wrap sigaction with the
 * linker so we catch calls to it here.  On receiving a call, we retrieve
 * and call the real libc sigaction to both set the requested handler and
 * retrieve the one we're overwriting.  We store the latter safely, and
 * then return an old sigaction that pretends that nothing was overwritten
 * (something was, but doing this suppresses a local libifp warning, one
 * that's ignorable because we're going to put everything back later).
 *
 * On plugin exit, we'll want to put all this back like it was so that the
 * main program's URL resolver continues to function.
 */
int
__wrap_sigaction (int signum,
                  const struct sigaction *act, struct sigaction *oldact)
{
  int status;

  /* If not yet done, find and cache the real and stub sigaction addresses. */
  if (!real_sigaction)
    real_sigaction = (sigaction_funcptr_t) dlsym (RTLD_NEXT, "sigaction");
  if (!local_sigaction)
    local_sigaction = (sigaction_funcptr_t) dlsym (RTLD_DEFAULT, "sigaction");

  /*
   * We _really_ expect only SIGIO; if not, or if we somehow have no real
   * sigaction available (highly unlikely!), pretend it never happened.
   * This will probably lead to the libifppi stub/rejecting sigaction().
   */
  if (signum != SIGIO || !real_sigaction)
    {
      assert (local_sigaction);
      return local_sigaction (signum, act, oldact);
    }

  /* Switch SIGIO handlers, saving the currently set one for later. */
  status = real_sigaction (signum, act, &original_sigaction);
  if (status != 0)
    return status;

  /* Finally, set up oldact as if no handler was overwritten. */
  oldact->sa_handler = SIG_DFL;
  sigemptyset (&oldact->sa_mask);
  oldact->sa_flags = 0;
  oldact->sa_restorer = NULL;

  is_sigaction_restore_required = TRUE;
  return status;
}


/*
 * url_cleanup()
 *
 * If we intercepted and installed our own SIGIO handler, put everything
 * back as it was when we got our first call, and clean up module variables
 * by resetting to initial values.
 */
void
url_cleanup (void)
{
  if (is_sigaction_restore_required)
    {
      int status;
      assert (real_sigaction);

      status = (*real_sigaction) (SIGIO, &original_sigaction, NULL);
      assert (status == 0);

      real_sigaction = NULL;
      local_sigaction = NULL;
      is_sigaction_restore_required = FALSE;
    }
}


/*
 * url_resolve()
 *
 * Resolve a URL, printing regular status update messages where the download
 * is lengthy.  If there's a problem, url_errno is handed the details.  A
 * keypress can interrupt the download.  If interrupted, url_errno is set
 * explicity to EINTR.  Interrupts are enabled only for Glk libraries that
 * offer timers (to exclude cheapglk).
 */
ifp_urlref_t
url_resolve (winid_t window, const char *url_path, int *url_errno)
{
  ifp_urlref_t url;
  int url_progress, url_status;
  int has_timers, is_interrupted = FALSE;

  has_timers = glk_gestalt (gestalt_Timer, 0);

  /* Start a new asynchronous URL. */
  url = ifp_url_new_resolve_async (url_path);
  if (!url)
    {
      *url_errno = errno;
      return NULL;
    }

  /* Detect cancellation requests only for timer-aware Glks. */
  if (has_timers)
    glk_request_char_event (window);

  /* Report progress periodically until interrupted or resolved. */
  url_progress = 0;
  while (!is_interrupted && !ifp_url_poll_resolved_async (url))
    {
      event_t event;
      int progress;

      glk_select_poll (&event);
      if (event.type == evtype_Arrange || event.type == evtype_Redraw)
        display_handle_redraw ();

      is_interrupted |= (event.type == evtype_CharInput);

      progress = ifp_url_poll_progress_async (url);

      if (progress > url_progress
          || event.type == evtype_Arrange || event.type == evtype_Redraw)
        {
          message_write_line (TRUE,
                              "Downloading, %d bytes...%s", progress,
                              has_timers ? " [hit any key to cancel]" : "");
          url_progress = progress;
        }

      /*
       * If we have Glk timers, use those to wait in between URL polls.
       * This both keeps the display refreshed and avoids breakage in
       * at least one Glk library's glk_select_poll() (you know who you
       * are!).  If no Glk timers, fall back to using a URL pause with
       * a prior glk_select_poll() in hopes of keeping the display fresh.
       */
      if (has_timers)
        {
          int timeouts;

          timeouts = 0;
          glk_request_timer_events (TIMEOUT_BASELINE);
          do
            {
              glk_select (&event);
              if (event.type == evtype_Arrange || event.type == evtype_Redraw)
                display_handle_redraw ();

              is_interrupted |= (event.type == evtype_CharInput);
              if (event.type == evtype_Timer)
                timeouts++;
            }
          while (timeouts < URL_PAUSE_TIMEOUT);
          glk_request_timer_events (0);
        }
      else
        {
          glk_select_poll (&event);
          if (event.type == evtype_Arrange || event.type == evtype_Redraw)
            display_handle_redraw ();

          is_interrupted |= (event.type == evtype_CharInput);
          ifp_url_pause_async (url);
        }

      /*
       * If interrupt requested, confirm download cancel.  If confirmed,
       * return NULL with EINTR, otherwise, reset flag, re-request character
       * events, and continue as if nothing happened.
       */
      if (is_interrupted)
        {
          if (message_confirm ("Cancel download? [y/n]"))
            {
              *url_errno = EINTR;
              ifp_url_forget (url);
              return NULL;
            }
          else
            {
              is_interrupted = FALSE;
              glk_request_char_event (window);
            }
        }
    }

  glk_cancel_char_event (window);
  message_end_dialog ();

  /* If the URL failed to resolve, destroy it and return NULL. */
  url_status = ifp_url_get_status_async (url);
  if (url_status != 0)
    {
      ifp_url_forget (url);
      *url_errno = url_status;
      return NULL;
    }

  return url;
}
