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


/* Game magic identifier, and descriptor structure definition. */
static const unsigned int GAMESET_MAGIC = 0xaa641698;
struct game_s {
  unsigned int magic;

  char *about;         /* IFMES:    BAF:game about (the game file!) */
                       /* iFiction: annotation, gamebox, about, or url+IFID */
  char *author;        /* IFMES:    author (if no Byline) */
                       /* iFiction: bibliographic, author */
  char *byline;        /* IFMES:    byline (overrides author) */
                       /* iFiction: (not available, unused) */
  char *description;   /* IFMES:    desc (overrides headline) */
                       /* iFiction: bibliographic, description */
  char *genre;         /* IFMES:    genre */
                       /* iFiction: bibliographic, genre */
  char *headline;      /* IFMES:    headline (if no Desc) */
                       /* iFiction: bibliographic, headline */
  char *length;        /* IFMES:    length */
                       /* iFiction: (not available, unused) */
  char *publisher;     /* IFMES:    publisher (if no author) */
                       /* iFiction: bibliographic, group */
  char *release_date;  /* IFMES:    releaseDate */
                       /* iFiction: release, releasedate, or
                                    bibliographic, firstpublished (close) */
  char *title;         /* IFMES:    title */
                       /* iFiction: bibliographic, title */
  char *version;       /* IFMES:    version */
                       /* iFiction: (not available, unused) */

  glui32 digest;
  glui32 id;
  gameref_t next;
};

/* List of game descriptors, unordered. */
static gameref_t games_head = NULL;

/* Vector of games in existence, for fast lookup by game id. */
static vectorref_t games_vector = NULL;


/*
 * gameset_add_game()
 * gameset_is_game()
 * gameset_get_game()
 * gameset_iterate()
 * gameset_is_empty()
 * gameset_erase()
 *
 * Create, iterate through, check, and erase a growable array of games.
 * All fields except 'about' and 'title' may be NULL.
 */
void
gameset_add_game (const char *about,
                  const char *author, const char *byline,
                  const char *description, const char *genre,
                  const char *headline, const char *length,
                  const char *publisher, const char *release_date,
                  const char *title, const char *version)
{
  gameref_t game;
  assert (about && title);

  game = memory_malloc (sizeof (*game));
  game->magic = GAMESET_MAGIC;
  game->about = memory_strdup (about);
  game->author = memory_strdup (author);
  game->byline = memory_strdup (byline);
  game->description = memory_strdup (description);
  game->genre = memory_strdup (genre);
  game->headline = memory_strdup (headline);
  game->length = memory_strdup (length);
  game->publisher = memory_strdup (publisher);
  game->release_date = memory_strdup (release_date);
  game->title = memory_strdup (title);
  game->version = memory_strdup (version);

  if (!games_vector)
    games_vector = vector_create (sizeof (game));

  game->digest = hash (about);
  game->id = vector_get_length (games_vector);
  vector_append (games_vector, &game);

  game->next = games_head;
  games_head = game;
}

int
gameset_is_game (const gameref_t game)
{
  assert (game);

  return game->magic == GAMESET_MAGIC;
}

void
gameset_get_game (const gameref_t game,
                  const char **about,
                  const char **author, const char **byline,
                  const char **description, const char **genre,
                  const char **headline, const char **length,
                  const char **publisher, const char **release_date,
                  const char **title, const char **version)
{
  assert (gameset_is_game (game));

  if (about)
    *about = game->about;
  if (author)
    *author = game->author;
  if (byline)
    *byline = game->byline;
  if (description)
    *description = game->description;
  if (genre)
    *genre = game->genre;
  if (headline)
    *headline = game->headline;
  if (length)
    *length = game->length;
  if (publisher)
    *publisher = game->publisher;
  if (release_date)
    *release_date = game->release_date;
  if (title)
    *title = game->title;
  if (version)
    *version = game->version;
}

gameref_t
gameset_iterate (const gameref_t game)
{
  assert (!game || gameset_is_game (game));

  return game ? game->next : games_head;
}

int
gameset_is_empty (void)
{
  return games_head == NULL;
}

void
gameset_erase (void)
{
  gameref_t game, next;

  for (game = games_head; game; game = next)
    {
      next = game->next;

      memory_free (game->about);
      memory_free (game->author);
      memory_free (game->byline);
      memory_free (game->description);
      memory_free (game->genre);
      memory_free (game->headline);
      memory_free (game->length);
      memory_free (game->publisher);
      memory_free (game->release_date);
      memory_free (game->title);
      memory_free (game->version);

      memset (game, 0, sizeof (*game));
      memory_free (game);
    }

  games_head = NULL;

  if (games_vector)
    {
      vector_destroy (games_vector);
      games_vector = NULL;
    }
}


/*
 * gameset_find_game()
 *
 * Return the game for a given location, NULL if not found.
 */
gameref_t
gameset_find_game (const char *location)
{
  glui32 digest;
  gameref_t game;

  digest = hash (location);
  for (game = games_head; game; game = game->next)
    {
      if (game->digest == digest && strcmp (game->about, location) == 0)
        break;
    }

  return game;
}


/*
 * gameset_get_game_location()
 * gameset_get_game_title()
 * gameset_get_game_author()
 * gameset_get_game_publisher()
 * gameset_get_game_genre()
 *
 * Convenience functions to return just the about and assorted other fields
 * of a game.  This about field is the "key" data field, indicating the
 * whereabouts of the game data.
 */
const char *
gameset_get_game_location (const gameref_t game)
{
  assert (gameset_is_game (game));

  return game->about;
}

const char *
gameset_get_game_title (const gameref_t game)
{
  assert (gameset_is_game (game));

  return game->title;
}

const char *
gameset_get_game_author (const gameref_t game)
{
  assert (gameset_is_game (game));

  return game->author;
}

const char *
gameset_get_game_publisher (const gameref_t game)
{
  assert (gameset_is_game (game));

  return game->publisher;
}

const char *
gameset_get_game_genre (const gameref_t game)
{
  assert (gameset_is_game (game));

  return game->genre;
}


/*
 * gameset_get_game_id()
 * gameset_id_to_game()
 *
 * Translations between game ids and games.  Used as a way to indicate games
 * in a flat 32bit unsigned space.
 */
glui32
gameset_get_game_id (const gameref_t game)
{
  assert (gameset_is_game (game));

  return game->id;
}

gameref_t
gameset_id_to_game (glui32 id)
{
  if (id < (glui32) vector_get_length (games_vector))
    {
      gameref_t game;

      vector_get (games_vector, id, &game);
      assert (game->id == id);

      return game;
    }

  return NULL;
}
