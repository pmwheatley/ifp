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
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "protos.h"


/* Line buffer length, the longest input line the module can handle. */
enum { MAX_INPUT_LINE = 8192 };

/* Section content, magic identifier, and descriptor structure definition. */
typedef struct pair_s *pairref_t;
struct pair_s {
  char *property;
  char *value;

  pairref_t next;
};

static const unsigned int SECTION_MAGIC = 0x956a97c8;
struct inisection_s {
  unsigned int magic;

  char *name;                     /* Section's name */

  pairref_t pairs;                /* Property/value pairs, unordered */

  inisectionref_t next;
};

/* Metadata magic identifier, and descriptor structure definition. */
static const unsigned int DOC_MAGIC = 0xb9a8712a;
struct inidoc_s {
  unsigned int magic;

  pairref_t pairs;                /* Global property/value pairs, unordered */

  inisectionref_t sections_head;  /* Contained sections, ordered */
  inisectionref_t sections_tail;
};


/*
 * inidoc_is_doc()
 * inidoc_is_section()
 *
 * Check document and section pointers for validity.
 */
static int
inidoc_is_doc (const inidocref_t doc)
{
  assert (doc);

  return doc->magic == DOC_MAGIC;
}

static int
inidoc_is_section (const inisectionref_t section)
{
  assert (section);

  return section->magic == SECTION_MAGIC;
}


/*
 * inidoc_find_pair()
 *
 * General helper for finding a property in a pairs list.  Returns NULL if
 * no matching property.  Input list may be NULL.
 */
static pairref_t
inidoc_find_pair (const pairref_t list, const char *property)
{
  pairref_t pair;

  for (pair = list; pair; pair = pair->next)
    {
      if (strcasecmp (pair->property, property) == 0)
        break;
    }

  return pair;
}


/*
 * inidoc_iterate()
 * inidoc_get_name()
 * inidoc_get_property_value()
 * inidoc_get_global_property_value()
 * inidoc_free()
 *
 * Iterate sections in a document, get a section name, a section property,
 * a global property, and free all document allocated memory.
 */
inisectionref_t
inidoc_iterate (const inidocref_t doc, const inisectionref_t section)
{
  assert (inidoc_is_doc (doc));
  assert (!section || inidoc_is_section (section));

  return section ? section->next : doc->sections_head;
}

const char *
inidoc_get_name (const inisectionref_t section)
{
  assert (inidoc_is_section (section));

  return section->name;
}

const char *
inidoc_get_property_value (const inisectionref_t section, const char *property)
{
  pairref_t pair;
  assert (inidoc_is_section (section));

  pair = inidoc_find_pair (section->pairs, property);
  return pair ? pair->value : NULL;
}

const char *
inidoc_get_global_property_value (const inidocref_t doc, const char *property)
{
  pairref_t pair;
  assert (inidoc_is_doc (doc));

  pair = inidoc_find_pair (doc->pairs, property);
  return pair ? pair->value : NULL;
}

void
inidoc_free (inidocref_t doc)
{
  inisectionref_t section, next_section;
  pairref_t pair, next_pair;
  assert (inidoc_is_doc (doc));

  for (pair = doc->pairs; pair; pair = next_pair)
    {
      next_pair = pair->next;

      memory_free (pair->property);
      memory_free (pair->value);

      memset (pair, 0, sizeof (*pair));
      memory_free (pair);
    }

  for (section = doc->sections_head; section; section = next_section)
    {
      next_section = section->next;

      for (pair = section->pairs; pair; pair = next_pair)
        {
          next_pair = pair->next;

          memory_free (pair->property);
          memory_free (pair->value);

          memset (pair, 0, sizeof (*pair));
          memory_free (pair);
        }

      memset (section, 0, sizeof (*section));
      memory_free (section);
    }

  memset (doc, 0, sizeof (*doc));
  memory_free (doc);
}


/*
 * inidoc_debug_abbreviate()
 * inidoc_debug_dump()
 *
 * Print the document to the given file stream.
 */
