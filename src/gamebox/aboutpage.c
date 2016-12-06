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
#include <stdio.h>

#include <glk.h>

#include "protos.h"


/*
 * aboutpage_preamble()
 *
 * Print the about page preamble.
 */
static void
aboutpage_preamble (int has_hyperlinks)
{
  if (has_hyperlinks)
    {
      glk_c_put_string (
        "To close Gamebox, use the [X] hyperlink at the top of the page.  To"
        " run a game directly, giving its path or URL, use the [GoTo]"
        " hyperlink.  To select the Games, Interpreters, and About pages, use"
        " the corresponding [Games], [Interpreters], and [About] hyperlinks."
        "  These hyperlinks are always active.\n\n");

      glk_c_put_string (
        "To return to the previous games category, use the [<<] hyperlink.  To"
        " sort games by title, author, or genre, use the corresponding Sort by:"
        " hyperlinks.  These hyperlinks are active only on the Games"
        " page.\n\n");

      glk_c_put_string ("To select full or brief game and interpreter"
        " information, use the Detail: hyperlinks.  These hyperlinks are"
        " active on both the Games and Interpreter pages.\n\n");

      glk_c_put_string (
        "You can also use following keyboard accelerators in Gamebox:\n\n");

      glk_set_style (style_Preformatted);
      glk_c_put_string (
        "  q, Escape         - Close Gamebox\n"
        "  b, u, Left Arrow  - Return to the previous games category\n"
        "  o, Fkey_9         - Run a game directly from its path or URL\n"
        "  t, Fkey_6         - Sort games by title\n"
        "  a, Fkey_7         - Sort games by author\n"
        "  e, Fkey_8         - Sort games by genre\n");
      glk_c_put_string (
        "  f, Fkey_4         - Show full games and interpreters information\n"
        "  s, Fkey_5         - Show brief games and interpreters information\n"
        "  g, Fkey_2         - Show the Games page\n"
        "  -, +, Up/Down     - Page up and page down within the Games page\n"
        "  i, Fkey_3         - Show the Interpreters page\n"
        "  h, ?, Fkey_1      - Show the About page (this page)\n"
        "  Tab, Fkey_12      - Cycle round all pages\n");
      glk_set_style (style_Normal);
      glk_put_char ('\n');

      glk_c_put_string (
        "Some Glk libraries may not respond to all of the function and other"
        " special keys listed above.\n\n");
    }
  else
    {
      glk_c_put_string (
        "Gamebox is using a Glk library that does not offer hyperlinks, so"
        " you cannot use the mouse to select games and options.  Instead,"
        " Gamebox will prompt you to enter the number of the game you want"
        " to play, or the category you want to display.\n\n");

      glk_c_put_string (
        "As well as entering a game or game category, you can use simple single"
        " character commands to close Gamebox, return to a previous category,"
        " run a game directly from its path or URL, sort games by Title,"
        " Author, or Genre, select the Games, Interpreters, or About pages,"
        " and set information detail to Full or Brief.\n\n");

      glk_c_put_string (
        "Gamebox will display its current settings in an upper status window,"
        " except where the Glk library it is using is extremely limited.\n\n");

      glk_c_put_string (
        "Gamebox understands the following commands:\n\n");

      glk_set_style (style_Preformatted);
      glk_c_put_string (
        "  q,          - Close Gamebox\n"
        "  b, u        - Return to the previous games category\n"
        "  o,          - Run a game directly from its path or URL\n"
        "  t,          - Sort games by title\n"
        "  a,          - Sort games by author\n"
        "  e,          - Sort games by genre\n"
        "  f,          - Show full game and interpreter details\n"
        "  s,          - Show brief game and interpreter details\n"
        "  g,          - Show the Games page\n"
        "  -, +, k, j  - Page up and page down within the Games page\n"
        "  i,          - Show the Interpreters page\n"
        "  h, ?        - Show the About page (this page)\n"
        "  n           - Cycle round all pages\n");
      glk_set_style (style_Normal);
      glk_put_char ('\n');
    }
}


