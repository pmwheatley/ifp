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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "ifp.h"
#include "ifp_internal.h"


/* Line buffer length, the longest input line the module can handle. */
enum { MAX_INPUT_LINE = 8192 };

/* Options separator string. */
static const char OPTIONS_SEPARATOR = ' ';

/* Section content, and descriptor structure definition. */
typedef struct ifp_pair_s *ifp_pairref_t;
struct ifp_pair_s {
  char *property;
  char *value;

  ifp_pairref_t next;
};

typedef struct ifp_section_s *ifp_sectionref_t;
struct ifp_section_s {
  char *name;           /* Section's name */

  ifp_pairref_t pairs;  /* Property/value pairs, unordered */

  ifp_sectionref_t next;
};

/* Metadata descriptor structure definition. */
typedef struct ifp_config_s *ifp_configref_t;
struct ifp_config_s {
  ifp_pairref_t pairs;             /* Global property/value pairs, unordered */

  ifp_sectionref_t sections_head;  /* Contained sections, ordered */
  ifp_sectionref_t sections_tail;
};


/*
 * ifp_config_find_pair()
 *
 * General helper for finding a property in a pairs list.  Returns NULL if
 * no matching property.  Input list may be NULL.
 */
static ifp_pairref_t
ifp_config_find_pair (const ifp_pairref_t list, const char *property)
{
  ifp_pairref_t pair;

  for (pair = list; pair; pair = pair->next)
    {
      if (strcasecmp (pair->property, property) == 0)
        break;
    }

  return pair;
}


/*
 * ifp_config_iterate()
 * ifp_config_get_name()
 * ifp_config_get_property_value()
 * ifp_config_get_global_property_value()
 * ifp_config_free()
 *
 * Iterate sections in a config, get a section name, a section property,
 * a global property, and free all config allocated memory.
 */
static ifp_sectionref_t
ifp_config_iterate (const ifp_configref_t config,
                    const ifp_sectionref_t section)
{
  return section ? section->next : config->sections_head;
}

static const char *
ifp_config_get_name (const ifp_sectionref_t section)
{
  return section->name;
}

static const char *
ifp_config_get_property_value (const ifp_sectionref_t section,
                               const char *property)
{
  ifp_pairref_t pair;

  pair = ifp_config_find_pair (section->pairs, property);
  return pair ? pair->value : NULL;
}

static const char *
ifp_config_get_global_property_value (const ifp_configref_t config,
                                      const char *property)
{
  ifp_pairref_t pair;

  pair = ifp_config_find_pair (config->pairs, property);
  return pair ? pair->value : NULL;
}

static void
ifp_config_free (ifp_configref_t config)
{
  ifp_sectionref_t section, next_section;
  ifp_pairref_t pair, next_pair;

  for (pair = config->pairs; pair; pair = next_pair)
    {
      next_pair = pair->next;

      ifp_free (pair->property);
      ifp_free (pair->value);

      memset (pair, 0, sizeof (*pair));
      ifp_free (pair);
    }

  for (section = config->sections_head; section; section = next_section)
    {
      next_section = section->next;

      for (pair = section->pairs; pair; pair = next_pair)
        {
          next_pair = pair->next;

          ifp_free (pair->property);
          ifp_free (pair->value);

          memset (pair, 0, sizeof (*pair));
          ifp_free (pair);
        }

      memset (section, 0, sizeof (*section));
      ifp_free (section);
    }

  memset (config, 0, sizeof (*config));
  ifp_free (config);
}


/*
 * ifp_config_lookup_section()
 *
 * Return the section for the given name, creating a new section if required.
 */
static ifp_sectionref_t
ifp_config_lookup_section (const ifp_configref_t config, const char *name)
{
  ifp_sectionref_t section;

  for (section = config->sections_head; section; section = section->next)
    {
      if (strcasecmp (section->name, name) == 0)
        break;
    }

  if (!section)
    {
      section = ifp_malloc (sizeof (*section));
      section->name = ifp_malloc (strlen (name) + 1);
      strcpy (section->name, name);
      section->pairs = NULL;

      section->next = NULL;
      if (!config->sections_head)
        {
          assert (!config->sections_tail);
          config->sections_head = section;
          config->sections_tail = section;
        }
      else
        {
          assert (config->sections_tail);
          config->sections_tail->next = section;
          config->sections_tail = section;
        }
    }

  return section;
}


/*
 * ifp_config_section_add_pair()
 * ifp_config_global_add_pair()
 *
 * Add a new name-value pair to a section or config.  If it already exists,
 * the pair is updated.
 */
static void
ifp_config_section_add_pair (ifp_sectionref_t section,
                             const char *property, const char *value)
{
  ifp_pairref_t pair;

  pair = ifp_config_find_pair (section->pairs, property);
  if (pair)
    {
      ifp_free (pair->value);
      pair->value = ifp_malloc (strlen (value) + 1);
      strcpy (pair->value, value);
    }
  else
    {
      pair = ifp_malloc (sizeof (*pair));
      pair->property = ifp_malloc (strlen (property) + 1);
      strcpy (pair->property, property);
      pair->value = ifp_malloc (strlen (value) + 1);
      strcpy (pair->value, value);

      pair->next = section->pairs;
      section->pairs = pair;
    }
}

