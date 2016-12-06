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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Current set limit on the size the cache may grow to.  Some games are very
 * large, others much more modest.  The default cache size is 10Mb, which is
 * extremely comfortable for most small to medium text games, but which could
 * be totally dominated by one single game with images and sounds.  Still, as
 * an initial swipe, 10Mb seems ample.
 */
static int ifp_cache_size_limit = 10485760;

/*
 * Definition of a cache list entry structure, and list.  Cache entries
 * record the URL, the corresponding file containing the URL data, the
 * file size, reference and usage counts, and a last access timestamp.
 */
struct ifp_cache
{
  char *url_path;
  char *data_file;
  int file_size;
  int reference_count;
  int usage_count;
  int timestamp;

  struct ifp_cache *next;
};
typedef struct ifp_cache *ifp_cacheref_t;
static ifp_cacheref_t ifp_cache_list = NULL;


/**
 * ifp_cache_set_limit()
 * ifp_cache_get_limit()
 *
 * Set and get the limits on cache size.  Limits are in bytes, and indicate
 * a limit on the total size of cached files.  When the size exceeds this
 * limit, older unreferenced URL data will be deleted from the cache.
 */
void
ifp_cache_set_limit (int limit)
{
  if (limit < 0)
    {
      ifp_error ("cache: invalid cache limit, %d", limit);
      return;
    }

  ifp_trace ("cache: cache limit set to %d bytes", limit);
  ifp_cache_size_limit = limit;
}

int
ifp_cache_get_limit (void)
{
  static int env_cache_size_limit, initialized = FALSE;
  static const char *ifp_cache_limit;

  if (!initialized)
    {
      ifp_cache_limit = getenv ("IFP_CACHE_LIMIT");
      if (ifp_cache_limit)
        {
          env_cache_size_limit = atoi (ifp_cache_limit);
          ifp_notice ("cache: %s initialized cache size limit to %d bytes",
                      "IFP_CACHE_LIMIT", env_cache_size_limit);
        }
      initialized = TRUE;
    }

  return ifp_cache_limit ? env_cache_size_limit : ifp_cache_size_limit;
}


/**
 * ifp_cache_size()
 *
 * Return the total number of bytes of data currently held in the URL cache.
 */
int
ifp_cache_size (void)
{
  ifp_cacheref_t entry;
  int size;

  size = 0;
  for (entry = ifp_cache_list; entry; entry = entry->next)
    size += entry->file_size;

  ifp_trace ("cache: cache size summed as %d bytes", size);
  return size;
}


/*
 * ifp_cache_finalize_cleanup()
 *
 * Go through the list of cache entries we have, and delete the data file
 * referenced by each one.  This function is called on process shutdown, to
 * clean up temporary files.
 */
static void
ifp_cache_finalize_cleanup (void)
{
  ifp_cacheref_t entry;

  ifp_trace ("cache: ifp_cache_finalize_cleanup <- void");

  for (entry = ifp_cache_list; entry; entry = ifp_cache_list)
    {
      unlink (entry->data_file);
      ifp_trace ("cache: finalizer unlinked '%s'", entry->data_file);

      ifp_cache_list = entry->next;
      ifp_free (entry->url_path);
      ifp_free (entry->data_file);
      ifp_free (entry);
    }
}


/*
 * ifp_cache_lookup_url_path()
 *
 * Find any matching cache entry for a given URL path.  Return the cache
 * entry, or NULL if not found.
 */
static ifp_cacheref_t
ifp_cache_lookup_url_path (const char *url_path)
{
  ifp_cacheref_t entry;

  ifp_trace ("cache: ifp_cache_lookup_url_path <- '%s'", url_path);

  for (entry = ifp_cache_list; entry; entry = entry->next)
    {
      if (strcmp (url_path, entry->url_path) == 0)
        {
          ifp_trace ("cache: found entry for"
                     " cache_%p", ifp_trace_pointer (entry));
          return entry;
        }
    }

  return NULL;
}


/*
 * ifp_cache_timestamp()
 *
 * Return a timestamp suitable for inclusion in a cache entry.  Timestamps
 * are a count of seconds since the system epoch.  Gettimeofday returns
 * seconds and microseconds since 1/1/70 in ints, so will overflow in 2037.
 */
