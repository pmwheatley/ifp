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
#include <limits.h>

#include "protos.h"


/* Typedefs; not required, but they do improve code clarity. */
typedef unsigned char utf_char_t;
typedef unsigned char iso_char_t;
typedef unsigned long unicode_char_t;

/* Constants for converting UTF-8 into ISO-8859-1. */
static const int UTF_MAX_SEQUENCE = 6, UTF_EXTENT_SHIFT = 6;
static const utf_char_t UTF_EXTENT_MASK = 0x3f, UTF_EXTENT = 0x80;
static const unicode_char_t UNICODE_UCS_UNUSED = 0xffff;
static const iso_char_t ISO_ERROR_CHAR = '?';


/*
 * utf_get_mask()
 *
 * Return the first character mask for a given UTF-8 byte sequence length.
 * This is 1xxxxxxx for 1 byte sequences, then 111xxxxx to 1111111x for
 * 2 to 6 byte sequences.
 */
static utf_char_t
utf_get_mask (int length)
{
  int shift;

  shift = UTF_MAX_SEQUENCE - length + (length == 1 ? 2 : 1);
  return UCHAR_MAX << shift;
}


/*
 * utf_get_sequence_length()
 *
 * Return the length of the next sequence in the input UTF-8 stream.  Check
 * UTF-8 conformance, and return 0 if the check fails.  The UTF-8 check is
 * that length is between 1 to 6 bytes inclusive, and for 2 to 6 byte
 * sequences, that each byte after the first matches 10xxxxxx.
 */
static int
utf_get_sequence_length (const utf_char_t *utf)
{
  int length, extent;

  for (length = 1; length <= UTF_MAX_SEQUENCE; length++)
    {
      utf_char_t mask = utf_get_mask (length);

      if ((utf[0] & mask) == (utf_char_t) (mask << 1))
        break;
    }
  if (length > UTF_MAX_SEQUENCE)
    {
      fprintf (stderr, "UTF-8 byte count sequence error, 0x%02x.\n", utf[0]);
      return 0;
    }

  for (extent = 1; extent < length; extent++)
    {
      if ((utf[extent] & ~UTF_EXTENT_MASK) != UTF_EXTENT)
        break;
    }
  if (extent < length)
    {
      fprintf (stderr, "UTF-8 extent byte error, 0x%02x:%d.\n", utf[0], extent);
      return 0;
    }

  return length;
}


/*
 * utf_generate_unicode()
 *
 * Generate and return a Unicode character from a UTF-8 sequence of the given
 * length.  Check that the UTF-8 uses the shortest form possible, and return
 * 0xFFFF (not a valid UCS character) if it doesn't.
 */
static unicode_char_t
utf_generate_unicode (const utf_char_t *utf, int length)
{
  int extent;
  unicode_char_t unicode;

  unicode = utf[0] & ~utf_get_mask (length);
  if (unicode == 0)
    {
      fprintf (stderr, "UTF-8 Unicode conformance error, 0x%02x.\n", utf[0]);
      return UNICODE_UCS_UNUSED;
    }

  for (extent = 1; extent < length; extent++)
    {
      unicode <<= UTF_EXTENT_SHIFT;
      unicode |= utf[extent] & UTF_EXTENT_MASK;
    }

  return unicode;
}


/*
 * utf_convert_sequence()
 *
 * Convert a UTF-8 sequence into the character in *iso, and return the pointer
 * to the next UTF-8 character to process.  If the conversion results in an
 * invalid ISO-8859-1 character, use '?'.  If the conversion runs into a
 * UTF-8 sequence or Unicode representation error, use '?' and ignore this
 * UTF-8 character.
 */
static const utf_char_t *
utf_convert_sequence (const utf_char_t *utf, iso_char_t *iso)
{
  int length;

  length = utf_get_sequence_length (utf);
  if (length > 0)
    {
      unicode_char_t unicode;

      unicode = utf_generate_unicode (utf, length);
      if (unicode != UNICODE_UCS_UNUSED)
        {
          *iso = unicode > UCHAR_MAX ? ISO_ERROR_CHAR : (iso_char_t) unicode;
          return utf + length;
        }
    }

  *iso = ISO_ERROR_CHAR;
  return utf + 1;
}


/*
 * utf_to_iso_internal()
 *
 * Function to convert UTF-8 as returned by the XML parser into ISO-8859-1
 * suitable for display in Glk.  Not all UTF-8 can be accommodated by any
 * means, but for the most part we don't expect to encounter any non-Latin1
 * characters to occur.  Any that do, we'll just replace by '?'.
 *
 * This function returns a malloc'ed string, and the caller is responsible
 * for freeing the returned data.
 *
 * This function is an internal typesafe version.  The external version
 * casts to/from (unsigned) char* for convenience.
 */
static iso_char_t *
utf_to_iso_internal (const utf_char_t *utf_string)
{
  iso_char_t *iso;
  int index_;
  const utf_char_t *utf;

  /*
   * Because the conversion above clamps to single-character Unicode values,
   * the return string can never be longer than the input string.
   */
  iso = memory_malloc (strlen ((const char *) utf_string) + 1);
  index_ = 0;

  for (utf = utf_string; utf[0]; )
    utf = utf_convert_sequence (utf, iso + index_++);

  iso[index_] = '\0';
  return iso;
}


/*
 * utf_utf8_to_iso8859()
 *
 * Public function to convert UTF-8 into ISO-8859-1.
 */
char *
utf_utf8_to_iso8859 (const unsigned char *utf_string)
{
  assert (utf_string);

  return (char *) utf_to_iso_internal ((const utf_char_t *) utf_string);
}
