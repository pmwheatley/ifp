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
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Malloc descriptor structure.  Each descriptor is placed on a list, and
 * gives the malloc'ed address it tracks, or NULL if on the free list.
 */
struct ifp_malloc_info
{
  void *address;
  struct ifp_malloc_info *next;
};
typedef struct ifp_malloc_info *ifp_malloc_inforef_t;

/*
 * Malloc data structures.  This is a variable sized array of list heads
 * (hash buckets), a note of its size, a pre-created pool of descriptors
 * sized at list size times fill factor elements, and a free list that
 * gathers up all unused pool entries.
 */
static int ifp_malloc_buckets = 0;
static ifp_malloc_inforef_t *ifp_malloc_lists = NULL,
                            ifp_malloc_pool = NULL,
                            ifp_malloc_freelist = NULL;

/*
 * List of prime numbers for hash buckets.  Rehash when table occupancy is
 * greater than or equal to the fill factor multiplied by the current sizing.
 * Using a prime number improves hash table performance.  The table ends
 * with a zero sentinel.
 */
static const int BUCKET_SIZES[] = {
  257, 521, 1049, 2099, 4201, 8419, 16843, 33703, 67409, 134837, 269683,
  539389, 1078787, 2157587, 4315183, 8630387, 17260781, 34521589, 69043189,
  138086407, 276172823, 552345671, 1104691373, 0
};

/* Hash table fill factor; when exceeded, it's time to rehash. */
static const int HASH_FILL_FACTOR = 2;

/*
 * The hashing algorithm is Knuth's multiplicative hash.  It uses the
 * "golden ratio", (sqrt(5)-1)/2, as the multiplier for the address, with
 * unsigned integer overflow, to achieve its distribution.  The following
 * constant approximates the "golden ratio" multiplied by MAX_UINT, giving
 * the multiplier for hashing heap addresses.  The same multiplication,
 * determined with 32-bit addresses in mind, also works tolerably well with
 * 64-bit addresses.
 */
static const glui32 HASH_MULTIPLIER = 2654435761u;


/*
 * ifp_memory_malloc_hash()
 *
 * Hash an address of heap data, returned from malloc, to a hash bucket
 * index number.  Hash is the pointer cast to an unsigned integer, multiplied
 * by the "golden ratio", then modulo'ed to the number of hash buckets.
 */
static int
ifp_memory_malloc_hash (const void *pointer)
{
  glui32 hash;

  if (ifp_malloc_buckets == 0)
    ifp_fatal ("memory: attempt to hash where no table exists");

  hash = (glui32) pointer * HASH_MULTIPLIER;
  return (int) (hash % ifp_malloc_buckets);
}


/*
 * ifp_memory_malloc_next_size()
 *
 * Return the next bucket count for the malloc hash tables.  If at the
 * largest size available from the list of prime bucket sizes, extend the
 * bucket count by fill factor plus one.
 */
static int
ifp_memory_malloc_next_size (int current_buckets)
{
  int index_, new_buckets;

  new_buckets = 0;
  for (index_ = 0; BUCKET_SIZES[index_] > 0; index_++)
    {
      if (BUCKET_SIZES[index_] > current_buckets)
        {
          new_buckets = BUCKET_SIZES[index_];
          break;
        }
    }

  /*
   * In the unlikely event that we hit the list end, double the current number
   * of buckets and add one to make the new number odd.
   */
  if (new_buckets == 0)
    {
      assert (current_buckets > 0);
      new_buckets = current_buckets * HASH_FILL_FACTOR + 1;
    }

  return new_buckets;
}


/*
 * ifp_memory_malloc_rehash()
 *
 * Extend the malloc hash tables to the next bucket count increment, and
 * rehash all contained malloc addresses.  On first call, create an empty
 * hash table of the smallest size.
 */
