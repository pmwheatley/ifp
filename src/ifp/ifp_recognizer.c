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
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "ifp.h"
#include "ifp_internal.h"


/**
 * ifp_recognizer_match_string()
 *
 * Given a string and a regular expression pattern, match the pattern to
 * the string.  Return TRUE if the string matches the pattern.
 */
int
ifp_recognizer_match_string (const char *string, const char *pattern)
{
  regex_t expression;
  int status, match;
  assert (string && pattern);

  ifp_trace ("recognizer:"
             " ifp_recognizer_match_string <- '%s' '%s'", string, pattern);

  status = regcomp (&expression, pattern, REG_EXTENDED | REG_ICASE);
  if (status == 0)
    match = (regexec (&expression, string, 0, NULL, 0) == 0);
  else
    {
      char message[256];

      regerror (status, &expression, message, sizeof (message));
      ifp_error ("recognizer:"
                 " error compiling pattern '%s': %s", pattern, message);
      match = FALSE;
    }

  regfree (&expression);

  if (match)
    ifp_trace ("recognizer: pattern matched successfully");
  else
    ifp_trace ("recognizer: pattern did not match");

  return match;
}


/**
 * ifp_recognizer_match_binary()
 *
 * Given a binary buffer and a regular expression pattern, match the pattern
 * to a character conversion of the binary data in the buffer.  Binary data
 * is converted into ASCII before pattern matching, so that each individual
 * byte in the buffer is represented as "%02X", and the individual byte
 * values are space-separated.  The pattern is applied to the converted data
 * representation.
 *
 * ASCIIfying is necessary because regular expressions work only on strings,
 * not on binary data.  Unfortunately, it make the acceptor patterns
 * obfuscated for the easy cases of simple ASCII ids in files (for example,
 * "Glul" becomes the pattern "^47 6c 75 6c$").  However, it makes cases
 * possible that would otherwise be impossible.
 */
int
ifp_recognizer_match_binary (const char *buffer,
                             int length, const char *pattern)
{
  char *representation;
  int allocation, index_, match;
  assert (buffer && length >= 0 && pattern);

  ifp_trace ("recognizer: ifp_recognizer_match_binary <- '%s'", pattern);

  /* Allow three characters for each byte in the buffer, one if empty. */
  allocation = length > 0 ? length * 3 : 1;
  representation = ifp_malloc (allocation);
  strcpy (representation, "");

  /* Convert each character to its two-byte character representation. */
  for (index_ = 0; index_ < length; index_++)
    {
      char conversion[4];

      snprintf (conversion, sizeof (conversion),
                index_ == 0 ? "%02x" : " %02x", (unsigned char) buffer[index_]);
      strcat (representation, conversion);
    }

  match = ifp_recognizer_match_string (representation, pattern);
  ifp_free (representation);
  return match;
}