static void
ifp_config_global_add_pair (ifp_configref_t config,
                            const char *property, const char *value)
{
  ifp_pairref_t pair;

  pair = ifp_config_find_pair (config->pairs, property);
  if (pair)
    {
      ifp_free (pair->value);
      pair->value = ifp_malloc (strlen (value) + 1);
      strcpy (pair->value, value);
    }
  else
    {
      pair = ifp_malloc (sizeof (*pair));
      pair->property = ifp_malloc (strlen (property) + 1);
      strcpy (pair->property, property);
      pair->value = ifp_malloc (strlen (value) + 1);
      strcpy (pair->value, value);

      pair->next = config->pairs;
      config->pairs = pair;
    }
}


/*
 * ifp_config_create_empty()
 *
 * Create a new empty config.
 */
static ifp_configref_t
ifp_config_create_empty (void)
{
  ifp_configref_t config;

  config = ifp_malloc (sizeof (*config));
  config->pairs = NULL;
  config->sections_head = config->sections_tail = NULL;

  return config;
}


/*
 * ifp_config_is_parse_comment()
 * ifp_config_is_parse_section()
 * ifp_config_is_parse_pair()
 *
 * Parser helpers, categorize lines and return allocated fields if matched.
 */
static int
ifp_config_is_parse_comment (const char *line)
{
  size_t index_;

  index_ = strspn (line, " \t");
  return index_ == strlen (line) || strchr (";#", line[index_]);
}

static int
ifp_config_is_parse_section (const char *line, char **name_ptr)
{
  char *name, close, dummy;
  int count;

  name = ifp_malloc (strlen (line) + 1);

  count = sscanf (line, " [ %[^] ] %c %c", name, &close, &dummy);
  if (count == 2 && close == ']')
    {
      *name_ptr = ifp_realloc (name, strlen (name) + 1);
      return TRUE;
    }

  ifp_free (name);
  return FALSE;
}

static int
ifp_config_is_parse_pair (const char *line,
                          char **property_ptr, char **value_ptr)
{
  char *property, *value, equals;
  int count;

  property = ifp_malloc (strlen (line) + 1);
  value = ifp_malloc (strlen (line) + 1);

  count = sscanf (line, " %[^=] =%[^\n]", property, value);
  if (count == 2)
    {
      *property_ptr = ifp_realloc (property, strlen (property) + 1);
      *value_ptr = ifp_realloc (value, strlen (value) + 1);
      return TRUE;
    }

  ifp_free (value);

  count = sscanf (line, " %[^=] %c", property, &equals);
  if (count == 2 && equals == '=')
    {
      *property_ptr = ifp_realloc (property, strlen (property) + 1);
      *value_ptr = ifp_malloc (1);
      strcpy (*value_ptr, "");
      return TRUE;
    }

  ifp_free (property);
  return FALSE;
}


/*
 * ifp_config_parse_getline()
 *
 * Helper for the config parser.  Gets the next line of input, trimming all
 * trailing cr/lf characters.  Returns false if at end of file.
 */
static int
ifp_config_parse_getline (FILE *stream,
                          char *buffer, int length, int *line_number)
{
  if (!feof (stream) && fgets (buffer, length, stream))
    {
      int bytes;

      bytes = strlen (buffer);
      while (bytes > 0 && strchr ("\r\n", buffer[bytes - 1]))
        buffer[--bytes] = '\0';

      *line_number += 1;
      return TRUE;
    }

  return FALSE;
}


/*
 * ifp_config_parse()
 *
 * Parse the input file and return a config, or NULL on error.
 */
static ifp_configref_t
ifp_config_parse (const char *file, FILE *stream)
{
  ifp_configref_t config;
  ifp_sectionref_t section;
  int line_number, errors;
  char line[MAX_INPUT_LINE];

  ifp_trace ("config: ifp_config_parse <- '%s' file_%p",
             file, ifp_trace_pointer (stream));

  section = NULL;
  line_number = errors = 0;

  config = ifp_config_create_empty ();

  while (ifp_config_parse_getline (stream, line, sizeof (line), &line_number))
    {
      char *name, *property, *value;

      if (ifp_config_is_parse_comment (line))
        continue;

      else if (ifp_config_is_parse_section (line, &name))
        {
          if (strcmp (name, "DEFAULT") == 0)
            section = NULL;
          else
            section = ifp_config_lookup_section (config, name);
          ifp_free (name);
          continue;
        }

      else if (ifp_config_is_parse_pair (line, &property, &value))
        {
          if (section)
            ifp_config_section_add_pair (section, property, value);
          else
            ifp_config_global_add_pair (config, property, value);

          ifp_free (property);
          ifp_free (value);
          continue;
        }

      ifp_error ("config: %s:%d: error: unrecognized line, expected either"
                 " [section] or property=value", file, line_number);
      ifp_error ("config: while parsing: '%s'", line);
      errors++;
    }

  if (errors > 0)
    {
      ifp_config_free (config);
      config = NULL;
    }

  if (config)
    ifp_trace ("config: ifp_config_parse returned config_%p",
               ifp_trace_pointer (config));

  return config;
}


