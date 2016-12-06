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
 * USA.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <glk.h>
#include <ifp_internal.h>

#include "protos.h"


/*
 * Constants representing display actions.  These take negative values so
 * that they map (cast) into glui32 numbers at the highest end of the range
 * of hyperlinks.  These same constant values are used to represent states.
 */
enum { DISPLAY_ACTION_NULL = -1,
       DISPLAY_ACTION_CLOSE = -2, DISPLAY_ACTION_BACK = -3,
       DISPLAY_ACTION_GOTO = -4,
       DISPLAY_SORT_BY_TITLE = -5, DISPLAY_SORT_BY_AUTHOR = -6,
       DISPLAY_SORT_BY_GENRE = -7,
       DISPLAY_INFO_FULL = -8, DISPLAY_INFO_BRIEF = -9,
       DISPLAY_SELECT_GAMES = -10, DISPLAY_SELECT_INTERPRETERS = -11,
       DISPLAY_SELECT_ABOUT = -12,
       DISPLAY_PRIOR_PAGE = -13, DISPLAY_NEXT_PAGE = -14 };

/* States for display modes and sorting.*/
enum { PAGE_GAMES, PAGE_INTERPRETERS, PAGE_ABOUT };
static int display_selected = PAGE_GAMES;
static int display_games_sort_by = DISPLAY_SORT_BY_TITLE;
static int display_games_full_brief = DISPLAY_INFO_FULL;
static int display_interpreters_full_brief = DISPLAY_INFO_FULL;

/*
 * This module uses a vector to map game/group ids to sequence numbers for
 * menus and hyperlinks.  The mapped value is the vector index as returned
 * by vector_append(), plus one to move into the range 1..N inclusive.
 */
static vectorref_t display_map = NULL;

/* Main display window, may be split for messages and status line. */
static winid_t main_window = NULL;

/*
 * Non-hyperlinking libraries might use a top line status window to help
 * out when indicating what the current display settings are.  To redraw
 * the window, we also need an endpoint static.
 */
static winid_t status_window = NULL;
static int status_is_endpoint = FALSE;


/*
 * display_sort_games_by_author()
 * display_sort_games_by_genre()
 * display_show_games_in_full()
 * display_show_interpreters_in_full()
 *
 * Protected, return the current sort and information level settings.
 */
int
display_sort_games_by_author (void)
{
  return display_games_sort_by == DISPLAY_SORT_BY_AUTHOR;
}

int
display_sort_games_by_genre (void)
{
  return display_games_sort_by == DISPLAY_SORT_BY_GENRE;
}

int
display_show_games_in_full (void)
{
  return display_games_full_brief == DISPLAY_INFO_FULL;
}

int
display_show_interpreters_in_full (void)
{
  return display_interpreters_full_brief == DISPLAY_INFO_FULL;
}


/*
 * display_get_pagination_codes()
 *
 * Protected, return the prior and next page action codes.
 */
void
display_get_pagination_codes (int *prior_page_code, int *next_page_code)
{
  if (prior_page_code)
    *prior_page_code = DISPLAY_PRIOR_PAGE;
  if (next_page_code)
    *next_page_code = DISPLAY_NEXT_PAGE;
}


/*
 * display_button()
 *
 * Function to display pseudo-buttons.
 */
void
display_button (int hyperlink, const char *legend, int is_active)
{
  assert (legend);

  if (strlen (legend) > 1)
    {
      glk_put_char ('[');
      glk_set_hyperlink (is_active ? (glui32) hyperlink : 0);
      glk_c_put_string (legend);
      glk_set_hyperlink (0);
      glk_put_char (']');
    }
  else
    {
      glk_set_hyperlink (is_active ? (glui32) hyperlink : 0);
      glk_put_char ('[');
      glk_c_put_string (legend);
      glk_put_char (']');
      glk_set_hyperlink (0);
    }
}


/*
 * display_toolbar()
 *
 * Construct an approximation of a toolbar using hyperlinks.
 */
