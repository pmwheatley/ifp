/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2006  Simon Baldwin (simon_baldwin@yahoo.com)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#include <stdio.h>

#include <glk.h>
#include <ifp.h>


/* True and False macros, in case they're missing. */
#ifndef FALSE
#  define FALSE (0)
#endif
#ifndef TRUE
#  define TRUE (!FALSE)
#endif

/* Malloc wrappers. */
extern void *memory_malloc (size_t size);
extern void *memory_realloc (void *ptr, size_t size);
extern void memory_free (void *ptr);
extern char *memory_strdup (const char *string);

/* Simple vector implementation. */
typedef struct vector_s *vectorref_t;
extern vectorref_t vector_create (int dimension);
extern void vector_destroy (vectorref_t vector);
extern const void *vector_get_address (const vectorref_t vector, int index_);
extern void vector_get (const vectorref_t vector, int index_, void *element);
extern void vector_set (vectorref_t vector, int index_, const void *element);
extern void vector_delete (const vectorref_t vector, int index_);
extern int vector_get_length (const vectorref_t vector);
extern void vector_clear (vectorref_t vector);
extern int vector_append (vectorref_t vector, const void *element);
extern void vector_remove (vectorref_t vector, void *element);
extern void vector_unqueue (vectorref_t vector, void *element);

/* Simple string hash/digest. */
extern glui32 hash (const char *string);

/* Basic game aggregation functions. */
typedef struct game_s *gameref_t;
extern void gameset_add_game (const char *about,
                              const char *author, const char *byline,
                              const char *description, const char *genre,
                              const char *headline, const char *length,
                              const char *publisher, const char *release_date,
                              const char *title, const char *version);
extern int gameset_is_game (const gameref_t game);
extern void gameset_get_game (const gameref_t game,
                              const char **about,
                              const char **author, const char **byline,
                              const char **description, const char **genre,
                              const char **headline, const char **length,
                              const char **publisher, const char **release_date,
                              const char **title, const char **version);
extern gameref_t gameset_iterate (const gameref_t cursor);
extern int gameset_is_empty (void);
extern void gameset_erase (void);

extern gameref_t gameset_find_game (const char *location);
extern const char *gameset_get_game_location (const gameref_t game);
extern const char *gameset_get_game_title (const gameref_t game);
extern const char *gameset_get_game_author (const gameref_t game);
extern const char *gameset_get_game_publisher (const gameref_t game);
extern const char *gameset_get_game_genre (const gameref_t game);

extern glui32 gameset_get_game_id (const gameref_t game);
extern gameref_t gameset_id_to_game (glui32 id);

/* Grouping game aggregation functions. */
typedef struct group_s *groupref_t;
extern groupref_t gamegroup_add_group (const char *about,
                                       const char *title,
                                       const char *description);
extern int gamegroup_is_group (const groupref_t group);
extern void gamegroup_get_group (const groupref_t group,
                                 const char **about,
                                 const char **title,
                                 const char **description);
extern groupref_t gamegroup_iterate (const groupref_t group);
extern int gamegroup_is_empty (void);
extern void gamegroup_erase (void);

extern groupref_t gamegroup_find_group (const char *name);
extern const char *gamegroup_get_group_title (const groupref_t group);

extern int gamegroup_add_game_to_group (groupref_t group, const char *location);
extern int gamegroup_add_group_to_group (groupref_t group, const char *about);

typedef union node_u *noderef_t;
extern gameref_t gamegroup_get_node_game (const noderef_t node);
extern groupref_t gamegroup_get_node_group (const noderef_t node);
extern int gamegroup_is_node_game (const noderef_t node);
extern int gamegroup_is_node_group (const noderef_t node);

extern groupref_t gamegroup_get_root (void);

extern noderef_t gamegroup_iterate_group (const groupref_t group,
                                          const noderef_t node);
extern int gamegroup_group_is_empty (const groupref_t group);

extern glui32 gamegroup_get_group_id (const groupref_t group);
extern groupref_t gamegroup_id_to_group (glui32 id);

/* Interpreter discovery functions. */
typedef struct terp_s *terpref_t;
extern void terps_discover (void);
extern int terps_is_interpreter (const terpref_t terp);
extern void terps_get_interpreter (const terpref_t terp,
                int *version, const char **build_timestamp,
                const char **engine_type, const char **engine_name,
                const char **engine_version, const char **blorb_pattern,
                int *acceptor_offset, int *acceptor_length,
                const char **acceptor_pattern, const char **author_name,
                const char **author_email, const char **engine_home_url,
                const char **builder_name, const char **builder_email,
                const char **engine_description, const char **engine_copyright);
extern terpref_t terps_iterate (const terpref_t terp);
extern int terps_is_empty (void);
extern void terps_erase (void);

extern const char *terps_get_interpreter_engine_name (const terpref_t terp);

/* Message line functions. */
extern void message_begin_dialog (void);
extern void message_end_dialog (void);
extern void message_read_line (const char *prompt, char *buffer, int length);
extern int message_confirm (const char *prompt);
extern void message_wait_for_keypress (void);
extern void message_write_line (int return_immediately,
                                const char *format, ...)
    __attribute__ ((__format__ (__printf__, 2, 3)));
extern void message_windows_closed (void);

/* URL handler functions. */
extern void url_cleanup (void);
extern ifp_urlref_t url_resolve (winid_t window,
                                 const char *url_path, int *url_errno);

/* UTF handler and XML parser functions. */
extern char *utf_utf8_to_iso8859 (const unsigned char *utf_string);
extern int xml_parse_file (const char *xml_file, int dump_document);

/* INI reader and parser functions. */
typedef struct inidoc_s *inidocref_t;
typedef struct inisection_s *inisectionref_t;
extern inisectionref_t inidoc_iterate (inidocref_t doc,
                                       inisectionref_t section);
extern const char *inidoc_get_name (inisectionref_t section);
extern const char *inidoc_get_property_value (inisectionref_t section,
                                              const char *property);
extern const char *inidoc_get_global_property_value (inidocref_t doc,
                                                     const char *property);
extern void inidoc_free (inidocref_t doc);
extern void inidoc_debug_dump (FILE *stream, inidocref_t doc);
extern inidocref_t inidoc_parse (const char *file);
extern int ini_parse_file (const char *ini_file, int dump_document);

/* Public formatting and display functions. */
extern void display_main_loop (void);
extern void display_handle_redraw (void);

/* Protected formatting and page display functions. */
extern int display_sort_games_by_author (void);
extern int display_sort_games_by_genre (void);
extern int display_show_games_in_full (void);
extern int display_show_interpreters_in_full (void);
extern void display_get_pagination_codes (int *prior_page_code,
                                          int *next_page_code);
extern void display_button (int hyperlink, const char *legend, int is_active);

extern void gamepage_display (const groupref_t group, vectorref_t display_map,
                              int has_hyperlinks, int page_increment,
                              int is_endpoint);
extern void terppage_display (void);
extern void aboutpage_display (int has_hyperlinks);
