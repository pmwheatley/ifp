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

#include "protos.h"


/* Vector magic identifier, and descriptor structure definition. */
const unsigned int VECTOR_MAGIC = 0x5e18fd28;
struct vector_s {
  unsigned int magic;

  int span;        /* Size of each contained item, in bytes */
  int allocation;  /* Number of elements allocated for in data */
  int length;      /* Count of elements actually used */
  void *data;      /* Allocated growable data space */
};


/*
 * vector_is_vector()
 * vector_is_valid_index()
 * vector_get_data()
 * vector_extend()
 * vector_expunge()
 *
 * Internal implementation of a simple growable array.  Check vector, check
 * index is valid, return data address for an index, and grow and shrink
 * the data array.
 */
static int
vector_is_vector (const vectorref_t vector)
{
  assert (vector);

  return vector->magic == VECTOR_MAGIC;
}

static int
vector_is_valid_index (const vectorref_t vector, int index_)
{
  assert (vector);

  return index_ >= 0 && index_ < vector->length;
}

static void *
vector_get_data (const vectorref_t vector, int index_)
{
  assert (vector_is_vector (vector));
  assert (vector_is_valid_index (vector, index_));

  return (unsigned char*) vector->data + (index_ * vector->span);
}

static void
vector_extend (vectorref_t vector, int elements)
{
  int new_allocation;
  assert (vector_is_vector (vector));
  assert (elements > 0);

  if (elements <= vector->length)
    return;

  for (new_allocation = vector->allocation; new_allocation < elements; )
    new_allocation = new_allocation == 0 ? 1 : new_allocation << 1;

  if (new_allocation > vector->allocation)
    {
      vector->allocation = new_allocation;
      vector->data = memory_realloc (vector->data,
                                     new_allocation * vector->span);
    }

  vector->length = elements;
}

static void
vector_expunge (vectorref_t vector, int index_)
{
  int new_length;
  assert (vector_is_vector (vector));
  assert (vector_is_valid_index (vector, index_));

  new_length = vector->length - 1;

  if (index_ < new_length)
    {
      unsigned char *addr;

      addr = (unsigned char*) vector->data + (index_ * vector->span);
      memmove (addr, addr + vector->span, (new_length - index_) * vector->span);
    }

  vector->length = new_length;
}


/*
 * vector_create()
 * vector_destroy()
 * vector_get_address()
 * vector_get()
 * vector_set()
 * vector_delete()
 * vector_get_length()
 * vector_clear()
 *
 * Public vector interface.  Creates and destroys a vector, gets the data
 * address for an index, copies out or in elements, deletes an element, gets
 * the vector length (count of used elements), and clears the vector (resets
 * count of used elements to zero, retaining allocation).
 */
vectorref_t
vector_create (int span)
{
  vectorref_t vector;
  assert (span > 0);

  vector = memory_malloc (sizeof (*vector));
  vector->magic = VECTOR_MAGIC;
  vector->span = span;
  vector->allocation = vector->length = 0;
  vector->data = NULL;

  return vector;
}

void
vector_destroy (vectorref_t vector)
{
  assert (vector_is_vector (vector));

  memory_free (vector->data);

  memset (vector, 0, sizeof (*vector));
  memory_free (vector);
}

const void *
vector_get_address (const vectorref_t vector, int index_)
{
  return vector_get_data (vector, index_);
}

void
vector_get (const vectorref_t vector, int index_, void *element)
{
  memcpy (element, vector_get_data (vector, index_), vector->span);
}

void
vector_set (vectorref_t vector, int index_, const void *element)
{
  vector_extend (vector, index_ + 1);
  memcpy (vector_get_data (vector, index_), element, vector->span);
}

void
vector_delete (const vectorref_t vector, int index_)
{
  vector_expunge (vector, index_);
}

int
vector_get_length (const vectorref_t vector)
{
  assert (vector_is_vector (vector));

  return vector->length;
}

void
vector_clear (vectorref_t vector)
{
  assert (vector_is_vector (vector));

  vector->length = 0;
}


/*
 * vector_append()
 * vector_remove()
 * vector_unqueue()
 *
 * Public stack and queue interface.  Grow the vector by one element at the
 * end, remove (pop) the last element, and remove (unqueue) the first element.
 */
int
vector_append (vectorref_t vector, const void *element)
{
  assert (vector_is_vector (vector));

  vector_set (vector, vector->length, element);
  return vector->length - 1;
}

void
vector_remove (vectorref_t vector, void *element)
{
  assert (vector_is_vector (vector));
  assert (vector->length > 0);

  vector_get (vector, vector->length - 1, element);
  vector_expunge (vector, vector->length - 1);
}

void
vector_unqueue (vectorref_t vector, void *element)
{
  assert (vector_is_vector (vector));
  assert (vector->length > 0);

  vector_get (vector, 0, element);
  vector_expunge (vector, 0);
}
