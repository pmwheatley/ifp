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
#include <string.h>
#include <limits.h>

#include "ifp.h"
#include "ifp_internal.h"


/**
 * ifp_blorb_is_file_blorb()
 *
 * This function returns TRUE if the file passed in looks like a Blorb file,
 * FALSE otherwise.  It reads what in a blorb file is the file header, 12
 * bytes, and checks for "FORM", 4 don't-care bytes, and "IFRS", returning
 * TRUE if found.
 */
int
ifp_blorb_is_file_blorb (strid_t stream)
{
  char blorb_header[12];
  int bytes;

  ifp_trace ("blorb: ifp_blorb_is_file_blorb <-"
             " stream_%p", ifp_trace_pointer (stream));

  glk_stream_set_position (stream, 0, seekmode_Start);
  bytes = glk_get_buffer_stream (stream, blorb_header, sizeof (blorb_header));
  if (bytes != sizeof (blorb_header))
    {
      ifp_trace ("blorb: error reading file header");
      return FALSE;
    }

  return memcmp (blorb_header, "FORM", strlen ("FORM")) == 0
         && memcmp (blorb_header + 8, "IFRS", strlen ("IFRS")) == 0;
}


/**
 * ifp_blorb_first_exec_type()
 *
 * Given a Glk stream, try to create resource map from it.  If this succeeds,
 * find the first executable chunk.  Get the chunk type from that, and return
 * it in blorb_type.  Returns TRUE if the stream is valid Blorb data and
 * contains an executable chunk, FALSE otherwise.
 */
int
ifp_blorb_first_exec_type (strid_t stream, glui32 *blorb_type)
{
  giblorb_result_t blorb_result;
  giblorb_map_t *blorb_map;

  ifp_trace ("blorb: ifp_blorb_first_exec_type <-"
             " stream_%p", ifp_trace_pointer (stream));

  glk_stream_set_position (stream, 0, seekmode_Start);
  if (giblorb_create_map (stream, &blorb_map) != 0)
    {
      ifp_trace ("blorb: file is not valid Blorb");
      return FALSE;
    }

  /* Locate the first executable resource chunk in the map. */
  if (giblorb_load_resource (blorb_map, giblorb_method_FilePos,
                             &blorb_result, giblorb_ID_Exec, 0) != 0)
    {
      ifp_trace ("blorb: file contains no executable chunk");
      giblorb_destroy_map (blorb_map);
      return FALSE;
    }
  giblorb_destroy_map (blorb_map);

  ifp_trace ("blorb: ifp_blorb_first_exec_type returned 0x%lX",
             blorb_result.chunktype);
  *blorb_type = blorb_result.chunktype;
  return TRUE;
}


/**
 * ifp_blorb_id_to_string()
 *
 * Converts a Blorb type integer into a four-character string suitable for
 * comparisons and pattern matches.  The result string must be freed by
 * the caller when finished.
 */
char *
ifp_blorb_id_to_string (glui32 blorb_type)
{
  char *string;
  int length, index_;

  ifp_trace ("blorb: ifp_blorb_id_to_string <- 0x%lx", blorb_type);

  length = sizeof (blorb_type);
  string = ifp_malloc (length + 1);

  for (index_ = length - 1; index_ >= 0; index_--)
    {
      string[index_] = blorb_type & UCHAR_MAX;
      blorb_type >>= CHAR_BIT;
    }
  string[length] = '\0';

  ifp_trace ("blorb: string type is '%s'", string);
  return string;
}
