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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <glk.h>

#include "protos.h"


/* Interpreter magic identifier, and descriptor structure definition. */
static const unsigned int TERP_MAGIC = 0xee5a7b66;
struct terp_s
{
  unsigned int magic;

  int  version;              /* Header version used by the engine. */
  char *build_timestamp;     /* Date and time of plugin build */

  char *engine_type;         /* Data type the engine runs. */
  char *engine_name;         /* Unique engine name. */
  char *engine_version;      /* Engine's version number. */

  char *blorb_pattern;       /* If Blorb-capable, the exec type. */

  int  acceptor_offset;      /* Offset in file to "magic". */
  int  acceptor_length;      /* Length of "magic". */
  char *acceptor_pattern;    /* Regular expression for "magic". */

  char *author_name;         /* Interpreter author's name. */
  char *author_email;        /* Interpreter author's email. */
  char *engine_home_url;     /* Any URL regarding the interpreter. */

  char *builder_name;        /* Engine porter/builder's name. */
  char *builder_email;       /* Engine porter/builder's email. */

  char *engine_description;  /* Miscellaneous engine information. */
  char *engine_copyright;    /* Engine's copyright information. */

  terpref_t next;
};

/* List of interpreter descriptors, unordered. */
static terpref_t terps_head = NULL;


/*
 * terps_discover()
 * terps_is_interpreter()
 * terps_get_interpreter()
 * terps_iterate()
 * terps_is_empty()
 * terps_erase()
 *
 * Discover, iterate through, check, and erase an array of interpreters.
 */
void
terps_discover (void)
{
  ifp_pluginref_t plugin;

  ifp_loader_search_plugins_path (ifp_manager_get_plugin_path ());

  for (plugin = ifp_loader_iterate_plugins (NULL);
       plugin; plugin = ifp_loader_iterate_plugins (plugin))
    {
      int version, acceptor_offset, acceptor_length;
      const char *engine_type, *engine_name, *engine_version, *build_timestamp,
                 *blorb_pattern, *acceptor_pattern, *author_name, *author_email,
                 *engine_home_url, *builder_name, *builder_email,
                 *engine_description, *engine_copyright;
      terpref_t terp;

      ifp_plugin_dissect_header (plugin, &version,
                                 &engine_type, &engine_name, &engine_version,
                                 &build_timestamp, &blorb_pattern,
                                 &acceptor_offset, &acceptor_length,
                                 &acceptor_pattern, &author_name, &author_email,
                                 &engine_home_url,
                                 &builder_name, &builder_email,
                                 &engine_description, &engine_copyright);

      terp = memory_malloc (sizeof (*terp));
      terp->magic = TERP_MAGIC;
      terp->version = version;
      terp->build_timestamp = memory_strdup (build_timestamp);
      terp->engine_type = memory_strdup (engine_type);
      terp->engine_name = memory_strdup (engine_name);
      terp->engine_version = memory_strdup (engine_version);
      terp->blorb_pattern = memory_strdup (blorb_pattern);
      terp->acceptor_offset = acceptor_offset;
      terp->acceptor_length = acceptor_length;
      terp->acceptor_pattern = memory_strdup (acceptor_pattern);
      terp->author_name = memory_strdup (author_name);
      terp->author_email = memory_strdup (author_email);
      terp->engine_home_url = memory_strdup (engine_home_url);
      terp->builder_name = memory_strdup (builder_name);
      terp->builder_email = memory_strdup (builder_email);
      terp->engine_description = memory_strdup (engine_description);
      terp->engine_copyright = memory_strdup (engine_copyright);

      terp->next = terps_head;
      terps_head = terp;
    }
}

int
terps_is_interpreter (const terpref_t terp)
{
  assert (terp);

  return terp->magic == TERP_MAGIC;
}

void
terps_get_interpreter (const terpref_t terp,
                 int *version, const char **build_timestamp,
                 const char **engine_type, const char **engine_name,
                 const char **engine_version, const char **blorb_pattern,
                 int *acceptor_offset, int *acceptor_length,
                 const char **acceptor_pattern, const char **author_name,
                 const char **author_email, const char **engine_home_url,
                 const char **builder_name, const char **builder_email,
                 const char **engine_description, const char **engine_copyright)
{
  assert (terps_is_interpreter (terp));

  if (version)
    *version = terp->version;
  if (build_timestamp)
    *build_timestamp = terp->build_timestamp;
  if (engine_type)
    *engine_type = terp->engine_type;
  if (engine_name)
    *engine_name = terp->engine_name;
  if (engine_version)
    *engine_version = terp->engine_version;
  if (blorb_pattern)
    *blorb_pattern = terp->blorb_pattern;
  if (acceptor_offset)
    *acceptor_offset = terp->acceptor_offset;
  if (acceptor_length)
    *acceptor_length = terp->acceptor_length;
  if (acceptor_pattern)
    *acceptor_pattern = terp->acceptor_pattern;
  if (author_name)
    *author_name = terp->author_name;
  if (author_email)
    *author_email = terp->author_email;
  if (engine_home_url)
    *engine_home_url = terp->engine_home_url;
  if (builder_name)
    *builder_name = terp->builder_name;
  if (builder_email)
    *builder_email = terp->builder_email;
  if (engine_description)
    *engine_description = terp->engine_description;
  if (engine_copyright)
    *engine_copyright = terp->engine_copyright;
}

terpref_t
terps_iterate (const terpref_t terp)
{
  assert (!terp || terps_is_interpreter (terp));

  return terp ? terp->next : terps_head;
}

int
terps_is_empty (void)
{
  return terps_head == NULL;
}

void
terps_erase (void)
{
  terpref_t terp, next;

  for (terp = terps_head; terp; terp = next)
    {
      next = terp->next;

      memory_free (terp->build_timestamp);
      memory_free (terp->engine_type);
      memory_free (terp->engine_name);
      memory_free (terp->engine_version);
      memory_free (terp->blorb_pattern);
      memory_free (terp->acceptor_pattern);
      memory_free (terp->author_name);
      memory_free (terp->author_email);
      memory_free (terp->engine_home_url);
      memory_free (terp->builder_name);
      memory_free (terp->builder_email);
      memory_free (terp->engine_description);
      memory_free (terp->engine_copyright);

      memset (terp, 0, sizeof (*terp));
      memory_free (terp);
    }

  terps_head = NULL;
}


/*
 * terps_get_interpreter_engine_name()
 *
 * Convenience function to return just the engine name of an interpreter.
 */
const char *
terps_get_interpreter_engine_name (const terpref_t terp)
{
  assert (terps_is_interpreter (terp));

  return terp->engine_name;
}