static void
display_toolbar (int is_endpoint)
{
  int sort_by, full_brief;
  int back_active;
  int by_title_active, by_author_active, by_genre_active;
  int full_active, brief_active;

  display_button (DISPLAY_ACTION_CLOSE, "X", TRUE);

  back_active = FALSE;
  by_title_active = by_author_active = by_genre_active = FALSE;
  full_active = brief_active = FALSE;

  switch (display_selected)
    {
    case PAGE_GAMES:
      sort_by = display_games_sort_by;
      full_brief = display_games_full_brief;

      back_active = !is_endpoint;
      by_title_active = !(sort_by == DISPLAY_SORT_BY_TITLE);
      by_author_active = !(sort_by == DISPLAY_SORT_BY_AUTHOR);
      by_genre_active = !(sort_by == DISPLAY_SORT_BY_GENRE);
      full_active = !(full_brief == DISPLAY_INFO_FULL);
      brief_active = !(full_brief == DISPLAY_INFO_BRIEF);
      break;

    case PAGE_INTERPRETERS:
      full_brief = display_interpreters_full_brief;

      full_active = !(full_brief == DISPLAY_INFO_FULL);
      brief_active = !(full_brief == DISPLAY_INFO_BRIEF);
      break;

    case PAGE_ABOUT:
      break;
    }

  display_button (DISPLAY_ACTION_BACK, "<<", back_active);
  display_button (DISPLAY_ACTION_GOTO, "GoTo", TRUE);

  glk_c_put_string ("  Sort by: ");
  display_button (DISPLAY_SORT_BY_TITLE, "Title", by_title_active);
  display_button (DISPLAY_SORT_BY_AUTHOR, "Author", by_author_active);
  display_button (DISPLAY_SORT_BY_GENRE, "Genre", by_genre_active);

  glk_c_put_string ("  Detail: ");
  display_button (DISPLAY_INFO_FULL, "Full", full_active);
  display_button (DISPLAY_INFO_BRIEF, "Brief", brief_active);

  glk_c_put_string ("  ");
  display_button (DISPLAY_SELECT_GAMES, "Games",
                  !(display_selected == PAGE_GAMES));
  display_button (DISPLAY_SELECT_INTERPRETERS, "Interpreters",
                  !(display_selected == PAGE_INTERPRETERS));
  display_button (DISPLAY_SELECT_ABOUT, "About",
                  !(display_selected == PAGE_ABOUT));
  glk_put_char ('\n');
}


/*
 * display_status()
 * display_handle_redraw()
 *
 * Print a one line status summary for non-hyperlinking Glk libraries.  If
 * the Glk library won't open an upper window, we give up silently.
 */
static void
display_status (int is_endpoint)
{
  if (!status_window)
    {
      status_window = glk_window_open (main_window,
                                       winmethod_Above|winmethod_Fixed,
                                       1, wintype_TextGrid, 0);
      if (!status_window)
        return;
    }

  glk_set_window (status_window);

  glk_window_clear (status_window);
  glk_c_put_string (" Gamebox 0.4");

  switch (display_selected)
    {
    case PAGE_GAMES:
      glk_c_put_string (" | Games page");
      switch (display_games_sort_by)
        {
        case DISPLAY_SORT_BY_TITLE:
          glk_c_put_string (" | Sorted by: Title");
          break;

        case DISPLAY_SORT_BY_AUTHOR:
          glk_c_put_string (" | Sorted by: Author");
          break;

        case DISPLAY_SORT_BY_GENRE:
          glk_c_put_string (" | Sorted by: Genre");
          break;
        }

      switch (display_games_full_brief)
        {
        case DISPLAY_INFO_FULL:
          glk_c_put_string (" | Detail: Full");
          break;

        case DISPLAY_INFO_BRIEF:
          glk_c_put_string (" | Detail: Brief");
          break;
        }

      if (is_endpoint)
        glk_c_put_string (" | Menu top");

      status_is_endpoint = is_endpoint;
      break;

    case PAGE_INTERPRETERS:
      glk_c_put_string (" | Interpreters page");

      switch (display_interpreters_full_brief)
        {
        case DISPLAY_INFO_FULL:
          glk_c_put_string (" | Detail: Full");
          break;

        case DISPLAY_INFO_BRIEF:
          glk_c_put_string (" | Detail: Brief");
          break;
        }

      break;

    case PAGE_ABOUT:
      glk_c_put_string (" | About page");
      break;
    }

  glk_set_window (main_window);
}

