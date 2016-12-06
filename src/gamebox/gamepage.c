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


/* Number of games or groups listed on a single page. */
static const int GAMEPAGE_ITEMS_PER_PAGE = 20;


/*
 * gamepage_convert_date()
 *
 * Display an ISO 8601 date, converted where possible to month-day-year
 * format (e.g. Aug 23 2005), on the current Glk stream.
 */
static void
gamepage_convert_date (const char *iso_date)
{
  int year, month, day, count;
  char buffer[32], trailer;

  /* Split into year, month, and day. */
  count = sscanf (iso_date, " %04d-%02d-%02d%c", &year, &month, &day, &trailer);
  if (!(count == 3 || (count == 4 && trailer == ' ')))
    {
      day = 0;
      count = sscanf (iso_date, " %04d-%02d%c", &year, &month, &trailer);
      if (!(count == 2 || (count == 3 && trailer == ' ')))
        {
          month = 0;
          count = sscanf (iso_date, " %04d%c", &year, &trailer);
          if (!(count == 1 || (count == 2 && trailer == ' ')))
            {
              /* Not formatted correctly, so just display original value. */
              glk_c_put_string (iso_date);
              return;
            }
        }
    }

  /*
   * Basic reasonableness check; not full or exhaustive.  Month and day of
   * zero are okay -- these indicate missing items in the input date.
   */
  if (month < 0 || month > 12
      || day < 0 || day > 31)
    {
      glk_c_put_string (iso_date);
      return;
    }

  /* Redisplay as a more normal date. */
  switch (month)
    {
      case  1: glk_c_put_string ("Jan "); break;
      case  2: glk_c_put_string ("Feb "); break;
      case  3: glk_c_put_string ("Mar "); break;
      case  4: glk_c_put_string ("Apr "); break;
      case  5: glk_c_put_string ("May "); break;
      case  6: glk_c_put_string ("Jun "); break;
      case  7: glk_c_put_string ("Jul "); break;
      case  8: glk_c_put_string ("Aug "); break;
      case  9: glk_c_put_string ("Sep "); break;
      case 10: glk_c_put_string ("Oct "); break;
      case 11: glk_c_put_string ("Nov "); break;
      case 12: glk_c_put_string ("Dec "); break;
    }
  if (day > 0)
    {
      snprintf (buffer, sizeof (buffer), "%2d ", day);
      glk_c_put_string (buffer);
    }
  snprintf (buffer, sizeof (buffer), "%04d", year);
  glk_c_put_string (buffer);
}


/*
 * gamepage_game_details()
 *
 * Write a game's details to the current Glk stream.
 */
static void
gamepage_game_details (const gameref_t game,
                       vectorref_t display_map, int has_hyperlinks)
{
  glui32 id, mapping;
  const char *about, *author, *byline, *description, *genre,
             *headline, *length, *publisher, *release_date,
             *title, *version;

  gameset_get_game (game,
                    &about, &author, &byline, &description, &genre,
                    &headline, &length, &publisher, &release_date,
                    &title, &version);
  assert (about && title);

  id = gameset_get_game_id (game);
  mapping = vector_append (display_map, &id);

  if (has_hyperlinks)
    {
      display_button (mapping + 1, ">>", TRUE);
      glk_c_put_string ("  ");
    }
  else
    {
      char buffer[32];

      snprintf (buffer, sizeof (buffer), "%lu.  ", mapping + 1);
      glk_c_put_string (buffer);
    }

  glk_set_style (style_Subheader);
  glk_c_put_string (title);
  glk_set_style (style_Normal);

  if (byline || author || publisher)
    {
      glk_put_char (' ');
      if (byline)
        glk_c_put_string (byline);
      else if (author)
        {
          glk_c_put_string ("by ");
          glk_c_put_string (author);
        }
      else
        {
          glk_c_put_string ("from ");
          glk_c_put_string (publisher);
        }
    }
  glk_put_char ('\n');

  if (display_show_games_in_full ())
    {
      if (genre)
        {
          glk_c_put_string ("Genre: ");
          glk_set_style (style_Emphasized);
          glk_c_put_string (genre);
          glk_set_style (style_Normal);
          glk_c_put_string ("  ");
        }

      if (version)
        {
          glk_c_put_string ("Version: ");
          glk_set_style (style_Emphasized);
          glk_c_put_string (version);
          glk_set_style (style_Normal);
          glk_c_put_string ("  ");
        }

      if (release_date)
        {
          glk_c_put_string ("Release Date: ");
          glk_set_style (style_Emphasized);
          gamepage_convert_date (release_date);
          glk_set_style (style_Normal);
          glk_c_put_string ("  ");
        }

      if (length)
        {
          glk_c_put_string ("Length: ");
          glk_set_style (style_Emphasized);
          glk_c_put_string (length);
          glk_set_style (style_Normal);
          glk_c_put_string ("  ");
        }

      if (genre || version || release_date || length)
        glk_put_char ('\n');

      if (description)
        {
          glk_c_put_string (description);
          glk_put_char ('\n');
        }
      else if (headline)
        {
          glk_c_put_string ("Brief Description: ");
          glk_c_put_string (headline);
          glk_put_char ('\n');
        }
    }

  glk_c_put_string ("Location: ");
  glk_set_style (style_Emphasized);
  glk_c_put_string (about);
  glk_set_style (style_Normal);
  glk_put_char ('\n');

  glk_put_char ('\n');
}


