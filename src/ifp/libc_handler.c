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

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Libc interface vector, created for passing to a loaded plugin library
 * as part of its initialization.  This passes off the libc intercept
 * functions to the caller.
 */
static struct ifp_libc_interface libc_interface = {
  .version = IFP_LIBC_VERSION,

  .malloc = ifp_libc_intercept_malloc,
  .calloc = ifp_libc_intercept_calloc,
  .realloc = ifp_libc_intercept_realloc,
  .strdup = ifp_libc_intercept_strdup,
  .getcwd = ifp_libc_intercept_getcwd,
  .scandir = ifp_libc_intercept_scandir,
  .free = ifp_libc_intercept_free,

  .open_2 = ifp_libc_intercept_open_2,
  .open_3 = ifp_libc_intercept_open_3,
  .close = ifp_libc_intercept_close,
  .creat = ifp_libc_intercept_creat,
  .dup = ifp_libc_intercept_dup,
  .dup2 = ifp_libc_intercept_dup2,

  .fopen = ifp_libc_intercept_fopen,
  .fdopen = ifp_libc_intercept_fdopen,
  .freopen = ifp_libc_intercept_freopen,
  .fclose = ifp_libc_intercept_fclose,
};


/*
 * ifp_libc_get_interface()
 *
 * Return a usable Libc interface vector.  If running inside a chaining
 * plugin, this function will return the Libc interface handed to the
 * chaining plugin on plugin load.  This allows the Libc interface to
 * be passed forwards to chained plugins.  Otherwise, the function returns
 * the main program Libc interface, listing the callable addresses of the
 * intercepted Libc routines.
 */
ifp_libc_interfaceref_t
ifp_libc_get_interface (void)
{
  ifp_trace ("libc: ifp_libc_get_interface <- void");

  if (ifp_self_inside_plugin ())
    {
      ifp_trace ("libc: returning a previously-attached libc interface");
      return ifp_plugin_retrieve_libc_interface (ifp_self ());
    }
  else
    {
      ifp_trace ("libc: returning the main libc interface");
      return &libc_interface;
    }
}