void
display_handle_redraw (void)
{
  if (status_window)
    display_status (status_is_endpoint);
}


/*
 * display_page()
 *
 * Rebuild the complete window set display to show whatever page is currently
 * selected.
 */
static void
display_page (const groupref_t group, int page_increment, int is_endpoint)
{
  int has_hyperlinks;
  assert (group);

  has_hyperlinks = glk_gestalt (gestalt_Hyperlinks, 0)
    && glk_gestalt (gestalt_HyperlinkInput, glk_window_get_type (main_window));

  if (display_map)
    vector_clear (display_map);
  else
    display_map = vector_create (sizeof (glui32));

  if (has_hyperlinks)
    display_toolbar (is_endpoint);
  else
    display_status (is_endpoint);

  switch (display_selected)
    {
    case PAGE_GAMES:
      glk_set_style (style_Header);
      glk_c_put_string ("\n\n        Gamebox - Games\n\n");
      glk_set_style (style_Normal);

      gamepage_display (group, display_map,
                        has_hyperlinks, page_increment, is_endpoint);
      break;

    case PAGE_INTERPRETERS:
      glk_set_style (style_Header);
      glk_c_put_string ("\n\n        Gamebox - Interpreters\n\n");
      glk_set_style (style_Normal);

      terppage_display ();
      break;

    case PAGE_ABOUT:
      glk_set_style (style_Header);
      glk_c_put_string ("\n\n        Gamebox - About\n\n");
      glk_set_style (style_Normal);

      aboutpage_display (has_hyperlinks);
      break;
    }
}


/*
 * display_get_user_action_hyperlinks()
 * display_get_user_action_no_hyperlinks()
 * display_get_user_action()
 *
 * Return a user menu selection, or other dialog action value.  If hyperlinks
 * are supported, this works by mouse input with some keyboard "accelerators".
 * If not, it uses a single line of input at the base of the dialog.
 */
