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
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#include "ifp.h"
#include "ifp_internal.h"


/*
 * ifp_dlopen()
 * ifp_dlsym()
 * ifp_dlerror()
 * ifp_dlclose()
 *
 * Wrappers for shared object loading functions.  These functions manage
 * loading shared objects, name lookups, shared object error reporting, and
 * closing shared objects.
 */
void *
ifp_dlopen (const char *filename)
{
  void *handle;

  ifp_trace ("util: ifp_dlopen <- '%s'", filename);

  handle = dlopen (filename, RTLD_NOW);
  ifp_trace ("util: ifp_dlopen returned"
             " handle_%p", ifp_trace_pointer (handle));
  return handle;
}

const char *
ifp_dlerror (void)
{
  return dlerror ();
}

void *
ifp_dlsym (void *handle, const char *symbol)
{
  ifp_trace ("util: ifp_dlsym <-"
             " handle_%p '%s'", ifp_trace_pointer (handle), symbol);
  return dlsym (handle, symbol);
}

int
ifp_dlclose (void *handle)
{
  ifp_trace ("util: ifp_dlclose <- handle_%p", ifp_trace_pointer (handle));
  return dlclose (handle);
}


/*
 * ifp_malloc()
 * ifp_realloc()
 * ifp_free()
 *
 * Wrappers for standard libc malloc(), realloc(), and free() functions.
 * The malloc() and realloc() wrappers ensure that allocation does not fail,
 * and will call abort() for out of memory conditions.
 */
void *
ifp_malloc (size_t size)
{
  void *pointer;

  pointer = malloc (size);
  if (!pointer)
    ifp_fatal ("util: malloc failed, allocating %zd bytes", size);

  ifp_trace ("util: ifp_malloc allocated %zd bytes at void_%p", size, pointer);
  return pointer;
}

void *
ifp_realloc (void *ptr, size_t size)
{
  void *pointer;

  pointer = realloc (ptr, size);
  if (!pointer)
    ifp_fatal ("util: realloc failed, allocating %zd bytes", size);

  if (pointer && pointer != ptr)
    ifp_trace ("util:"
               " ifp_realloc allocated %zd bytes at void_%p", size, pointer);

  return pointer;
}

void
ifp_free (void *pointer)
{
  free (pointer);
  if (pointer)
    ifp_trace ("util: ifp_free freed void_%p", pointer);
}


/*
 * ifp_split_string()
 * ifp_free_split_string()
 *
 * Split a string on a separator character and return an array of allocated
 * strings, and a count of elements.  Modeled on the return structure of
 * scandir(), with a matching destructor.
 */
int
ifp_split_string (const char *string, char separator, char ***elements)
{
  int count, index_;
  char **split, separator_string[2];
  const char *element;

  /* Handle the awkward case of empty string; return a 0-element allocation. */
  if (string[0] == '\0')
    {
      *elements = ifp_malloc (0);
      return 0;
    }

  /* Count the number of elements present in the string. */
  count = 1;
  for (index_ = 0; string[index_] != '\0'; index_++)
    {
      if (string[index_] == separator)
        count++;
    }

  /* Allocate, and set up a separator string for strcspn(). */
  split = ifp_malloc (count * sizeof (*split));
  separator_string[0] = separator;
  separator_string[1] = '\0';

  /* Find individual elements, and allocate into the split array. */
  index_ = 0;
  element = string;
  while (element[0] != '\0')
    {
      int length;

      length = strcspn (element, separator_string);
      split[index_] = ifp_malloc (length + 1);
      strncpy (split[index_], element, length);
      split[index_][length] = '\0';

      element += length + (element[length] == separator ? 1 : 0);
      index_++;
    }

  /* If string ended with a separator, add one final empty element. */
  if (index_ < count)
    {
      split[index_] = ifp_malloc (2);
      strcpy (split[index_++], "");
    }

  *elements = split;
  return count;
}

void
ifp_free_split_string (char **elements, int count)
{
  int index_;

  for (index_ = 0; index_ < count; index_++)
    ifp_free (elements[index_]);

  ifp_free (elements);
}


/**
 * glk_c_*()
 *
 * Const-correct glk wrapper functions.  Several of Glk's functions take
 * pointer arguments that are not declared const but that could be.  These
 * can require ugly casting if using high levels of compiler warning.  To
 * help with these, IFP offers wrappers that have const pointer parameters.
 */
strid_t
glk_c_stream_open_memory (const char *buf,
                          glui32 buflen, glui32 fmode, glui32 rock)
{
  return glk_stream_open_memory ((char *) buf, buflen, fmode, rock);
}

void
glk_c_put_string (const char *s)
{
  glk_put_string ((char *) s);
}

void
glk_c_put_string_stream (strid_t str, const char *s)
{
  glk_put_string_stream (str, (char *) s);
}

void
glk_c_put_buffer (const char *buf, glui32 len)
{
  glk_put_buffer ((char *) buf, len);
}

void
glk_c_put_buffer_stream (strid_t str, const char *buf, glui32 len)
{
  glk_put_buffer_stream (str, (char *) buf, len);
}

strid_t
glk_c_stream_open_memory_uni (const glui32 *buf,
                              glui32 buflen, glui32 fmode, glui32 rock)
{
  return glk_stream_open_memory_uni ((glui32 *) buf, buflen, fmode, rock);
}

void
glk_c_put_string_uni (const glui32 *s)
{
  glk_put_string_uni ((glui32 *) s);
}

void
glk_c_put_buffer_uni (const glui32 *buf, glui32 len)
{
  glk_put_buffer_uni ((glui32 *) buf, len);
}

void
glk_c_put_string_stream_uni (strid_t str, const glui32 *s)
{
  glk_put_string_stream_uni (str, (glui32 *) s);
}

void
glk_c_put_buffer_stream_uni (strid_t str, const glui32 *buf, glui32 len)
{
  glk_put_buffer_stream_uni (str, (glui32 *) buf, len);
}
