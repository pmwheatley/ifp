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
 * A note of the plugin reference that this code is running in.  If NULL,
 * the code is not running in a plugin (and is therefore in some form of
 * 'main' program).  If not NULL, this is a chaining plugin.
 */
static ifp_pluginref_t ifp_plugin_self = NULL;

/*
 * Note of the chained plugin.  Within a chaining plugin, this is the plugin
 * that's chained forwards from self.
 */
static ifp_pluginref_t ifp_chained_plugin = NULL;


/*
 * ifp_self_set_plugin()
 *
 * Sets the plugin reference for the notion of "self".  For a chaining
 * plugin, this will be set by IFP to the reference of the chaining plugin
 * itself, allowing a chaining plugin to call plugin functions on itself.
 * For 'main' (non-plugin) code, the value of "self" is NULL.
 */
void
ifp_self_set_plugin (ifp_pluginref_t plugin)
{
  assert (!plugin || ifp_plugin_is_valid (plugin));

  /*
   * This function is called as part of loading a plugin.  At the point it is
   * called, the plugin is not fully loaded.  And functions that return plugin
   * data (such as the name and version) will complain if called for plugins
   * not loaded.  Because the tracer uses these functions, we need to be
   * careful not to call any tracing functions here.
   */

  ifp_plugin_self = plugin;
}


/*
 * ifp_self()
 *
 * Returns the plugin reference to "self" for code running inside a plugin.
 * The function returns NULL if the code is not running in a plugin.  The
 * value for "self" is set automatically by the IFP loader on loading a
 * plugin.
 */
ifp_pluginref_t
ifp_self (void)
{
  return ifp_plugin_self;
}


/*
 * ifp_self_inside_plugin()
 *
 * Returns TRUE if the code is running in what appears to be a chaining
 * plugin, FALSE otherwise.  If this function returns TRUE, it is guaranteed
 * that calls to ifp_self() will return a valid, non-NULL, plugin reference.
 */
int
ifp_self_inside_plugin (void)
{
  return ifp_plugin_self != NULL;
}


/*
 * ifp_chain_set_chained_plugin()
 * ifp_chain_get_chained_plugin()
 *
 * Set and get the current chained plugin.  Plugin may be NULL.  These
 * functions are defined explicitly for chaining plugins.  A chaining plugin
 * should use ifp_chain_set_chained_plugin() to inform IFP of which plugin it
 * has chained to.  This data can then be used automatically by the underlying
 * IFP functions.  ifp_chain_get_chained_plugin() is a convenience function
 * to allow chained plugin retrieval.
 */
void
ifp_chain_set_chained_plugin (ifp_pluginref_t plugin)
{
  ifp_trace ("chain: ifp_chain_set_chained_plugin <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  ifp_chained_plugin = plugin;
}

ifp_pluginref_t
ifp_chain_get_chained_plugin (void)
{
  return ifp_chained_plugin;
}


/*
 * ifpi_chain_set_plugin_self()
 *
 * Chaining support function.  This function is called by the loader when it
 * is found inside the plugin being loaded.  It passes the plugin reference
 * of the loaded plugin to the plugin itself.  The plugin should then store
 * this, so that it can call plugin functions on itself when required.
 *
 * The presence of this symbol is also the indication to the loader that
 * the plugin is a chaining plugin.
 */
void
ifpi_chain_set_plugin_self (ifp_pluginref_t plugin)
{
  /*
   * This function is called as part of loading a plugin.  At the point it is
   * called, the plugin is not fully loaded.  And functions that return
   * plugin data (such as the name and version) will complain if called for
   * plugins not loaded.  Because the tracer uses these functions, we need to
   * be careful not to call any tracing functions here.
   */

  ifp_self_set_plugin (plugin);
}


/*
 * ifpi_chain_return_plugin()
 *
 * Chaining support function.  The function returns the details of the
 * plugin we are chaining to the caller.  Used by main code to identify the
 * plugin that is doing the actual work of interpreting the game (that is,
 * the one at the end of the chain, and not the middlemen in between).
 */
ifp_pluginref_t
ifpi_chain_return_plugin (void)
{
  ifp_trace ("chain: ifpi_chain_return_plugin <- void");

  return ifp_chained_plugin;
}


/*
 * ifpi_chain_accept_preferences()
 *
 * Chaining support function.  This function is called to send this plugin
 * a set of preferences which it should use when constructing arguments to
 * send to its chained plugins.  This is the method by which preferences
 * pass forwards along a chain of plugins.
 */
void
ifpi_chain_accept_preferences (ifp_prefref_t prefs_list)
{
  ifp_trace ("chain: ifpi_chain_accept_preferences <-"
             " prefs_%p", ifp_trace_pointer (prefs_list));

  ifp_pref_use_foreign_data (prefs_list);
}


/*
 * ifp_chain_accept_plugin_path()
 *
 * Chaining support function.  This function is called to send this plugin
 * the plugin path set at the chain head.  The plugin path is the only part
 * of IFP configuration that needs to pass forwards.  Chaining plugins do
 * not use URLs or load Glk libraries.
 */
void
ifpi_chain_accept_plugin_path (const char *new_path)
{
  ifp_trace ("chain: ifpi_chain_accept_plugin_path <- '%s'", new_path);

  ifp_manager_set_plugin_path (new_path);
}
