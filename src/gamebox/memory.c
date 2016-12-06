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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "protos.h"


/*
 * memory_malloc()
 * memory_realloc()
 * memory_free()
 *
 * Convenience wrappers around malloc/realloc/free.
 */
void *
memory_malloc (size_t size)
{
  void *pointer;

  pointer = malloc (size);
  if (!pointer)
    {
      fprintf (stderr, "GLK INTERNAL ERROR:"
               " malloc failed, allocating %u bytes.\n", size);
      abort ();
    }

  return pointer;
}

void *
memory_realloc (void *ptr, size_t size)
{
  void *pointer;

  pointer = realloc (ptr, size);
  if (!pointer)
    {
      fprintf (stderr, "GLK INTERNAL ERROR:"
               " realloc failed, allocating %u bytes.\n", size);
      abort ();
    }

  return pointer;
}

void
memory_free (void *ptr)
{
  free (ptr);
}


/*
 * memory_strdup()
 *
 * Convenience function to duplicate any non-NULL strings.  Returns NULL
 * if the input string is NULL.
 */
char *
memory_strdup (const char *string)
{
  char *duplicate = NULL;

  if (string)
    {
      duplicate = memory_malloc (strlen (string) + 1);
      strcpy (duplicate, string);
    }
  return duplicate;
}
