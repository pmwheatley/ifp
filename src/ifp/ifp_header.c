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
 * USA.
 */

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "ifp.h"
#include "ifp_internal.h"


/**
 * ifp_plugin_engine_type()
 * ifp_plugin_engine_name()
 * ifp_plugin_engine_version()
 *
 * Convenience functions to return a plugin engine's type, name, and
 * version strings.  If this information is not available, the functions
 * return NULL.
 */
const char *
ifp_plugin_engine_type (ifp_pluginref_t plugin)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return NULL;
    }

  return header->engine_type;
}

const char *
ifp_plugin_engine_name (ifp_pluginref_t plugin)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return NULL;
    }

  return header->engine_name;
}

const char *
ifp_plugin_engine_version (ifp_pluginref_t plugin)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return NULL;
    }

  return header->engine_version;
}


/**
 * ifp_plugin_blorb_pattern()
 * ifp_plugin_acceptor_offset()
 * ifp_plugin_acceptor_length()
 * ifp_plugin_acceptor_pattern()
 *
 * Convenience functions to return a plugin engine's blorb pattern and
 * acceptor details.  If this information is not available, the functions
 * return NULL or zero as appropriate.
 */
const char *
ifp_plugin_blorb_pattern (ifp_pluginref_t plugin)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return NULL;
    }

  return header->blorb_pattern;
}

int
ifp_plugin_acceptor_offset (ifp_pluginref_t plugin)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return 0;
    }

  return header->acceptor_offset;
}

int
ifp_plugin_acceptor_length (ifp_pluginref_t plugin)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return 0;
    }

  return header->acceptor_length;
}

const char *
ifp_plugin_acceptor_pattern (ifp_pluginref_t plugin)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return NULL;
    }

  return header->acceptor_pattern;
}


/**
 * ifp_plugin_find_chain_end()
 *
 * Given a plugin, follow the chain of plugins it may be attached to until
 * finding the last one on the chain.  This will be the one doing the real
 * work of interpreting a game.  Return that plugin.
 */
ifp_pluginref_t
ifp_plugin_find_chain_end (ifp_pluginref_t plugin)
{
  ifp_pluginref_t cursor, final;
  assert (ifp_plugin_is_valid (plugin));

  ifp_trace ("header: ifp_plugin_find_chain_end <-"
             " plugin_%p", ifp_trace_pointer (plugin));

  final = plugin;
  for (cursor = ifp_plugin_get_chain (plugin); cursor;
       cursor = ifp_plugin_get_chain (cursor))
    final = cursor;

  ifp_trace ("header: chain end is plugin_%p", ifp_trace_pointer (final));
  return final;
}


/**
 * ifp_plugin_is_equal()
 *
 * Compare two plugins for equality.  Plugins are equivalent if their name
 * and version number strings match.
 */
int
ifp_plugin_is_equal (ifp_pluginref_t plugin, ifp_pluginref_t check)
{
  assert (ifp_plugin_is_valid (plugin) && ifp_plugin_is_valid (check));

  return (strcmp (ifp_plugin_engine_name (plugin),
                  ifp_plugin_engine_name (check)) == 0
          && strcmp (ifp_plugin_engine_version (plugin),
                     ifp_plugin_engine_version (check)) == 0);
}


/**
 * ifp_plugin_dissect_header()
 *
 * Return each header field to the caller individually.  Passing in a NULL
 * pointer indicates no interest in the field on the part of the caller.
 */
void
ifp_plugin_dissect_header (ifp_pluginref_t plugin,
                           int *version,
                           const char **engine_type,
                           const char **engine_name,
                           const char **engine_version,
                           const char **build_timestamp,
                           const char **blorb_pattern,
                           int *acceptor_offset,
                           int *acceptor_length,
                           const char **acceptor_pattern,
                           const char **author_name,
                           const char **author_email,
                           const char **engine_home_url,
                           const char **builder_name,
                           const char **builder_email,
                           const char **engine_description,
                           const char **engine_copyright)
{
  ifp_headerref_t header;
  assert (ifp_plugin_is_valid (plugin));

  header = ifp_plugin_get_header (plugin);
  if (!header)
    {
      ifp_error ("header: failed to obtain plugin header");
      return;
    }

  if (version)
    *version = header->version;
  if (engine_type)
    *engine_type = header->engine_type;
  if (engine_name)
    *engine_name = header->engine_name;
  if (engine_version)
    *engine_version = header->engine_version;
  if (build_timestamp)
    *build_timestamp = header->build_timestamp;
  if (blorb_pattern)
    *blorb_pattern = header->blorb_pattern;
  if (acceptor_offset)
    *acceptor_offset = header->acceptor_offset;
  if (acceptor_length)
    *acceptor_length = header->acceptor_length;
  if (acceptor_pattern)
    *acceptor_pattern = header->acceptor_pattern;
  if (author_name)
    *author_name = header->author_name;
  if (author_email)
    *author_email = header->author_email;
  if (engine_home_url)
    *engine_home_url = header->engine_home_url;
  if (builder_name)
    *builder_name = header->builder_name;
  if (builder_email)
    *builder_email = header->builder_email;
  if (engine_description)
    *engine_description = header->engine_description;
  if (engine_copyright)
    *engine_copyright = header->engine_copyright;
}