static void
inidoc_debug_abbreviate (FILE *stream, const char *string, int length)
{
  if (strlen (string) > (size_t) length)
    {
      char *copy;

      copy = memory_malloc (length + 1);

      snprintf (copy, length + 1, "%s", string);
      fprintf (stream, "%s%s", copy, "...");

      memory_free (copy);
    }
  else
    fprintf (stream, "%s", string);
}

void
inidoc_debug_dump (FILE *stream, const inidocref_t doc)
{
  inisectionref_t section;
  pairref_t pair;
  assert (inidoc_is_doc (doc));

  fprintf (stream, "INIFILE\n");

  for (pair = doc->pairs; pair; pair = pair->next)
    {
      fprintf (stream, "  property=");
      inidoc_debug_abbreviate (stream, pair->property, 60);
      fprintf (stream, "\n");

      fprintf (stream, "  value=");
      inidoc_debug_abbreviate (stream, pair->value, 60);
      fprintf (stream, "\n");
    }

  for (section = doc->sections_head; section; section = section->next)
    {
      fprintf (stream, "  SECTION %s\n", section->name);

      for (pair = section->pairs; pair; pair = pair->next)
        {
          fprintf (stream, "    property=");
          inidoc_debug_abbreviate (stream, pair->property, 60);
          fprintf (stream, "\n");

          fprintf (stream, "    value=");
          inidoc_debug_abbreviate (stream, pair->value, 60);
          fprintf (stream, "\n");
        }
    }
}


/*
 * inidoc_lookup_section()
 *
 * Return the section for the given name, creating a new section if required.
 */
static inisectionref_t
inidoc_lookup_section (const inidocref_t doc, const char *name)
{
  inisectionref_t section;

  for (section = doc->sections_head; section; section = section->next)
    {
      if (strcasecmp (section->name, name) == 0)
        break;
    }

  if (!section)
    {
      section = memory_malloc (sizeof (*section));
      section->magic = SECTION_MAGIC;
      section->name = memory_strdup (name);
      section->pairs = NULL;

      section->next = NULL;
      if (!doc->sections_head)
        {
          assert (!doc->sections_tail);
          doc->sections_head = section;
          doc->sections_tail = section;
        }
      else
        {
          assert (doc->sections_tail);
          doc->sections_tail->next = section;
          doc->sections_tail = section;
        }
    }

  return section;
}


/*
 * inidoc_section_add_pair()
 * inidoc_global_add_pair()
 *
 * Add a new name-value pair to a section or document.  If it already exists,
 * the pair is updated.
 */
static void
inidoc_section_add_pair (inisectionref_t section,
                         const char *property, const char *value)
{
  pairref_t pair;

  pair = inidoc_find_pair (section->pairs, property);
  if (pair)
    {
      memory_free (pair->value);
      pair->value = memory_strdup (value);
    }
  else
    {
      pair = memory_malloc (sizeof (*pair));
      pair->property = memory_strdup (property);
      pair->value = memory_strdup (value);

      pair->next = section->pairs;
      section->pairs = pair;
    }
}

static void
inidoc_global_add_pair (inidocref_t doc,
                        const char *property, const char *value)
{
  pairref_t pair;

  pair = inidoc_find_pair (doc->pairs, property);
  if (pair)
    {
      memory_free (pair->value);
      pair->value = memory_strdup (value);
    }
  else
    {
      pair = memory_malloc (sizeof (*pair));
      pair->property = memory_strdup (property);
      pair->value = memory_strdup (value);

      pair->next = doc->pairs;
      doc->pairs = pair;
    }
}


/*
 * inidoc_create_empty()
 *
 * Create a new empty document.
 */
static inidocref_t
inidoc_create_empty (void)
{
  inidocref_t doc;

  doc = memory_malloc (sizeof (*doc));
  doc->magic = DOC_MAGIC;
  doc->pairs = NULL;
  doc->sections_head = doc->sections_tail = NULL;

  return doc;
}