/*
 * gamepage_group_details()
 *
 * Write a group's details to the current Glk stream.
 */
static void
gamepage_group_details (const groupref_t group, vectorref_t display_map,
                        int has_hyperlinks)
{
  glui32 id, mapping;
  const char *title, *description;

  gamegroup_get_group (group, NULL, &title, &description);
  assert (title);

  id = gamegroup_get_group_id (group);
  mapping = vector_append (display_map, &id);

  if (has_hyperlinks)
    {
      display_button (mapping + 1, ">>", TRUE);
      glk_c_put_string ("  ");
    }
  else
    {
      char buffer[32];

      snprintf (buffer, sizeof (buffer), "%lu.  ", mapping + 1);
      glk_c_put_string (buffer);
    }

  glk_set_style (style_Subheader);
  glk_c_put_string (title);
  glk_set_style (style_Normal);
  glk_put_char ('\n');

  if (description)
    {
      glk_c_put_string (description);
      glk_put_char ('\n');
    }

  glk_put_char ('\n');
}


/*
 * gamepage_preamble()
 *
 * Print the overall game page preamble.
 */
static void
gamepage_preamble (int has_hyperlinks, int is_endpoint)
{
  glk_c_put_string (
    "Gamebox offers a browser-like interface to a collection of Interactive"
    " Fiction games.  Details of the collection are stored in XML or INI"
    " files.\n\n");

  if (has_hyperlinks)
    {
      glk_c_put_string (
        "To run a game or to view a different category, use the [>>] hyperlink"
        " alongside its list entry.  ");

      if (is_endpoint)
        glk_c_put_string (
          "To close Gamebox, use the [X] hyperlink at the top of the page, or"
          " the 'q' keyboard accelerator.  ");
      else
        glk_c_put_string (
          "To close Gamebox, or to return to the previous category, use the"
          " [X] or [<<] hyperlinks at the top of the page, or the 'q' or 'u'"
          " keyboard accelerators.  ");

      glk_c_put_string (
          "For multiple game pages, use the [<] and [>] hyperlinks to move"
          " between pages.  ");

      glk_c_put_string (
        "For more about Gamebox, use the [About] hyperlink or the '?' keyboard"
        " accelerator.\n\n");
    }
  else
    {
      glk_c_put_string (
        "To run a game or to view a different category, enter its number at"
        " the Gamebox prompt.  To close Gamebox, enter 'q' at the prompt.  ");

      if (!is_endpoint)
        glk_c_put_string (
          "To return to the previous category, enter 'u' at the prompt.  ");

      glk_c_put_string (
          "For multiple game pages, use '-' and '+' to move between pages.  ");

      glk_c_put_string ("For more about Gamebox, enter '?' at the prompt.\n\n");
    }
}


/*
 * gamepage_compare_get_primary()
 * gamepage_compare_get_secondary()
 * gamepage_compare()
 *
 * Comparison function and helpers for qsort.  Sorts group entries by the
 * primary sort field, and by game or group title within equal primaries.
 * Primary may be NULL, in which case any non-NULL is deemed greater.
 */
static const char *
gamepage_compare_get_primary (const noderef_t node)
{
  gameref_t game;

  game = gamegroup_get_node_game (node);
  if (game)
    {
      if (display_sort_games_by_author ())
        {
          const char *author;

          author = gameset_get_game_author (game);
          return author ? author : gameset_get_game_publisher (game);
        }
      else if (display_sort_games_by_genre ())
        return gameset_get_game_genre (game);

      return NULL;
    }

  assert (gamegroup_get_node_group (node));
  return NULL;
}

static const char *
gamepage_compare_get_secondary (const noderef_t node)
{
  gameref_t game;

  game = gamegroup_get_node_game (node);
  if (game)
    {
      const char *title;

      title = gameset_get_game_title (game);
      if (strncasecmp (title, "The ", 4) == 0)
        return title + 4;
      else if (strncasecmp (title, "An ", 3) == 0)
        return title + 3;
      else if (strncasecmp (title, "A ", 2) == 0)
        return title + 2;
      else
        return title;
    }
  else
    {
      groupref_t group;

      group = gamegroup_get_node_group (node);
      assert (group);
      return gamegroup_get_group_title (group);
    }
}