static glui32
display_get_user_action_hyperlinks (void)
{
  event_t event;
  glui32 user_action, mapping;

  user_action = DISPLAY_ACTION_NULL;

  glk_request_hyperlink_event (main_window);
  glk_request_char_event (main_window);

  do
    {
      glk_select (&event);
      if (event.type == evtype_CharInput)
        {
          switch (event.val1)
            {
            case 'q': case 'Q': case keycode_Escape:
              user_action = DISPLAY_ACTION_CLOSE;
              break;

            case 'u': case 'U': case 'b': case 'B': case keycode_Left:
              user_action = DISPLAY_ACTION_BACK;
              break;

            case 'o': case 'O': case keycode_Func9:
              user_action = DISPLAY_ACTION_GOTO;
              break;

            case '-': case 'k': case 'K': case keycode_Up:
              user_action = DISPLAY_PRIOR_PAGE;
              break;

            case '+': case 'j': case 'J': case keycode_Down:
              user_action = DISPLAY_NEXT_PAGE;
              break;

            case 't': case 'T': case keycode_Func6:
              display_games_sort_by = DISPLAY_SORT_BY_TITLE;
              break;

            case 'a': case 'A': case keycode_Func7:
              display_games_sort_by = DISPLAY_SORT_BY_AUTHOR;
              break;

            case 'e': case 'E': case keycode_Func8:
              display_games_sort_by = DISPLAY_SORT_BY_GENRE;
              break;

            case 'f': case 'F': case keycode_Func4:
              switch (display_selected)
                {
                case PAGE_GAMES:
                  display_games_full_brief = DISPLAY_INFO_FULL;
                  break;

                case PAGE_INTERPRETERS:
                  display_interpreters_full_brief = DISPLAY_INFO_FULL;
                  break;
                }
              break;

            case 's': case 'S': case keycode_Func5:
              switch (display_selected)
                {
                case PAGE_GAMES:
                  display_games_full_brief = DISPLAY_INFO_BRIEF;
                  break;

                case PAGE_INTERPRETERS:
                  display_interpreters_full_brief = DISPLAY_INFO_BRIEF;
                  break;
                }
              break;

            case 'g': case 'G': case keycode_Func2:
              display_selected = PAGE_GAMES;
              break;

            case 'i': case 'I': case keycode_Func3:
              display_selected = PAGE_INTERPRETERS;
              break;

            case 'h': case 'H': case '?': case keycode_Func1:
              display_selected = PAGE_ABOUT;
              break;

            case 'n': case 'N': case keycode_Tab: case keycode_Func12:
              switch (display_selected)
                {
                case PAGE_GAMES:
                  display_selected = PAGE_INTERPRETERS;
                  break;

                case PAGE_INTERPRETERS:
                  display_selected = PAGE_ABOUT;
                  break;

                case PAGE_ABOUT:
                  display_selected = PAGE_GAMES;
                  break;
                }
              break;

            default:
              glk_request_char_event (main_window);
              continue;
            }

          glk_cancel_hyperlink_event (main_window);
          break;
        }
      else if (event.type == evtype_Hyperlink)
        {
          switch (event.val1)
            {
            case DISPLAY_ACTION_CLOSE:
            case DISPLAY_ACTION_BACK:
            case DISPLAY_ACTION_GOTO:
              user_action = event.val1;
              break;

            case DISPLAY_SORT_BY_TITLE:
            case DISPLAY_SORT_BY_AUTHOR:
            case DISPLAY_SORT_BY_GENRE:
              display_games_sort_by = event.val1;
              break;

            case DISPLAY_INFO_FULL:
            case DISPLAY_INFO_BRIEF:
              if (display_selected == PAGE_GAMES)
                display_games_full_brief = event.val1;
              else
                display_interpreters_full_brief = event.val1;
              break;

            case DISPLAY_SELECT_GAMES:
              display_selected = PAGE_GAMES;
              break;

            case DISPLAY_SELECT_INTERPRETERS:
              display_selected = PAGE_INTERPRETERS;
              break;

            case DISPLAY_SELECT_ABOUT:
              display_selected = PAGE_ABOUT;
              break;
            }

          if (user_action == (glui32) DISPLAY_ACTION_NULL)
            user_action = event.val1;

          glk_cancel_char_event (main_window);
          break;
        }
    }
  while (TRUE);

  mapping = user_action - 1;
  if (mapping < (glui32) vector_get_length (display_map))
    {
      glui32 id;

      vector_get (display_map, mapping, &id);
      return id;
    }

  return user_action;
}