static void
ifp_memory_malloc_rehash (void)
{
  int old_pool_size, pool_size, hash, bytes;
  ifp_malloc_inforef_t entry;

  /* Resize and reallocate the pool of malloc info entries. */
  old_pool_size = ifp_malloc_buckets * HASH_FILL_FACTOR;

  ifp_malloc_buckets = ifp_memory_malloc_next_size (ifp_malloc_buckets);
  pool_size = ifp_malloc_buckets * HASH_FILL_FACTOR;

  ifp_malloc_pool = ifp_realloc (ifp_malloc_pool,
                                 sizeof (*ifp_malloc_pool) * pool_size);

  /*
   * Create a clean set of hash buckets.  The old hash buckets are invalidated
   * by the pool realloc above.  Start a new free list.
   */
  bytes = sizeof (*ifp_malloc_lists) * ifp_malloc_buckets;
  ifp_malloc_lists = ifp_realloc (ifp_malloc_lists, bytes);
  memset (ifp_malloc_lists, 0, bytes);

  ifp_malloc_freelist = NULL;

  /*
   * Rehash all occupied entries in the old hash pool into the new hash
   * buckets, and place any unoccupied entries from the old allocation into
   * the free list.  Only entries from zero to the old pool size considered;
   * ones beyond this are part of the new extension, dealt with later.
   */
  for (entry = ifp_malloc_pool;
       entry < ifp_malloc_pool + old_pool_size; entry++)
    {
      /*
       * If the entry is occupied, rehash and place it on the relevant list.
       * If unoccupied, indicated by a NULL in its address field, place it
       * on the free list.
       */
      if (entry->address)
        {
          hash = ifp_memory_malloc_hash (entry->address);
          entry->next = ifp_malloc_lists[hash];
          ifp_malloc_lists[hash] = entry;
        }
      else
        {
          entry->next = ifp_malloc_freelist;
          ifp_malloc_freelist = entry;
        }
    }

  /*
   * Add all the new pool entries from the extension area to the free list.
   * New entries are all those from the old pool size onwards.
   */
  for (entry = ifp_malloc_pool + old_pool_size;
       entry < ifp_malloc_pool + pool_size; entry++)
    {
      entry->address = NULL;
      entry->next = ifp_malloc_freelist;
      ifp_malloc_freelist = entry;
    }

  ifp_trace ("memory: rehashed table to %d entries", ifp_malloc_buckets);
}


/*
 * ifp_memory_malloc_add_address()
 * ifp_memory_malloc_remove_address()
 *
 * Add and remove addresses to/from the main malloc address list.  On
 * addition, the address must not be present, and on removal, it must be
 * present precisely once.
 */
static void
ifp_memory_malloc_add_address (void *pointer)
{
  int hash;
  ifp_malloc_inforef_t entry;

  if (!ifp_malloc_freelist)
    {
      ifp_trace ("memory: empty free list prompted rehash");
      ifp_memory_malloc_rehash ();
    }

  hash = ifp_memory_malloc_hash (pointer);

  for (entry = ifp_malloc_lists[hash]; entry; entry = entry->next)
    {
      if (entry->address == pointer)
        ifp_fatal ("memory: address %p already listed as malloced", pointer);
    }

  assert (ifp_malloc_freelist);
  entry = ifp_malloc_freelist;
  ifp_malloc_freelist = entry->next;

  entry->address = pointer;
  entry->next = ifp_malloc_lists[hash];
  ifp_malloc_lists[hash] = entry;
}

static void
ifp_memory_malloc_remove_address (const void *pointer)
{
  int hash, found;
  ifp_malloc_inforef_t entry, prior, next;

  if (!ifp_malloc_lists)
    {
      ifp_error ("memory: no lists to remove address %p from", pointer);
      return;
    }

  hash = ifp_memory_malloc_hash (pointer);

  found = FALSE;
  prior = NULL;
  for (entry = ifp_malloc_lists[hash]; entry; entry = next)
    {
      next = entry->next;

      if (entry->address == pointer)
        {
          if (found)
            ifp_fatal ("memory: address %p listed multiple times", pointer);
          else
            found = TRUE;

          if (!prior)
            {
              assert (entry == ifp_malloc_lists[hash]);
              ifp_malloc_lists[hash] = next;
            }
          else
            prior->next = next;

          entry->address = NULL;
          entry->next = ifp_malloc_freelist;
          ifp_malloc_freelist = entry;
        }
      else
        prior = entry;
    }

  if (!found)
    ifp_error ("memory: address %p not listed as malloced", pointer);
}


/*
 * ifp_libc_intercept_malloc()
 * ifp_libc_intercept_calloc()
 *
 * Intercept function for calls to malloc/calloc.  These call the real
 * libc functions, and keep each address returned on a list.  Addresses
 * free'd are removed from the list.  On finalization, addresses that
 * remain on the list are garbage-collected.
 */