static int
gamepage_compare (const void *left_ptr, const void *right_ptr)
{
  noderef_t left = *(noderef_t*)left_ptr, right = *(noderef_t*)right_ptr;
  const char *compare, *with;

  compare = gamepage_compare_get_primary (left);
  with = gamepage_compare_get_primary (right);

  if (compare && with)
    {
      if (strcasecmp (compare, with) != 0)
        return strcasecmp (compare, with);
    }

  else if (compare && !with)
    return 1;
  else if (!compare && with)
    return -1;

  compare = gamepage_compare_get_secondary (left);
  with = gamepage_compare_get_secondary (right);
  assert (compare && with);

  return strcasecmp (compare, with);
}


/*
 * gamepage_gamegroup()
 *
 * Print details of each game and group in the given group.
 */
static void
gamepage_gamegroup (const groupref_t group, vectorref_t display_map,
                    int has_hyperlinks, int page_increment)
{
  static int page_number = 0;
  static groupref_t last_group = NULL;

  int has_games, has_groups, nodes_count, begin, end, index_;
  vectorref_t nodes;
  noderef_t node;

  if (group != last_group)
    {
      page_number = 0;
      last_group = group;
    }

  nodes = vector_create (sizeof (noderef_t));

  has_games = has_groups = FALSE;

  for (node = gamegroup_iterate_group (group, NULL);
       node; node = gamegroup_iterate_group (group, node))
    {
      has_games |= gamegroup_is_node_game (node);
      has_groups |= gamegroup_is_node_group (node);

      vector_append (nodes, &node);
    }

  glk_c_put_string ("The ");
  glk_set_style (style_Subheader);
  glk_c_put_string (gamegroup_get_group_title (group));
  glk_set_style (style_Normal);
  glk_c_put_string (" category");

  nodes_count = vector_get_length (nodes);
  if (nodes_count == 0)
    {
      glk_c_put_string (" contains no entries.  Sorry.\n\n");
      vector_destroy (nodes);
      return;
    }

  glk_c_put_string (" offers the following");
  if (has_games)
    glk_c_put_string (" games");
  if (has_games && has_groups)
    glk_c_put_string (" and");
  if (has_groups)
    glk_c_put_string (" categories");
  glk_c_put_string (":\n\n");

  qsort ((void *) vector_get_address (nodes, 0),
         nodes_count, sizeof (noderef_t), gamepage_compare);

  if (nodes_count > GAMEPAGE_ITEMS_PER_PAGE)
    {
      char buffer[32];
      int last_page;

      last_page = nodes_count / GAMEPAGE_ITEMS_PER_PAGE;
      page_number += page_increment;

      if (page_number < 0)
        page_number = 0;
      else if (page_number > last_page)
        page_number = last_page;

      glk_c_put_string ("Page ");
      snprintf (buffer, sizeof (buffer), "%d", page_number + 1);
      glk_c_put_string (buffer);
      glk_c_put_string (" of ");
      snprintf (buffer, sizeof (buffer), "%d", last_page + 1);
      glk_c_put_string (buffer);

      if (has_hyperlinks)
        {
          int prior_page_code, next_page_code;

          glk_c_put_string ("  ");

          display_get_pagination_codes (&prior_page_code, &next_page_code);
          display_button (prior_page_code, "<", page_number > 0);
          display_button (next_page_code, ">", page_number < last_page);
        }

      glk_c_put_string ("\n\n");
    }

  begin = page_number * GAMEPAGE_ITEMS_PER_PAGE;
  end = (page_number + 1) * GAMEPAGE_ITEMS_PER_PAGE;

  for (index_ = begin; index_ < end && index_ < nodes_count; index_++)
    {
      vector_get (nodes, index_, &node);

      if (gamegroup_is_node_game (node))
        {
          gameref_t entry;

          entry = gamegroup_get_node_game (node);
          gamepage_game_details (entry, display_map, has_hyperlinks);
        }

      else if (gamegroup_is_node_group (node))
        {
          groupref_t entry;

          entry = gamegroup_get_node_group (node);
          gamepage_group_details (entry, display_map, has_hyperlinks);
        }
    }

  if (nodes_count > GAMEPAGE_ITEMS_PER_PAGE
      && page_number < nodes_count / GAMEPAGE_ITEMS_PER_PAGE)
    glk_c_put_string ("More...\n");

  vector_destroy (nodes);
}


/*
 * gamepage_display()
 *
 * Repaginate the complete details of each game and group in the given group.
 */
void
gamepage_display (const groupref_t group, vectorref_t display_map,
                  int has_hyperlinks, int page_increment, int is_endpoint)
{
  gamepage_preamble (has_hyperlinks, is_endpoint);
  gamepage_gamegroup (group, display_map, has_hyperlinks, page_increment);
}
