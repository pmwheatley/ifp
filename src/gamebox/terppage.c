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
#include <stdlib.h>

#include <glk.h>

#include "protos.h"


/*
 * terppage_convert_date()
 *
 * Shrink a standard IFP engine timestamp down from "mmm dd yyyy, hh:mm:ss"
 * to just "mmm dd yyyy".
 */
static void
terppage_convert_date (const char *ifp_date)
{
  int year, day, hours, minutes, seconds, count;
  char trailer;

  /* Check the format of the string before removing the time portion. */
  count = sscanf (ifp_date, " %*3s %2d %4d, %2d:%2d:%2d%c",
                  &day, &year, &hours, &minutes, &seconds, &trailer);
  if (!(count == 5 || (count == 6 && trailer == ' ')))
    {
      glk_c_put_string (ifp_date);
      return;
    }

  if (day < 1 || day > 31)
    {
      glk_c_put_string (ifp_date);
      return;
    }

  glk_c_put_buffer (ifp_date, strchr (ifp_date, ',') - ifp_date);
}


/*
 * terppage_interpreter_details
 *
 * Write an interpreter plugin's details to the current Glk stream.
 */
static void
terppage_interpreter_details (const terpref_t terp)
{
  int version, acceptor_offset, acceptor_length;
  const char *build_timestamp, *engine_type, *engine_name, *engine_version,
             *blorb_pattern, *acceptor_pattern, *author_name, *author_email,
             *engine_home_url, *builder_name, *builder_email,
             *engine_description, *engine_copyright;


  terps_get_interpreter (terp,
                         &version, &build_timestamp, &engine_type,
                         &engine_name, &engine_version, &blorb_pattern,
                         &acceptor_offset, &acceptor_length, &acceptor_pattern,
                         &author_name, &author_email, &engine_home_url,
                         &builder_name, &builder_email, &engine_description,
                         &engine_copyright);
  assert (engine_name);

  glk_set_style (style_Subheader);
  glk_c_put_string (engine_name);
  glk_set_style (style_Normal);

  if (engine_version)
    {
      glk_c_put_string (" version ");
      glk_c_put_string (engine_version);
    }

  if (engine_type)
    {
      glk_c_put_string (" (");
      glk_c_put_string (engine_type);
      glk_put_char (')');
    }
  glk_put_char ('\n');

  if (author_name)
    {
      glk_c_put_string ("Author: ");
      glk_set_style (style_Emphasized);
      glk_c_put_string (author_name);
      glk_set_style (style_Normal);

      if (author_email)
        {
          glk_set_style (style_Emphasized);
          glk_c_put_string (" <");
          glk_c_put_string (author_email);
          glk_put_char ('>');
          glk_set_style (style_Normal);
        }
    }

  if (build_timestamp)
    {
      glk_c_put_string ("  Built: ");
      glk_set_style (style_Emphasized);
      terppage_convert_date (build_timestamp);
      glk_set_style (style_Normal);
    }

  if (author_name || build_timestamp)
    glk_put_char ('\n');

  if (display_show_interpreters_in_full ())
    {
      if (engine_description)
        glk_c_put_string (engine_description);

      if (engine_copyright)
        {
          glk_set_style (style_Emphasized);
          glk_c_put_string ("Licensing: ");
          glk_set_style (style_Normal);
          glk_c_put_string (engine_copyright);
        }

      if (engine_home_url)
        {
          glk_c_put_string ("Home Page: ");
          glk_set_style (style_Emphasized);
          glk_c_put_string (engine_home_url);
          glk_set_style (style_Normal);
          glk_put_char ('\n');
        }
    }

  glk_put_char ('\n');
}


/*
 * terppage_compare()
 *
 * Comparison function and helper for qsort.  Sorts by engine name only.
 */
static int
terppage_compare (const void *left_ptr, const void *right_ptr)
{
  terpref_t left = *(terpref_t*)left_ptr, right = *(terpref_t*)right_ptr;

  return strcmp (terps_get_interpreter_engine_name (left),
                 terps_get_interpreter_engine_name (right));
}


/*
 * terppage_interpreters()
 *
 * Print details of each interpreter discovered on the system.
 */
static void
terppage_interpreters (void)
{
  int index_;
  vectorref_t terps;
  terpref_t terp;

  terps = vector_create (sizeof (terpref_t));

  for (terp = terps_iterate (NULL); terp; terp = terps_iterate (terp))
    vector_append (terps, &terp);

  if (vector_get_length (terps) == 0)
    {
      glk_c_put_string ("Gamebox found no interpreters.  Sorry.\n\n");
      vector_destroy (terps);
      return;
    }

  glk_c_put_string ("Gamebox found the following interpreter engines available"
                    " on your system:\n\n");

  qsort ((void *) vector_get_address (terps, 0),
         vector_get_length (terps),
         sizeof (terpref_t), terppage_compare);

  for (index_ = 0; index_ < vector_get_length (terps); index_++)
    {
      vector_get (terps, index_, &terp);
      terppage_interpreter_details (terp);
    }

  vector_destroy (terps);
}


/*
 * terppage_preamble()
 *
 * Print the interpreters display page preamble and postamble.
 */
static void
terppage_preamble (void)
{
  if (terps_is_empty ())
    {
      glk_c_put_string (
        "You do not have any interpreter plugins available at the moment."
        " Try setting a value for the environment variable IF_PLUGIN_PATH,"
        " or if you have set a value, please check it.  Without plugins,"
        " Gamebox will not be able to play any Interactive Fiction games.\n\n");
    }
}


/*
 * terppage_display()
 *
 * Repaginate the complete details of each interpreter discovered.
 */
void
terppage_display (void)
{
  terppage_preamble ();
  terppage_interpreters ();
}