void *
ifp_libc_intercept_malloc (size_t size)
{
  void *pointer;

  pointer = malloc (size);
  if (pointer)
    ifp_memory_malloc_add_address (pointer);

  return pointer;
}

void *
ifp_libc_intercept_calloc (size_t nmemb, size_t size)
{
  void *pointer;

  pointer = calloc (nmemb, size);
  if (pointer)
    ifp_memory_malloc_add_address (pointer);

  return pointer;
}


/*
 * ifp_libc_intercept_realloc()
 *
 * Interception for realloc() calls.  Updated the malloc addresses list
 * according to the result of the real realloc.
 */
void *
ifp_libc_intercept_realloc (void *ptr, size_t size)
{
  void *pointer;

  pointer = realloc (ptr, size);
  if (pointer != ptr)
    {
      if (ptr)
        ifp_memory_malloc_remove_address (ptr);
      if (pointer)
        ifp_memory_malloc_add_address (pointer);
    }

  return pointer;
}


/*
 * ifp_libc_intercept_strdup()
 *
 * Interception for strdup() calls.  Because strdup() malloc's its return
 * address, it must be tracked as malloc().
 */
char *
ifp_libc_intercept_strdup (const char *s)
{
  void *pointer;

  pointer = strdup (s);
  if (pointer)
    ifp_memory_malloc_add_address (pointer);

  return pointer;
}


/*
 * ifp_libc_intercept_getcwd()
 *
 * Interception for getcwd() calls.  Getcwd can, depending on usage, malloc
 * the buffer it returns.
 */
char *
ifp_libc_intercept_getcwd (char *buf, size_t size)
{
  char *buffer;

  buffer = getcwd (buf, size);

  /*
   * If buf is NULL, this was a request for a malloc'ed return.  If buffer
   * is non-NULL, it was a successful call.
   */
  if (!buf && buffer)
    ifp_memory_malloc_add_address (buffer);

  return buffer;
}


/*
 * ifp_libc_intercept_scandir()
 *
 * Interception for scandir() calls.
 */
int
ifp_libc_intercept_scandir (const char *dir, struct dirent ***namelist,
                            int (*select_) (const struct dirent *),
                            int (*compar_) (const void *, const void *))
{
  int count;

  count = scandir (dir, namelist, select_, compar_);

  /*
   * If scandir() returned a non-error status, add each entry in namelist
   * to the lists, then add namelist itself.
   */
  if (count >= 0)
    {
      struct dirent **entries = *namelist;
      int index_;

      for (index_ = 0; index_ < count; index_++)
        {
          if (entries[index_])
            ifp_memory_malloc_add_address (entries[index_]);
        }

      if (entries)
        ifp_memory_malloc_add_address (entries);
    }

  return count;
}


/*
 * ifp_libc_intercept_free()
 *
 * Intercept free(), and delete the free'd address from the list.
 */
void
ifp_libc_intercept_free (void *ptr)
{
  if (ptr)
    ifp_memory_malloc_remove_address (ptr);

  free (ptr);
}


/**
 * ifp_memory_malloc_garbage_collect()
 *
 * Garbage-collect any memory addresses remaining on our lists after a plugin
 * completes running.
 */
void
ifp_memory_malloc_garbage_collect (void)
{
  int hash, count;
  ifp_malloc_inforef_t entry;

  ifp_trace ("memory: ifp_memory_malloc_garbage_collect <- void");

  count = 0;
  for (hash = 0; hash < ifp_malloc_buckets; hash++)
    {
      for (entry = ifp_malloc_lists[hash]; entry; entry = entry->next)
        {
          free (entry->address);
          count++;
        }
    }

  if (count > 0)
    ifp_trace ("memory: recycled %d allocation(s)", count);

  /*
   * Free the hash buckets and pool, and reset all malloc tracking data
   * structures back to their initial values.
   */
  ifp_free (ifp_malloc_lists);
  ifp_malloc_lists = NULL;
  ifp_malloc_buckets = 0;

  ifp_free (ifp_malloc_pool);
  ifp_malloc_pool = NULL;
  ifp_malloc_freelist = NULL;
}