static glui32
display_get_user_action_no_hyperlinks (void)
{
  while (TRUE)
    {
      static char buffer[32] = "";
      const char *prompt;
      char first;
      glui32 user_action, mapping;

      user_action = DISPLAY_ACTION_NULL;

      if (display_selected == PAGE_GAMES)
        prompt = "Choose a game or category, or enter 'h' for help: ";
      else
        prompt = "Enter a command, or 'h' for help: ";

      message_read_line (prompt, buffer, sizeof (buffer));

      first = glk_char_to_lower (buffer[0]);
      switch (first)
        {
        case 'q':
          user_action = DISPLAY_ACTION_CLOSE;
          break;

        case 'u': case 'b':
          switch (display_selected)
            {
            case PAGE_GAMES:
              user_action = DISPLAY_ACTION_BACK;
              break;

            default:
              message_write_line (FALSE, "That is not valid in this page.");
              continue;
            }
          break;

        case 'o':
          user_action = DISPLAY_ACTION_GOTO;
          break;

        case '-': case 'k':
          user_action = DISPLAY_PRIOR_PAGE;
          break;

        case '+': case 'j':
          user_action = DISPLAY_NEXT_PAGE;
          break;

        case 't': case 'a': case 'e':
          switch (display_selected)
            {
            case PAGE_GAMES:
              switch (first)
                {
                case 't':
                  display_games_sort_by = DISPLAY_SORT_BY_TITLE;
                  break;

                case 'a':
                  display_games_sort_by = DISPLAY_SORT_BY_AUTHOR;
                  break;

                case 'e':
                  display_games_sort_by = DISPLAY_SORT_BY_GENRE;
                  break;
                }
              break;

            default:
              message_write_line (FALSE, "That is not valid in this page.");
              continue;
            }
          break;

        case 'f':
          switch (display_selected)
            {
            case PAGE_GAMES:
              display_games_full_brief = DISPLAY_INFO_FULL;
              break;

            case PAGE_INTERPRETERS:
              display_interpreters_full_brief = DISPLAY_INFO_FULL;
              break;

            default:
              message_write_line (FALSE, "That is not valid in this page.");
              continue;
            }
          break;

        case 's':
          switch (display_selected)
            {
            case PAGE_GAMES:
              display_games_full_brief = DISPLAY_INFO_BRIEF;
              break;

            case PAGE_INTERPRETERS:
              display_interpreters_full_brief = DISPLAY_INFO_BRIEF;
              break;

            default:
              message_write_line (FALSE, "That is not valid in this page.");
              continue;
            }
          break;

        case 'g':
          display_selected = PAGE_GAMES;
          break;

        case 'i':
          display_selected = PAGE_INTERPRETERS;
          break;

        case 'h': case '?':
          display_selected = PAGE_ABOUT;
          break;

        case 'n':
          switch (display_selected)
            {
            case PAGE_GAMES:
              display_selected = PAGE_INTERPRETERS;
              break;

            case PAGE_INTERPRETERS:
              display_selected = PAGE_ABOUT;
              break;

            case PAGE_ABOUT:
              display_selected = PAGE_GAMES;
              break;
            }
          break;
        }

      if (strchr ("qubo-+jktaefsgih?n", first))
        {
          buffer[0] = '\0';
          return user_action;
        }

      mapping = strtoul (buffer, NULL, 10) - 1;
      if (mapping < (glui32) vector_get_length (display_map))
        {
          glui32 id;

          vector_get (display_map, mapping, &id);
          if (gamegroup_id_to_group (id) || gameset_id_to_game (id))
            {
              buffer[0] = '\0';
              return id;
            }
        }

      if (buffer[0])
        {
          message_write_line (FALSE, (mapping + 1) == 0
                              ? "That is not a valid game identifier."
                              : "The collection does not contain that entry.");
        }
    }
}

static glui32
display_get_user_action (void)
{
  int has_hyperlinks;

  has_hyperlinks = glk_gestalt (gestalt_Hyperlinks, 0)
    && glk_gestalt (gestalt_HyperlinkInput, glk_window_get_type (main_window));

  return has_hyperlinks ? display_get_user_action_hyperlinks ()
                        : display_get_user_action_no_hyperlinks ();
}


/*
 * display_attempt_game_run()
 *
 * Locate, download, and if successful, run a given game.
 */
