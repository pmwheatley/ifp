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


/*
 * Id offset used by groups.  This lets games and groups appear to clients
 * as if they have different id ranges, allowing them to be represented
 * uniformly as glui32's.
 */
static const glui32 GAMEGROUP_ID_OFFSET = 0x80000000;


/* A group tree node can contain either a game or a sub-group. */
union node_u {
  gameref_t game;
  groupref_t group;
};
typedef union node_u node_t;

/* Group magic identifier, and descriptor structure definition. */
static const unsigned int GAMEGROUP_MAGIC = 0x6db51af0;
struct group_s {
  unsigned int magic;

  char *about;           /* IFMES:    RDF:about */
                         /* iFiction: annotation, gamebox, group */
  char *title;           /* IFMES:    DC:title */
                         /* iFiction: annotation, gamebox, group */
  char *description;     /* IFMES:    DC:description */
                         /* iFiction: (not available, unused) */

  vectorref_t contents;  /* Contained games/groups, growable, ordered */

  glui32 digest;
  glui32 id;
  groupref_t next;
};

/* List of group descriptors, unordered. */
static groupref_t groups_head = NULL;

/* Vector of groups in existence, for fast lookup by group id. */
static vectorref_t groups_vector = NULL;

/*
 * Designated groups root node, either the top level grouping, or something
 * constructed out of top level tree fragments.
 */
static groupref_t group_root = NULL;


/*
 * gamegroup_add_group()
 * gamegroup_is_group()
 * gamegroup_get_group()
 * gamegroup_iterate()
 * gamegroup_is_empty()
 * gamegroup_erase()
 *
 * Create, iterate through, check, and erase a growable array of game groups.
 * All fields except 'about' and 'title' may be NULL.
 */
groupref_t
gamegroup_add_group (const char *about,
                     const char *title, const char *description)
{
  groupref_t group;
  assert (about && title);

  /* Create and add the new entry. */
  group = memory_malloc (sizeof (*group));
  group->magic = GAMEGROUP_MAGIC;
  group->about = memory_strdup (about);
  group->title = memory_strdup (title);
  group->description = memory_strdup (description);

  group->contents = vector_create (sizeof (node_t));

  if (!groups_vector)
    groups_vector = vector_create (sizeof (group));

  group->digest = hash (about);
  group->id = vector_get_length (groups_vector);
  vector_append (groups_vector, &group);

  group->next = groups_head;
  groups_head = group;

  return group;
}

int
gamegroup_is_group (const groupref_t group)
{
  assert (group);

  return group->magic == GAMEGROUP_MAGIC;
}

void
gamegroup_get_group (const groupref_t group,
                     const char **about,
                     const char **title, const char **description)
{
  assert (gamegroup_is_group (group));

  if (about)
    *about = group->about;
  if (title)
    *title = group->title;
  if (description)
    *description = group->description;
}

groupref_t
gamegroup_iterate (const groupref_t group)
{
  assert (!group || gamegroup_is_group (group));

  return group ? group->next : groups_head;
}

int
gamegroup_is_empty (void)
{
  return groups_head == NULL;
}

void
gamegroup_erase (void)
{
  groupref_t group, next;

  for (group = groups_head; group; group = next)
    {
      next = group->next;

      memory_free (group->about);
      memory_free (group->title);
      memory_free (group->description);

      memory_free (group->contents);

      memset (group, 0, sizeof (*group));
      memory_free (group);
    }

  groups_head = NULL;

  if (groups_vector)
    {
      vector_destroy (groups_vector);
      groups_vector = NULL;
    }
}


/*
 * gamegroup_find_group()
 *
 * Return the group for a given name, NULL if not found.
 */
groupref_t
gamegroup_find_group (const char *name)
{
  glui32 digest;
  groupref_t group;

  digest = hash (name);
  for (group = groups_head; group; group = group->next)
    {
      if (group->digest == digest && strcmp (group->about, name) == 0)
        break;
    }

  return group;
}


/*
 * gamegroup_get_group_title()
 *
 * Convenience functions to return just the title field of a group.
 */
const char *
gamegroup_get_group_title (const groupref_t group)
{
  assert (gamegroup_is_group (group));

  return group->title;
}


/*
 * gamegroup_add_game_to_group()
 * gamegroup_add_group_to_group()
 *
 * Add a game or a (sub-)group to a given game group.  The item to add is
 * found by lookup on location (for game) or about (for group).  Returns
 * true if added successfully, false otherwise.
 */
int
gamegroup_add_game_to_group (groupref_t group, const char *location)
{
  gameref_t game;
  assert (gamegroup_is_group (group));

  game = gameset_find_game (location);
  if (game)
    {
      node_t node;

      node.game = game;
      vector_append (group->contents, &node);
    }

  return game != NULL;
}

int
gamegroup_add_group_to_group (groupref_t group, const char *about)
{
  groupref_t other;
  assert (gamegroup_is_group (group));

  for (other = gamegroup_iterate (NULL); other;
       other = gamegroup_iterate (other))
    {
      if (strcmp (other->about, about) == 0)
        break;
    }

  if (other)
    {
      node_t node;

      node.group = other;
      vector_append (group->contents, &node);
    }

  return other != NULL;
}


/*
 * gamegroup_get_node_game()
 * gamegroup_get_node_group()
 * gamegroup_is_node_game()
 * gamegroup_is_node_group()
 *
 * Retrieve the game or group from a tree node.  Returns NULL if the node
 * is not of the correct type.  And check node type.
 */