static int
ifp_cache_timestamp (void)
{
  struct timeval now;

  if (gettimeofday (&now, NULL) == -1)
    {
      ifp_error ("cache: error getting timestamp");
      return 0;
    }

  ifp_trace ("cache: ifp_cache_timestamp returned %ld", (long) now.tv_sec);
  return now.tv_sec;
}


/*
 * ifp_cache_find_entry()
 *
 * Find any matching cache entry for a given URL path.  If found, return the
 * path to the temporary file containing the downloaded data, increment
 * the cache entry's reference and usage counts, and update its last access
 * timestamp.  On cache miss, return NULL.
 */
const char *
ifp_cache_find_entry (const char *url_path)
{
  ifp_cacheref_t entry;
  assert (url_path);

  ifp_trace ("cache: ifp_cache_find_entry <- '%s'", url_path);

  entry = ifp_cache_lookup_url_path (url_path);
  if (!entry)
    {
      ifp_trace ("cache: cache miss");
      return NULL;
    }

  entry->reference_count++;
  entry->usage_count++;
  entry->timestamp = ifp_cache_timestamp ();

  ifp_trace ("cache: cache hit, referenced entry"
             " cache_%p", ifp_trace_pointer (entry));
  return entry->data_file;
}


/*
 * ifp_cache_release_entry()
 *
 * Decrement the reference count of any cache entry associated with the
 * given URL path.  Used when a URL that uses this entry is deleted.
 */
void
ifp_cache_release_entry (const char *url_path)
{
  ifp_cacheref_t entry;
  assert (url_path);

  ifp_trace ("cache: ifp_cache_release_entry <- '%s'", url_path);

  entry = ifp_cache_lookup_url_path (url_path);
  if (entry)
    {
      if (entry->reference_count > 0)
        {
          entry->reference_count--;
          ifp_trace ("cache: cache entry cache_%p released",
                     ifp_trace_pointer (entry));
        }
      else
        ifp_trace ("cache: cache entry cache_%p unreferenced",
                   ifp_trace_pointer (entry));
    }
}



/*
 * ifp_cache_remove_entry()
 *
 * Remove any cache entry that exists for the given URL path, and delete the
 * temporary file associated with that entry.
 */
void
ifp_cache_remove_entry (const char *url_path)
{
  ifp_cacheref_t entry, prior, next;
  assert (url_path);

  ifp_trace ("cache: ifp_cache_remove_entry <- '%s'", url_path);

  for (prior = NULL, entry = ifp_cache_list; entry; entry = next)
    {
      next = entry->next;

      if (strcmp (url_path, entry->url_path) == 0)
        {
          ifp_trace ("cache: removing entry"
                     " cache_%p", ifp_trace_pointer (entry));
          if (!prior)
            {
              assert (entry = ifp_cache_list);
              ifp_cache_list = next;
            }
          else
            prior->next = next;

          ifp_trace ("cache: deleting file '%s'", entry->data_file);
          unlink (entry->data_file);

          ifp_free (entry->url_path);
          ifp_free (entry->data_file);
          ifp_free (entry);
        }
      else
        prior = entry;
    }
}


/*
 * ifp_cache_weight()
 *
 * Calculate a weight for a cache entry.  The weight is an integer based on
 * the most recent cache entry access, and the number of times an entry has
 * been used (modified LRU algorithm).  Larger weights indicate entries less
 * suitable for scavenging.
 *
 * There are several refinements possible to this way of selecting cache
 * entries to delete, for example, one large one may be preferable to
 * multiple small but slightly older ones.
 */
