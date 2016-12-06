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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "ifp.h"
#include "ifp_internal.h"

/*
 * This module should never call malloc/free/strdup/open/close/... or any of
 * the libc (or Glk) functions that we intercept.  It may have to jump through
 * some hoops to avoid this, but it's important.
 *
 * On the plugin-side, strdup, say, is intercepted and fed to the client side.
 * That's fine.  However, what if we want to write a chaining plugin, something
 * that has both plugin and client libraries built in?  If we call strdup
 * from here, it'll go to the plugin-side strdup, in libc_proxy, wind up in
 * ifp_libc_intercept_strdup() after passing through the libc interface, and
 * guess what - ifp_libc_intercept_strdup() makes a call to the function
 * ifp_memory_malloc_add_address(), which calls ifp_trace(), which calls
 * ifp_trace_check_selector(), which calls strdup, and around we go...
 *
 * Of course, this shows up only when you turn tracing on, but it's unpleasant,
 * and renders the program useless.  Besides, tracing is allowed to be somewhat
 * inefficient - it's debug code, right?
 */

/*
 * Trace selector set by the direct interface, and flag indicating if error
 * messages are enabled.  Fatal errors are always printed, but "ordinary" ones
 * may be turned off.
 */
static const char *ifp_trace_selector = NULL;
static int ifp_error_messages = TRUE;


/**
 * ifp_trace_select()
 *
 * Set a tracing selector string.  This string augments any found in the
 * IFP_TRACE environment variable.
 */
void
ifp_trace_select (const char *selector)
{
  ifp_trace_selector = selector;
}


/*
 * ifp_trace_check_selector()
 *
 * Given a format string, find the facility from it (the bit up to the first
 * ':').  Search for this in the trace selector, and if found, return TRUE.
 */
static int
ifp_trace_check_selector (const char *selector, const char *format)
{
  char facility[1024], *facility_end;

  /* Special cases: NULL traces nothing, and "all"/"*" traces everything. */
  if (!selector)
    return FALSE;
  else if (strcasecmp (selector, "all") == 0 || strcmp (selector, "*") == 0)
    return TRUE;

  /* Take a local copy of the format string. */
  memset (facility, 0, sizeof (facility));
  strncpy (facility, format, sizeof (facility) - 1);

  /*
   * Isolate the facility, all up to ':', then return TRUE if what remains
   * is in the selector.
   */
  facility_end = strchr (facility, ':');
  if (facility_end && facility_end != facility)
    {
      const char *match;
      int length;

      /* Search for exact word match in the selector. */
      *facility_end = '\0';
      length = strlen (facility);
      match = strstr (selector, facility);
      return match
             && (match == selector || match[-1] == ' ')
             && (match[length] == ' ' || match[length] == '\0');
    }

  /* Either no ':' in the format passed in, or nothing precedes it. */
  return FALSE;
}


/*
 * ifp_trace()
 *
 * On the first call, this function looks for an environment variable
 * IFP_TRACE.  If set, then the value is taken as the trace selector.  Next,
 * the trace selector is searched for any occurrence of the name of the
 * calling module, and if found, the trace message is printed.
 *
 * Trace selection can also be set by the ifp_trace_select() function.  If
 * both ifp_trace_select() and IFP_TRACE are used, the message is printed if
 * the module is listed in either selector.
 */
void
ifp_trace (const char *format, ...)
{
  static int initialized = FALSE;
  static const char *env_trace_selector = NULL;
  assert (format);

  /* Look for the environment variable to trigger tracing. */
  if (!initialized)
    {
      env_trace_selector = getenv ("IFP_TRACE");
      if (env_trace_selector)
        {
          ifp_notice ("tracer:"
                      " %s initialized trace selector to '%s'",
                      "IFP_TRACE", env_trace_selector);
        }
      initialized = TRUE;
    }

  /*
   * If either tracing selector is set, and the caller module selected is in
   * it, print out the message.  Prefix with the plugin name for chaining
   * plugins.
   */
  if (ifp_trace_check_selector (ifp_trace_selector, format)
      || ifp_trace_check_selector (env_trace_selector, format))
    {
      va_list ap;

      if (ifp_self_inside_plugin ())
        {
          ifp_pluginref_t self;

          self = ifp_self ();
          fprintf (stderr, "%s-%s: ",
                   ifp_plugin_engine_name (self),
                   ifp_plugin_engine_version (self));
        }

      va_start (ap, format);
      vfprintf (stderr, format, ap);
      va_end (ap);
      fprintf (stderr, "\n");
      fflush (stderr);
    }
}


/*
 * ifp_trace_pointer()
 *
 * Alternative to unpleasant-looking casting.
 */
const void *
ifp_trace_pointer (const void *pointer)
{
  return pointer;
}


/*
 * ifp_trace_print()
 *
 * Low-level message printing function.  This function prepends a message
 * with details of any enclosing chaining plugin.
 */
static void
ifp_trace_print (const char *intro, const char *format, va_list ap)
{
  fprintf (stderr, "%s: ", intro);

  if (ifp_self_inside_plugin ())
    {
      ifp_pluginref_t self;

      self = ifp_self ();
      fprintf (stderr, "%s-%s: ",
               ifp_plugin_engine_name (self),
               ifp_plugin_engine_version (self));
    }

  vfprintf (stderr, format, ap);
  fprintf (stderr, "\n");
  fflush (stderr);
}


/**
 * ifp_messages()
 * ifp_messages_enabled()
 *
 * Enables or disables IFP error and warning messages, and reports message
 * enable/disable state.
 *
 */
void
ifp_messages (int enabled)
{
  ifp_error_messages = (enabled != 0);
}

int
ifp_messages_enabled (void)
{
  return ifp_error_messages;
}


/*
 * ifp_notice()
 * ifp_error()
 * ifp_fatal()
 *
 * IFP error reporting routines.  ifp_notice() and ifp_error() may be disabled.
 * ifp_fatal() messages are always printed.
 */
void
ifp_notice (const char *format, ...)
{
  va_list ap;
  assert (format);

  if (ifp_error_messages)
    {
      va_start (ap, format);
      ifp_trace_print ("IFP library notice", format, ap);
      va_end (ap);
    }
}

void
ifp_error (const char *format, ...)
{
  va_list ap;
  assert (format);

  if (ifp_error_messages)
    {
      va_start (ap, format);
      ifp_trace_print ("IFP library error", format, ap);
      va_end (ap);
    }
}

void
ifp_fatal (const char *format, ...)
{
  va_list ap;
  assert (format);

  va_start (ap, format);
  ifp_trace_print ("IFP fatal error", format, ap);
  va_end (ap);

  fprintf (stderr, "IFP dumping core...\n");
  abort ();
}