gameref_t
gamegroup_get_node_game (const noderef_t node)
{
  return gameset_is_game (node->game) ? node->game : NULL;
}

groupref_t
gamegroup_get_node_group (const noderef_t node)
{
  return gamegroup_is_group (node->group) ? node->group : NULL;
}

int
gamegroup_is_node_game (const noderef_t node)
{
  return gameset_is_game (node->game);
}

int
gamegroup_is_node_group (const noderef_t node)
{
  return gamegroup_is_group (node->group);
}


/*
 * gamegroup_get_root()
 *
 * Get the root game group.  If none is yet designated, then search the
 * known groups and either decide on one or construct one and return that.
 */
groupref_t
gamegroup_get_root (void)
{
  vectorref_t nodes;
  int nodes_count;
  gameref_t game;
  groupref_t group;

  /*
   * Return any cached root node value we hold, and don't bother trying to
   * find a new one if none cached but no games or groups available.
   */
  if (group_root)
    return group_root;

  else if (gameset_is_empty () && gamegroup_is_empty ())
    return NULL;

  /* First, list every game and group known. */
  nodes = vector_create (sizeof (node_t));

  for (game = gameset_iterate (NULL); game; game = gameset_iterate (game))
    {
      node_t node;

      node.game = game;
      vector_append (nodes, &node);
    }

  for (group = gamegroup_iterate (NULL);
       group; group = gamegroup_iterate (group))
    {
      node_t node;

      node.group = group;
      vector_append (nodes, &node);
    }

  nodes_count = vector_get_length (nodes);

  /* Now remove all entries that are themselves members of a group. */
  for (group = gamegroup_iterate (NULL);
       group; group = gamegroup_iterate (group))
    {
      noderef_t node;

      for (node = gamegroup_iterate_group (group, NULL);
           node; node = gamegroup_iterate_group (group, node))
        {
          int index_;

          index_ = 0;
          while (index_ < nodes_count)
            {
              node_t other;

              vector_get (nodes, index_, &other);

              if (memcmp (node, &other, sizeof (other)) == 0)
                {
                  vector_delete (nodes, index_);
                  nodes_count--;
                }
              else
                index_++;
            }
        }
    }

  /* If the nodes count is one and that node is a group, return it. */
  if (nodes_count == 1)
    {
      node_t node;

      vector_get (nodes, 0, &node);
      group_root = gamegroup_is_group (node.group) ? node.group : NULL;
    }

  /*
   * The nodes count may be zero, indicating a loop in the groups, or more
   * than one.  For the first case, choosing a group arbitrarily may leave
   * some loop parts unreachable.  So for both cases, then, manufacture a
   * top level group to contain either all groups (if length is zero) or all
   * island nodes (where length is greater than one).
   */
  if (!group_root)
    {
      int index_;

      /* Re-add groups if everything looks like it's contained. */
      if (nodes_count == 0)
        {
          for (group = gamegroup_iterate (NULL);
               group; group = gamegroup_iterate (group))
            {
              node_t node;

              node.group = group;
              vector_set (nodes, nodes_count++, &node);
            }
        }

      /* Manufacture a root node and dump everything into it. */
      group_root = gamegroup_add_group ("[Root]", "Main", NULL);

      for (index_ = 0; index_ < nodes_count; index_++)
        {
          node_t node;

          vector_get (nodes, index_, &node);

          if (gamegroup_is_node_game (&node))
            {
              gameref_t entry;

              entry = gamegroup_get_node_game (&node);
              gamegroup_add_game_to_group (group_root,
                                           gameset_get_game_location (entry));
            }
          else if (gamegroup_is_node_group (&node))
            {
              groupref_t entry;

              entry = gamegroup_get_node_group (&node);
              gamegroup_add_group_to_group (group_root, entry->about);
            }
        }
    }

  vector_destroy (nodes);

  return group_root;
}


/*
 * gamegroup_iterate_group()
 * gamegroup_group_is_empty()
 *
 * Iterate games/groups in a group, and check a group for emptiness.
 */
noderef_t
gamegroup_iterate_group (const groupref_t group, const noderef_t node)
{
  noderef_t contents;
  int length;
  assert (gamegroup_is_group (group));

  contents = (noderef_t) vector_get_address (group->contents, 0);
  length = vector_get_length (group->contents);
  assert (!node || (node >= contents && node <= contents + (length - 1)));

  if (node)
    {
      if (node < contents + (length - 1))
        return node + 1;
      else
        return NULL;
    }
  else
    return contents;
}

int
gamegroup_group_is_empty (const groupref_t group)
{
  assert (gamegroup_is_group (group));

  return vector_get_length (group->contents) == 0;
}


/*
 * gamegroup_get_group_id()
 * gamegroup_id_to_group()
 *
 * Translations between group ids and groups.  Used as a way to indicate groups
 * in a flat 32bit unsigned space.
 */
glui32
gamegroup_get_group_id (const groupref_t group)
{
  assert (gamegroup_is_group (group));

  return group->id + GAMEGROUP_ID_OFFSET;
}

groupref_t
gamegroup_id_to_group (glui32 id)
{
  id -= GAMEGROUP_ID_OFFSET;

  if (id < (glui32) vector_get_length (groups_vector))
    {
      groupref_t group;

      vector_get (groups_vector, id, &group);
      assert (group->id == id);

      return group;
    }

  return NULL;
}