/*
 * aboutpage_environment()
 *
 * Print assorted details about the environment.
 */
static void
aboutpage_environment (void)
{
  glui32 version;
  int has_timers, has_graphics, has_sound, has_hyperlinks, has_unicode;
  char buffer[64];

  version = glk_gestalt (gestalt_Version, 0);
  glk_c_put_string ("Gamebox is using a version ");

  snprintf (buffer, sizeof (buffer), "%lu.%lu.%lu",
            version >> 16, (version >> 8) & 0xff, version & 0xff);
  glk_c_put_string (buffer);

  has_timers = glk_gestalt (gestalt_Timer, 0);
  has_graphics = glk_gestalt (gestalt_Graphics, 0);
  has_sound = glk_gestalt (gestalt_Sound, 0);
  has_hyperlinks = glk_gestalt (gestalt_Hyperlinks, 0);
  has_unicode = glk_gestalt (gestalt_Unicode, 0);

  glk_c_put_string (" Glk library, supporting ");
  glk_c_put_string (has_timers ? "timers, " : "no timers, ");
  glk_c_put_string (has_graphics ? "graphics, " : "no graphics, ");
  glk_c_put_string (has_sound ? "sound, " : "no sound, ");
  glk_c_put_string (has_hyperlinks ? "hyperlinks, " : "no hyperlinks, ");
  glk_c_put_string (has_unicode
                    ? "and unicode.\n\n" : "and no unicode.\n\n");

  glk_c_put_string ("This is Gamebox 0.4, built on "
                    __DATE__ " " __TIME__ ".\n\n");
}


/*
 * aboutpage_license()
 *
 * Print Gamebox's licensing.
 */
static void
aboutpage_license (void)
{
  glk_set_style (style_Subheader);
  glk_c_put_string ("Gamebox");
  glk_set_style (style_Normal);
  glk_c_put_string (" is ");
  glk_set_style (style_Emphasized);
  glk_c_put_string ("copyright (C) 2006-2007"
                    "  Simon Baldwin (simon_baldwin@yahoo.com)");
  glk_set_style (style_Normal);
  glk_put_char ('\n');

  glk_c_put_string (
    "This program is free software; you can redistribute it and/or modify it"
    " under the terms of the GNU General Public License as published by the"
    " Free Software Foundation; either version 2 of the License, or (at your"
    " option) any later version.\n\n");

  glk_c_put_string ("This program is distributed in the hope that it will be"
                    " useful, but ");
  glk_set_style (style_Subheader);
  glk_c_put_string ("WITHOUT ANY WARRANTY");
  glk_set_style (style_Normal);
  glk_c_put_string ("; without even the implied warranty of ");
  glk_set_style (style_Subheader);
  glk_c_put_string ("MERCHANTABILITY");
  glk_set_style (style_Normal);
  glk_c_put_string (" or ");
  glk_set_style (style_Subheader);
  glk_c_put_string ("FITNESS FOR A PARTICULAR PURPOSE");
  glk_set_style (style_Normal);
  glk_c_put_string (".  See the GNU General Public License for more details.");
  glk_c_put_string ("\n\n");

  glk_c_put_string (
    "You should have received a copy of the GNU General Public License"
    " along with this program; if not, write to the Free Software"
    " Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307"
    " USA\n\n");

  glk_c_put_string ("Please report any bugs, omissions, or misfeatures to ");
  glk_set_style (style_Emphasized);
  glk_c_put_string ("simon_baldwin@yahoo.com");
  glk_set_style (style_Normal);
  glk_c_put_string (".\n");
}


/*
 * aboutpage_display()
 *
 * Display a fresh about page.
 */
void
aboutpage_display (int has_hyperlinks)
{
  aboutpage_preamble (has_hyperlinks);
  aboutpage_environment ();
  aboutpage_license ();
}