/*
 * ifp_config_handle()
 *
 * Read values from a config and pass them on to other modules, preferences,
 * or wherever they belong.
 */
static void
ifp_config_handle (ifp_configref_t config)
{
  const char *value;
  ifp_sectionref_t section;

  ifp_trace ("config: ifp_config_handle <- config_%p",
             ifp_trace_pointer(config));

  /* Handle global values that affect other modules directly. */
  value = ifp_config_get_global_property_value (config, "plugin_path");
  if (value)
    {
      ifp_trace ("config: setting plugin_path to '%s'", value);
      ifp_manager_set_plugin_path (value);
    }

  value = ifp_config_get_global_property_value (config, "glk_libraries");
  if (value)
    {
      ifp_trace ("config: setting glk_libraries to '%s'", value);
      ifp_main_set_glk_libraries (value);
    }

  value = ifp_config_get_global_property_value (config, "cache_limit");
  if (value)
    {
      ifp_trace ("config: setting cache_limit to '%s'", value);
      ifp_cache_set_limit (atoi (value));
    }

  value = ifp_config_get_global_property_value (config, "url_timeout");
  if (value)
    {
      ifp_trace ("config: setting url_timeout to '%s'", value);
      ifp_url_set_pause_timeout (atoi (value));
    }

  /* Iterate sections, passing options values as preferences. */
  for (section = ifp_config_iterate (config, NULL);
       section; section = ifp_config_iterate (config, section))
    {
      const char *section_name, *options;
      char *engine_name, *engine_version, *separator;

      section_name = ifp_config_get_name (section);
      engine_name = ifp_malloc (strlen (section_name) + 1);
      strcpy (engine_name, section_name);

      separator = strchr (engine_name, '-');
      if (separator)
        {
          separator[0] = '\0';
          engine_version = separator + 1;
        }
      else
        engine_version = NULL;

      ifp_trace ("config: section for engine '%s', version '%s'",
                 engine_name, engine_version ? engine_version : "*");

      options = ifp_config_get_property_value (section, "options");
      if (options)
        {
          char **elements;
          int count, index_;

          /* Split the options string on ' ' characters. */
          count = ifp_split_string (options, OPTIONS_SEPARATOR, &elements);

          ifp_trace ("config: adding preferences for options '%s'", options);

          for (index_ = 0; index_ < count; index_++)
            {
              const char *preference;

              preference = elements[index_];
              ifp_pref_register (engine_name, engine_version, preference);
            }

          ifp_free_split_string (elements, count);
        }

      ifp_free (engine_name);
    }
}


/*
 * ifp_config_read()
 *
 * Locate and process any user-specific configuration file.
 */
void
ifp_config_read (void)
{
  const char *env_path;
  char file[PATH_MAX];
  FILE *stream;
  ifp_configref_t config;

  ifp_trace ("config: ifp_config_read <- void");

  stream = NULL;

  /* Check for a fully specified configuration file path first. */
  env_path = getenv ("IFP_CONFIGURATION");
  if (env_path)
    {
      stream = fopen (env_path, "r");
      if (!stream)
        {
          ifp_error ("config: %s: %s", env_path, strerror (errno));
          return;
        }

      snprintf (file, sizeof (file), "%s", env_path);
      ifp_trace ("config: using config from env %s", env_path);
    }

  /* If no fully specified configuration file, check in $HOME and /etc. */
  if (!stream)
    {
      const char *home;

      home = getenv ("HOME");
      if (home)
        {
          char path[PATH_MAX];

          snprintf (path, sizeof (path), "%s/%s", home, ".ifprc");

          stream = fopen (path, "r");
          if (stream)
            {
              snprintf (file, sizeof (file), "%s", path);
              ifp_trace ("config: using config from home %s", path);
            }
        }
      else
        ifp_trace ("config: no value found for %s", "HOME");
    }

  if (!stream)
    {
      stream = fopen ("/etc/ifprc", "r");
      if (stream)
        {
          snprintf (file, sizeof (file), "%s", "/etc/ifprc");
          ifp_trace ("config: using config from default %s", "/etc/ifprc");
        }
    }

  if (!stream)
    {
      ifp_trace ("config: no config file found");
      return;
    }

  config = ifp_config_parse (file, stream);
  if (!config)
    {
      ifp_error ("config: %s: error parsing configuration file", file);
      fclose (stream);
      return;
    }
  fclose (stream);

  ifp_config_handle (config);

  ifp_trace ("config: done with the config file");
  ifp_config_free (config);
}
