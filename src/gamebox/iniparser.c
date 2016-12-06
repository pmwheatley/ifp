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

#include "protos.h"


/*
 * IFMES namespace and element/attribute name constants.  The following set
 * is generated from version 1.1, working draft "2 September 2005".
 */
typedef struct {
  const char *ABOUT, *AUTHOR, *BYLINE, *DESC, *GENRE, *HEADLINE,
             *LENGTH, *PUBLISHER, *RELEASE_DATE, *TITLE, *VERSION;
} ini_names_t;

static const ini_names_t ini_names =
{
  .ABOUT = "about", .AUTHOR = "author", .BYLINE = "byline", .DESC = "desc",
  .GENRE = "genre", .HEADLINE = "headline", .LENGTH = "length",
  .PUBLISHER = "publisher", .RELEASE_DATE = "releasedate", .TITLE = "title",
  .VERSION = "version"
};


/* Binding structure to allow variables to be bound to the above names. */
typedef struct {
  const char *name;
  char ** const variable_ptr;
} binding_t;


/*
 * ini_get_section_properties()
 *
 * Process multiple section properties using a given binding.  If
 * is_iso_encoded is not set, convert each bound property from UTF-8 to
 * ISO-8859-1.  section may not be NULL.
 */
static void
ini_get_section_properties (const inisectionref_t section,
                            const binding_t *bindings, int is_iso_encoded)
{
  const binding_t *binding;
  assert (section);

  for (binding = bindings; binding->variable_ptr; binding++)
    {
      const char *property;

      property = inidoc_get_property_value (section, binding->name);
      if (property)
        {
          *binding->variable_ptr = is_iso_encoded
                                   ? memory_strdup (property)
                                   : utf_utf8_to_iso8859 (property);
        }
      else
        *binding->variable_ptr = NULL;
    }
}


/*
 * ini_parse_section()
 *
 * Parse an inidoc section.  Extract the properties and add an entry to the
 * game set to represent the section.
 */
static void
ini_parse_section (const inisectionref_t section, int is_iso_encoded)
{
  char *about;            /* about (the game file!) */
  char *author;           /* author (used if no byline) */
  char *byline;           /* byline (overrides author) */
  char *description;      /* desc (overrides headline) */
  char *genre;            /* genre */
  char *headline;         /* headline (used if no desc) */
  char *length;           /* length */
  char *publisher;        /* publisher (used if no author) */
  char *release_date;     /* releaseDate */
  char *title;            /* title */
  char *version;          /* version */

  binding_t bindings[] = {
    { ini_names.ABOUT, &about },
    { ini_names.AUTHOR, &author },
    { ini_names.BYLINE, &byline },
    { ini_names.DESC, &description },
    { ini_names.GENRE, &genre },
    { ini_names.HEADLINE, &headline },
    { ini_names.LENGTH, &length },
    { ini_names.PUBLISHER, &publisher },
    { ini_names.RELEASE_DATE, &release_date },
    { ini_names.TITLE, &title },
    { ini_names.VERSION, &version },
    { NULL, NULL }
  };

  ini_get_section_properties (section, bindings, is_iso_encoded);

  if (!about)
    about = memory_strdup (inidoc_get_name (section));

  if (about)
    {
      if (!title)
        title = memory_strdup ("[Unknown Game]");

      gameset_add_game (about, author, byline, description, genre, headline,
                        length, publisher, release_date, title, version);
    }

  memory_free (about);
  memory_free (author);
  memory_free (byline);
  memory_free (description);
  memory_free (genre);
  memory_free (headline);
  memory_free (length);
  memory_free (publisher);
  memory_free (release_date);
  memory_free (title);
  memory_free (version);
}


/*
 * ini_parse_file()
 *
 * Parse the given ini file into a set of games.
 */
int
ini_parse_file (const char *ini_file, int dump_document)
{
  inidocref_t document;
  inisectionref_t section;
  const char *encoding;
  int is_iso_encoded;

  document = inidoc_parse (ini_file);
  if (!document)
    return FALSE;

  if (dump_document)
    inidoc_debug_dump (stderr, document);

  encoding = inidoc_get_global_property_value (document, "encoding");
  is_iso_encoded = encoding && strcasecmp (encoding, "iso-8859-1") == 0;

  for (section = inidoc_iterate (document, NULL);
       section; section = inidoc_iterate (document, section))
    {
      ini_parse_section (section, is_iso_encoded);
    }

  inidoc_free (document);
  return TRUE;
}
