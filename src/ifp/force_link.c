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

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Force a reference to the ifpi_header symbol.  It's mandatory in a plugin,
 * so as part of forcing the link to find everything a plugin needs, we'll
 * include it, and make a nugatory reference to it later on.
 */
extern struct ifp_header ifpi_header;


/*
 * ifpi_force_link()
 *
 * This function serves only to make it easy to get the linker to include
 * all the plugin-side IFP modules in a plugin's .so.  If flagged as an
 * undefined symbol when linking the .so, it causes all the rest of the
 * plugin-side IFP functions to be loaded automatically.
 *
 * On call, the function does nothing.
 */
void
ifpi_force_link (void)
{
  /*
   * A hokey way to force a reference to ifpi_header and at the same time
   * defeat most compiler checks for unreachable code and unused variables.
   */
  if (ifpi_header.version != 0)
    return;

  /*
   * Make calls that we know will be ignored to both interface-attaching
   * functions (should we somehow get to here...).
   */
  ifpi_attach_glk_interface (ifpi_retrieve_glk_interface ());
  ifpi_attach_libc_interface (ifpi_retrieve_libc_interface ());
}