static int
ifp_cache_weight (ifp_cacheref_t entry, int timestamp)
{
  int age_seconds, age_milliseconds, scaled_age, weight;

  ifp_trace ("cache: ifp_cache_weight <-"
             " cache_%p %d", ifp_trace_pointer (entry), timestamp);

  /* Calculate age in seconds, disallowing negative values. */
  age_seconds = (timestamp >= entry->timestamp)
                ? timestamp - entry->timestamp : 0;
  assert (age_seconds >= 0);

  /*
   * Convert that age to milliseconds, checking for overflow.  An entry older
   * than about 24 days will overflow age_millisecs.
   */
  age_milliseconds = (age_seconds < INT_MAX / 1000)
                     ? age_seconds * 1000 : INT_MAX;
  assert (age_milliseconds >= 0);

  /* Scale the age in milliseconds by the entry usage count. */
  assert (entry->usage_count > 0);
  scaled_age = age_milliseconds / entry->usage_count;

  /*
   * Lower scaled age implies higher weight, so calculate the return weighting
   * accordingly.
   */
  assert (scaled_age >= 0);
  weight = INT_MAX - scaled_age;

  ifp_trace ("cache: ifp_cache_weight returned %d", weight);
  return weight;
}


/*
 * ifp_cache_scavenge()
 *
 * Sum the amount of data in the cache.  If larger than the limit, scavenge
 * the lightest unreferenced cache entries found, until either the data drops
 * to or below the limit, or until there are no remaining unreferenced entries.
 */
static void
ifp_cache_scavenge (void)
{
  int timestamp;
  ifp_cacheref_t entry, target;

  ifp_trace ("cache: ifp_cache_scavenge <- void");

  timestamp = ifp_cache_timestamp ();

  /*
   * Until the cache size is reduced below the limit, search out and scavenge
   * the entry with the lowest weight for the current timestamp.
   */
  while (ifp_cache_size () > ifp_cache_get_limit ())
    {
      ifp_trace ("cache: cache size is [still] above limit");

      target = NULL;
      for (entry = ifp_cache_list; entry; entry = entry->next)
        {
          if (entry->reference_count == 0)
            {
              if (!target
                  || ifp_cache_weight (entry, timestamp)
                     < ifp_cache_weight (target, timestamp))
                target = entry;
            }
        }

      if (!target)
        {
          ifp_trace ("cache: no unreferenced entries remain");
          break;
        }

      ifp_trace ("cache: scavenging entry"
                 " cache_%p", ifp_trace_pointer (target));
      ifp_cache_remove_entry (target->url_path);
    }
}


/*
 * ifp_cache_add_entry()
 *
 * Add a new cache entry, given a URL path and temporary file path.  Once
 * added, the cache will handle temporary file removal, so the caller
 * should not subsequently remove the file.  The function returns TRUE
 * if the new entry was added successfully, otherwise FALSE.  The new
 * entry's reference count is 1.  It is an error to add a cache entry to
 * a URL path already cached.
 */
int
ifp_cache_add_entry (const char *url_path, const char *data_file)
{
  static int initialized = FALSE;
  ifp_cacheref_t entry;
  struct stat statbuf;
  assert (url_path && data_file);

  ifp_trace ("cache: ifp_cache_add_entry <- '%s' '%s'", url_path, data_file);

  /* If this is the first call, add ourselves to the finalizer. */
  if (!initialized)
    {
      ifp_register_finalizer (ifp_cache_finalize_cleanup);
      initialized = TRUE;
    }

  if (ifp_cache_lookup_url_path (url_path))
    {
      ifp_error ("cache: duplicate cache entry for '%s'", url_path);
      return FALSE;
    }

  /* Find the size of the data file, for later use. */
  if (stat (data_file, &statbuf) == -1)
    {
      ifp_error ("cache: unable to stat '%s'", data_file);
      return FALSE;
    }

  /*
   * Create the new entry, and populate it.  The initial reference and usage
   * counts for a new entry are both one.
   */
  entry = ifp_malloc (sizeof (*entry));
  entry->url_path = ifp_malloc (strlen (url_path) + 1);
  strcpy (entry->url_path, url_path);
  entry->data_file = ifp_malloc (strlen (data_file) + 1);
  strcpy (entry->data_file, data_file);
  entry->file_size = statbuf.st_size;
  entry->reference_count = 1;
  entry->usage_count = 1;
  entry->timestamp = ifp_cache_timestamp ();

  /* Add the entry to the cache list head, and scavenge the list. */
  entry->next = ifp_cache_list;
  ifp_cache_list = entry;
  ifp_cache_scavenge ();

  ifp_trace ("cache:"
             " entry cache_%p added successfully", ifp_trace_pointer (entry));
  return TRUE;
}