static void
display_attempt_game_run (const char *location)
{
  ifp_urlref_t url;
  int url_errno;
  ifp_pluginref_t plugin;

  /* Convert the location into a file or URL path, and then into a URL. */
  url = url_resolve (main_window, location, &url_errno);
  if (!url)
    {
      if (url_errno == EINTR)
        message_write_line (FALSE, "The URL download was canceled.");
      else
        message_write_line (FALSE, "Invalid file path or URL [%s].",
                            strerror (url_errno));
      return;
    }

  /* Find, and if found, run a plugin for this location. */
  plugin = ifp_manager_locate_plugin_url (url);
  if (plugin)
    {
      ifp_chain_set_chained_plugin (plugin);
      ifp_manager_run_plugin (plugin);
      ifp_chain_set_chained_plugin (NULL);

      message_wait_for_keypress ();
    }
  else
    message_write_line (FALSE, "No plugin engine accepted the file or URL.");

  /*
   * Drop all plugins to force rescan on the next game.  This helps preserve
   * the "using first found" ordering; without this, when multiple plugins
   * match game data, IFP will select between them in cyclical order, not
   * really what we want to have happen.
   *
   * Legion has this problem too, at least into IFP 1.3.  It's an IFP buglet.
   */
  ifp_loader_forget_all_plugins ();
}


/*
 * display_main_loop()
 *
 * Display pages and handle user input requests.  Returns when the user
 * quits the dialog.
 */
void
display_main_loop (void)
{
  groupref_t current_group;
  vectorref_t groups;
  glui32 user_action;

  /* Begin navigating at the root node of the groups tree. */
  current_group = gamegroup_get_root ();
  assert (current_group);
  groups = vector_create (sizeof (current_group));

  user_action = (glui32) DISPLAY_ACTION_NULL;

  /* Loop until user requests dialog close. */
  while (user_action != (glui32) DISPLAY_ACTION_CLOSE)
    {
      int page_increment, is_endpoint;

      /* Destroy all current windows, and create a new main window. */
      main_window = glk_window_get_root ();
      if (main_window)
        {
          glk_window_close (main_window, NULL);
          status_window = NULL;
          message_windows_closed ();
        }

      main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
      if (!main_window)
        {
          fprintf (stderr, "GLK INTERNAL ERROR: can't open main window.\n");
          break;
        }

      glk_set_window (main_window);
      glk_window_clear (main_window);

      /*
       * Set any page increment (or decrement) depending on the last user
       * action requested, and note if this is an endpoint page, then display
       * the current page.
       */
      page_increment = user_action == (glui32) DISPLAY_PRIOR_PAGE ? -1
                       : user_action == (glui32) DISPLAY_NEXT_PAGE ? 1: 0,
      is_endpoint = vector_get_length (groups) == 0;
      display_page (current_group, page_increment, is_endpoint);

      /* Get user input, and handle any special action values. */
      user_action = display_get_user_action ();

      if (user_action == (glui32) DISPLAY_ACTION_CLOSE)
        {
          if (!message_confirm ("Close Gamebox? [y/n]"))
            user_action = DISPLAY_ACTION_NULL;
        }

      else if (user_action == (glui32) DISPLAY_ACTION_BACK)
        {
          if (vector_get_length (groups) > 0)
            vector_remove (groups, &current_group);
          else
            message_write_line (FALSE,
                                "You are already at the top level category.");
        }

      else if (user_action == (glui32) DISPLAY_ACTION_GOTO)
        {
          static char path[1024] = "";

          message_read_line ("Enter a file path or URL: ", path, sizeof (path));

          if (strlen (path) > 0)
            display_attempt_game_run (path);
        }

      /* If this is a group, navigate to it and redisplay. */
      else if (gamegroup_id_to_group (user_action))
        {
          vector_append (groups, &current_group);
          current_group = gamegroup_id_to_group (user_action);
        }

      /* If this is a game, try running it. */
      else if (gameset_id_to_game (user_action))
        {
          gameref_t game;

          game = gameset_id_to_game (user_action);
          display_attempt_game_run (gameset_get_game_location (game));
        }
    }

  /*
   * Clean up and return.  Here we need to take care to unload any and all
   * loaded plugins in our loader instance, to avoid the system's runtime
   * linker reference counts becoming skewed.
   */
  ifp_loader_forget_all_plugins ();

  vector_destroy (groups);
  if (display_map)
    {
      vector_destroy (display_map);
      display_map = NULL;
    }
}