/*
 * inidoc_is_parse_comment()
 * inidoc_is_parse_section()
 * inidoc_is_parse_pair()
 *
 * Parser helpers, categorize lines and return allocated fields if matched.
 */
static int
inidoc_is_parse_comment (const char *line)
{
  size_t index_;

  index_ = strspn (line, " \t");
  return index_ == strlen (line) || strchr (";#", line[index_]);
}

static int
inidoc_is_parse_section (const char *line, char **name_ptr)
{
  char *name, close, dummy;
  int count;

  name = memory_malloc (strlen (line) + 1);

  count = sscanf (line, " [ %[^] ] %c %c", name, &close, &dummy);
  if (count == 2 && close == ']')
    {
      *name_ptr = memory_realloc (name, strlen (name) + 1);
      return TRUE;
    }

  memory_free (name);
  return FALSE;
}

static int
inidoc_is_parse_pair (const char *line, char **property_ptr, char **value_ptr)
{
  char *property, *value, equals;
  int count;

  property = memory_malloc (strlen (line) + 1);
  value = memory_malloc (strlen (line) + 1);

  count = sscanf (line, " %[^=] =%[^\n]", property, value);
  if (count == 2)
    {
      *property_ptr = memory_realloc (property, strlen (property) + 1);
      *value_ptr = memory_realloc (value, strlen (value) + 1);
      return TRUE;
    }

  memory_free (value);

  count = sscanf (line, " %[^=] %c", property, &equals);
  if (count == 2 && equals == '=')
    {
      *property_ptr = memory_realloc (property, strlen (property) + 1);
      *value_ptr = memory_strdup ("");
      return TRUE;
    }

  memory_free (property);
  return FALSE;
}


/*
 * inidoc_parse_error()
 *
 * Helper for the document parser.  Reports parser errors and increments
 * the error count.
 */
static void
inidoc_parse_error (const char *file, const char *line,
                    int line_number, int *error_count, const char *format, ...)
{
  va_list ap;
  char *copy;

  fprintf (stderr, "%s:%d: error: ", file, line_number);

  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);

  copy = memory_strdup (line);
  copy[strspn (line, " \t")] = '\0';

  fprintf (stderr, "\n%s\n%s^\n", line, copy);

  memory_free (copy);

  *error_count += 1;
}


/*
 * inidoc_parse_getline()
 *
 * Helper for the document parser.  Gets the next line of input, trimming all
 * trailing cr/lf characters.  Returns false if at end of file.
 */
static int
inidoc_parse_getline (FILE *stream, char *buffer, int length, int *line_number)
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
 * inidoc_parse()
 *
 * Parse the input file and return a document, or NULL on error.
 */
inidocref_t
inidoc_parse (const char *file)
{
  FILE *stream;
  inidocref_t doc;
  inisectionref_t section;
  int line_number, errors;
  char line[MAX_INPUT_LINE];

  stream = fopen (file, "r");
  if (!stream)
    {
      fprintf (stderr, "%s: error: %s\n", file, strerror (errno));
      return NULL;
    }

  section = NULL;
  line_number = errors = 0;

  doc = inidoc_create_empty ();

  while (inidoc_parse_getline (stream, line, sizeof (line), &line_number))
    {
      char *name, *property, *value;

      if (inidoc_is_parse_comment (line))
        continue;

      else if (inidoc_is_parse_section (line, &name))
        {
          if (strcmp (name, "DEFAULT") == 0)
            section = NULL;
          else
            section = inidoc_lookup_section (doc, name);
          memory_free (name);
          continue;
        }

      else if (inidoc_is_parse_pair (line, &property, &value))
        {
          if (section)
            inidoc_section_add_pair (section, property, value);
          else
            inidoc_global_add_pair (doc, property, value);

          memory_free (property);
          memory_free (value);
          continue;
        }

      inidoc_parse_error (file, line, line_number, &errors,
                          "Unrecognized line:"
                          " expected either [section] or property=value");
    }

  fclose (stream);

  if (errors > 0)
    {
      inidoc_free (doc);
      doc = NULL;
    }

  return doc;
}
