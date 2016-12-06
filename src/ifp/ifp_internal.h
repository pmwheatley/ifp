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

#ifndef IFP_INTERNAL_H
#define IFP_INTERNAL_H

/*
 * IF engine header structure.  Each engine exports one of these, and it
 * defines a few things about the engine contained in the plugin.
 */
enum { IFP_HEADER_VERSION = 0x00000300 };
struct ifp_header
{
  int version;                    /* Header version used by the engine. */
  const char *build_timestamp;    /* Date and time of plugin build */

  const char *engine_type;        /* Data type the engine runs. */
  const char *engine_name;        /* Unique engine name. */
  const char *engine_version;     /* Engine's version number. */

  const char *blorb_pattern;      /* If Blorb-capable, the exec type. */

  int acceptor_offset;            /* Offset in file to "magic". */
  int acceptor_length;            /* Length of "magic". */
  const char *acceptor_pattern;   /* Regular expression for "magic". */

  const char *author_name;        /* Interpreter author's name. */
  const char *author_email;       /* Interpreter author's email. */
  const char *engine_home_url;    /* Any URL regarding the interpreter. */

  const char *builder_name;       /* Engine porter/builder's name. */
  const char *builder_email;      /* Engine porter/builder's email. */

  const char *engine_description; /* Miscellaneous engine information. */
  const char *engine_copyright;   /* Engine's copyright information. */
};

/* Skip all except the header definition for IFP plugin headers. */
#if !defined(IFP_HEADER_ONLY)

#include <sys/types.h>
#include <stdio.h>
#include <dirent.h>
#include <gi_dispa.h>
#include <gi_blorb.h>
#include <ifp.h>


/*
 * Library call proxy interface.  This is a vector of functions which would
 * normally be resolved by libc, but which for one reason or another we want
 * to trap in a plugin and bring back to the main program for some additional
 * processing.  Typically, this would be something like malloc(), so that
 * the program that loads the plugin can track what it uses, and garbage-
 * collect when the plugin completes.
 */
enum { IFP_LIBC_VERSION = 0x00000400 };
struct ifp_libc_interface
{
  int version;

  void *(*malloc) (size_t);
  void *(*calloc) (size_t, size_t);
  void *(*realloc) (void *, size_t);
  char *(*strdup) (const char *);
  char *(*getcwd) (char *, size_t);
  int (*scandir) (const char *, struct dirent ***,
                  int (*) (const struct dirent *),
                  int (*) (const void *, const void *));
  void (*free) (void *);

  int (*open_2) (const char *, int);
  int (*open_3) (const char *, int, mode_t);
  int (*close) (int);
  int (*creat) (const char *, mode_t);
  int (*dup) (int);
  int (*dup2) (int, int);

  FILE *(*fopen) (const char *, const char *);
  FILE *(*fdopen) (int, const char *);
  FILE *(*freopen) (const char *, const char *, FILE *);
  int (*fclose) (FILE *);
};


/*
 * Prototypes for libc interception functions.  These are in place only for
 * the plugin handler library code -- a caller of plugin functions should not
 * need to worry about them.
 */
extern void ifp_libc_intercept_exit (int status);
extern void *ifp_libc_intercept_malloc (size_t size);
extern void *ifp_libc_intercept_calloc (size_t nmemb, size_t size);
extern void *ifp_libc_intercept_realloc (void *ptr, size_t size);
extern char *ifp_libc_intercept_strdup (const char *s);
extern char *ifp_libc_intercept_getcwd (char *buf, size_t size);
extern int ifp_libc_intercept_scandir (const char *dir,
                                       struct dirent ***namelist,
                                       int (*select_) (const struct dirent *),
                                       int (*compar_) (const void *,
                                                       const void *));
extern void ifp_libc_intercept_free (void *ptr);

extern int ifp_libc_intercept_open_2 (const char *pathname, int flags);
extern int ifp_libc_intercept_open_3 (const char *pathname,
                                      int flags, mode_t mode);
extern int ifp_libc_intercept_close (int fd);
extern int ifp_libc_intercept_creat (const char *pathname, mode_t mode);
extern int ifp_libc_intercept_dup (int oldfd);
extern int ifp_libc_intercept_dup2 (int oldfd, int newfd);
extern FILE *ifp_libc_intercept_fopen (const char *path, const char *mode);
extern FILE *ifp_libc_intercept_fdopen (int filedes, const char *mode);
extern FILE *ifp_libc_intercept_freopen (const char *path, const char *mode,
                                         FILE *stream);
extern int ifp_libc_intercept_fclose (FILE *stream);


/*
 * Set of supported Glk versions.  When loading a Glk library, IFP needs to
 * take care not to load one with a version number higher than the ones it
 * knows about, since new functions could have crept in that we don't support.
 */
enum {GLK_VERSION_0_6_1 = 0x00000601,
      GLK_VERSION_0_7_0 = 0x00000700 };

/*
 * Glk proxy interface.  This is a vector of glk functions.  A plugin is
 * supplied with a copy of this table that the main program has filled out.
 * The proxy interface will then take each Glk call from the plugin, and
 * handle it by passing to the relevant function from this vector.
 *
 * There is, simply, one entry in the vector for each Glk function that
 * exists in the supported version of Glk.
 */
enum { IFP_GLK_VERSION = 0x00000400 };
struct ifp_glk_interface
{
  int version;

  /* Base Glk functions. */
  void (*glk_exit) (void);
  void (*glk_set_interrupt_handler) (void (*func) (void));
  void (*glk_tick) (void);

  glui32 (*glk_gestalt) (glui32 sel, glui32 val);
  glui32 (*glk_gestalt_ext) (glui32 sel, glui32 val, glui32 *arr,
                             glui32 arrlen);

  unsigned char (*glk_char_to_lower) (unsigned char ch);
  unsigned char (*glk_char_to_upper) (unsigned char ch);

  winid_t (*glk_window_get_root) (void);
  winid_t (*glk_window_open) (winid_t split, glui32 method, glui32 size,
                             glui32 wintype, glui32 rock);
  void (*glk_window_close) (winid_t win, stream_result_t *result);
  void (*glk_window_get_size) (winid_t win, glui32 *widthptr,
                               glui32 *heightptr);
  void (*glk_window_set_arrangement) (winid_t win, glui32 method,
                                      glui32 size, winid_t keywin);
  void (*glk_window_get_arrangement) (winid_t win, glui32 *methodptr,
                                      glui32 *sizeptr, winid_t *keywinptr);
  winid_t (*glk_window_iterate) (winid_t win, glui32 *rockptr);
  glui32 (*glk_window_get_rock) (winid_t win);
  glui32 (*glk_window_get_type) (winid_t win);
  winid_t (*glk_window_get_parent) (winid_t win);
  winid_t (*glk_window_get_sibling) (winid_t win);
  void (*glk_window_clear) (winid_t win);
  void (*glk_window_move_cursor) (winid_t win, glui32 xpos, glui32 ypos);

  strid_t (*glk_window_get_stream) (winid_t win);
  void (*glk_window_set_echo_stream) (winid_t win, strid_t str);
  strid_t (*glk_window_get_echo_stream) (winid_t win);
  void (*glk_set_window) (winid_t win);

  strid_t (*glk_stream_open_file) (frefid_t fileref, glui32 fmode,
                                   glui32 rock);
  strid_t (*glk_stream_open_memory) (char *buf, glui32 buflen,
                                     glui32 fmode, glui32 rock);
  void (*glk_stream_close) (strid_t str, stream_result_t *result);
  strid_t (*glk_stream_iterate) (strid_t str, glui32 *rockptr);
  glui32 (*glk_stream_get_rock) (strid_t str);
  void (*glk_stream_set_position) (strid_t str, glsi32 pos, glui32 seekmode);
  glui32 (*glk_stream_get_position) (strid_t str);
  void (*glk_stream_set_current) (strid_t str);
  strid_t (*glk_stream_get_current) (void);

  void (*glk_put_char) (unsigned char ch);
  void (*glk_put_char_stream) (strid_t str, unsigned char ch);
  void (*glk_put_string) (char *s);
  void (*glk_put_string_stream) (strid_t str, char *s);
  void (*glk_put_buffer) (char *buf, glui32 len);
  void (*glk_put_buffer_stream) (strid_t str, char *buf, glui32 len);
  void (*glk_set_style) (glui32 styl);
  void (*glk_set_style_stream) (strid_t str, glui32 styl);

  glsi32 (*glk_get_char_stream) (strid_t str);
  glui32 (*glk_get_line_stream) (strid_t str, char *buf, glui32 len);
  glui32 (*glk_get_buffer_stream) (strid_t str, char *buf, glui32 len);

  void (*glk_stylehint_set) (glui32 wintype, glui32 styl, glui32 hint,
                             glsi32 val);
  void (*glk_stylehint_clear) (glui32 wintype, glui32 styl, glui32 hint);
  glui32 (*glk_style_distinguish) (winid_t win, glui32 styl1, glui32 styl2);
  glui32 (*glk_style_measure) (winid_t win, glui32 styl, glui32 hint,
                               glui32 *result);

  frefid_t (*glk_fileref_create_temp) (glui32 usage, glui32 rock);
  frefid_t (*glk_fileref_create_by_name) (glui32 usage, char *name,
                                          glui32 rock);
  frefid_t (*glk_fileref_create_by_prompt) (glui32 usage,
                                            glui32 fmode, glui32 rock);
  frefid_t (*glk_fileref_create_from_fileref) (glui32 usage,
                                               frefid_t fref, glui32 rock);
  void (*glk_fileref_destroy) (frefid_t fref);
  frefid_t (*glk_fileref_iterate) (frefid_t fref, glui32 *rockptr);
  glui32 (*glk_fileref_get_rock) (frefid_t fref);
  void (*glk_fileref_delete_file) (frefid_t fref);
  glui32 (*glk_fileref_does_file_exist) (frefid_t fref);

  void (*glk_select) (event_t *event);
  void (*glk_select_poll) (event_t *event);

  void (*glk_request_timer_events) (glui32 millisecs);

  void (*glk_request_line_event) (winid_t win, char *buf,
                                     glui32 maxlen, glui32 initlen);
  void (*glk_request_char_event) (winid_t win);
  void (*glk_request_mouse_event) (winid_t win);

  void (*glk_cancel_line_event) (winid_t win, event_t *event);
  void (*glk_cancel_char_event) (winid_t win);
  void (*glk_cancel_mouse_event) (winid_t win);

  glui32 (*glk_buffer_to_lower_case_uni) (glui32 *buf,
                                          glui32 len, glui32 numchars);
  glui32 (*glk_buffer_to_upper_case_uni) (glui32 *buf,
                                          glui32 len, glui32 numchars);
  glui32 (*glk_buffer_to_title_case_uni) (glui32 *buf, glui32 len,
                                          glui32 numchars, glui32 lowerrest);

  void (*glk_put_char_uni) (glui32 ch);
  void (*glk_put_string_uni) (glui32 *s);
  void (*glk_put_buffer_uni) (glui32 *buf, glui32 len);

  void (*glk_put_char_stream_uni) (strid_t str, glui32 ch);
  void (*glk_put_string_stream_uni) (strid_t str, glui32 *s);
  void (*glk_put_buffer_stream_uni) (strid_t str, glui32 *buf, glui32 len);

  glsi32 (*glk_get_char_stream_uni) (strid_t str);
  glui32 (*glk_get_buffer_stream_uni) (strid_t str,
                                       glui32 *buf, glui32 len);
  glui32 (*glk_get_line_stream_uni) (strid_t str,
                                     glui32 *buf, glui32 len);

  strid_t (*glk_stream_open_file_uni) (frefid_t fileref,
                                       glui32 fmode, glui32 rock);
  strid_t (*glk_stream_open_memory_uni) (glui32 *buf, glui32 buflen,
                                         glui32 fmode, glui32 rock);

  void (*glk_request_char_event_uni) (winid_t win);
  void (*glk_request_line_event_uni) (winid_t win, glui32 *buf,
                                      glui32 maxlen, glui32 initlen);

  glui32 (*glk_image_draw) (winid_t win, glui32 image, glsi32 val1,
                            glsi32 val2);
  glui32 (*glk_image_draw_scaled) (winid_t win, glui32 image,
                                   glsi32 val1, glsi32 val2, glui32 width,
                                   glui32 height);
  glui32 (*glk_image_get_info) (glui32 image, glui32 *width,
                                glui32 *height);

  void (*glk_window_flow_break) (winid_t win);

  void (*glk_window_erase_rect) (winid_t win,
                                 glsi32 left, glsi32 top, glui32 width,
                                 glui32 height);
  void (*glk_window_fill_rect) (winid_t win, glui32 color,
                                glsi32 left, glsi32 top, glui32 width,
                                glui32 height);
  void (*glk_window_set_background_color) (winid_t win, glui32 color);

  schanid_t (*glk_schannel_create) (glui32 rock);
  void (*glk_schannel_destroy) (schanid_t chan);
  schanid_t (*glk_schannel_iterate) (schanid_t chan, glui32 *rockptr);
  glui32 (*glk_schannel_get_rock) (schanid_t chan);

  glui32 (*glk_schannel_play) (schanid_t chan, glui32 snd);
  glui32 (*glk_schannel_play_ext) (schanid_t chan, glui32 snd,
                                   glui32 repeats, glui32 notify);
  void (*glk_schannel_stop) (schanid_t chan);
  void (*glk_schannel_set_volume) (schanid_t chan, glui32 vol);

  void (*glk_sound_load_hint) (glui32 snd, glui32 flag);

  void (*glk_set_hyperlink) (glui32 linkval);
  void (*glk_set_hyperlink_stream) (strid_t str, glui32 linkval);
  void (*glk_request_hyperlink_event) (winid_t win);
  void (*glk_cancel_hyperlink_event) (winid_t win);

  void (*glkunix_set_base_file) (char *filename);
  strid_t (*glkunix_stream_open_pathname) (char *pathname,
                                           glui32 textmode, glui32 rock);

  /* Dispatch-layer Glk functions. */
  void (*gidispatch_set_object_registry) (gidispatch_rock_t (*regi)
                                          (void *obj, glui32 objclass),
                                          void (*unregi) (void *obj,
                                                          glui32 objclass,
                                                          gidispatch_rock_t
                                                          objrock));
  gidispatch_rock_t (*gidispatch_get_objrock) (void *obj, glui32 objclass);
  void (*gidispatch_set_retained_registry) (gidispatch_rock_t (*regi)
                                            (void *array, glui32 len,
                                             char *typecode),
                                            void (*unregi) (void *array,
                                                            glui32 len,
                                                            char *typecode,
                                                            gidispatch_rock_t
                                                            objrock));

  void (*gidispatch_call) (glui32 funcnum, glui32 numargs,
                           gluniversal_t *arglist);
  char *(*gidispatch_prototype) (glui32 funcnum);
  glui32 (*gidispatch_count_classes) (void);
  glui32 (*gidispatch_count_intconst) (void);
  gidispatch_intconst_t *(*gidispatch_get_intconst) (glui32 index_);
  glui32 (*gidispatch_count_functions) (void);
  gidispatch_function_t *(*gidispatch_get_function) (glui32 index_);
  gidispatch_function_t *(*gidispatch_get_function_by_id) (glui32 id);

  /* Blorb-layer Glk functions. */
  giblorb_err_t (*giblorb_create_map) (strid_t file,
                                       giblorb_map_t **newmap);
  giblorb_err_t (*giblorb_destroy_map) (giblorb_map_t *map);

  giblorb_err_t (*giblorb_load_chunk_by_type) (giblorb_map_t *map,
                                               glui32 method,
                                               giblorb_result_t *res,
                                               glui32 chunktype,
                                               glui32 count);
  giblorb_err_t (*giblorb_load_chunk_by_number) (giblorb_map_t *map,
                                                 glui32 method,
                                                 giblorb_result_t *res,
                                                 glui32 chunknum);
  giblorb_err_t (*giblorb_unload_chunk) (giblorb_map_t *map,
                                         glui32 chunknum);

  giblorb_err_t (*giblorb_load_resource) (giblorb_map_t *map,
                                          glui32 method,
                                          giblorb_result_t *res,
                                          glui32 usage, glui32 resnum);
  giblorb_err_t (*giblorb_count_resources) (giblorb_map_t *map,
                                            glui32 usage, glui32 *num,
                                            glui32 *min, glui32 *max);

  giblorb_err_t (*giblorb_set_resource_map) (strid_t file);
  giblorb_map_t *(*giblorb_get_resource_map) (void);

  /* Finally, a function we use for our own nefarious purposes. */
  strid_t (*gli_stream_open_pathname) (char *pathname,
                                       int textmode, glui32 rock);
};


/* Additional IFP prototypes for control interface functions. */
typedef struct ifp_prefs *ifp_prefref_t;
extern void ifpi_initializer (void);
extern void ifpi_finalizer (void);
extern int ifpi_attach_glk_interface (ifp_glk_interfaceref_t interface);
extern ifp_glk_interfaceref_t ifpi_retrieve_glk_interface (void);
extern int ifpi_attach_libc_interface (ifp_libc_interfaceref_t interface);
extern ifp_libc_interfaceref_t ifpi_retrieve_libc_interface (void);
extern void ifpi_chain_set_plugin_self (ifp_pluginref_t plugin);
extern ifp_pluginref_t ifpi_chain_return_plugin (void);
extern void ifpi_chain_accept_preferences (ifp_prefref_t prefs_list);
extern void ifpi_chain_accept_plugin_path (const char *new_path);
extern int ifpi_glkunix_startup_code (glkunix_startup_t *startdata);
extern void ifpi_glk_main (void);
extern void ifpi_force_link (void);

/* Internal-only interfaces. */
extern void ifp_trace (const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void ifp_notice (const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void ifp_error (const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void ifp_fatal (const char *format, ...)
    __attribute__ ((format (printf, 1, 2)))
    __attribute__ ((noreturn));
extern const void *ifp_trace_pointer (const void *pointer);

extern void ifp_config_read (void);
extern void ifp_main_set_glk_libraries (const char *glk_libraries);
extern int ifp_glk_load_interface (const char *filename);
extern int ifp_glk_verify_dso (const char *filename);
extern void *ifp_glk_get_main (void);
extern void ifp_glk_unload_interface (void);

extern void ifp_plugin_set_next (ifp_pluginref_t plugin,
                                 ifp_pluginref_t next);
extern ifp_pluginref_t ifp_plugin_get_next (ifp_pluginref_t plugin);
extern void ifp_plugin_set_prior (ifp_pluginref_t plugin,
                                  ifp_pluginref_t prior);
extern ifp_pluginref_t ifp_plugin_get_prior (ifp_pluginref_t plugin);
extern void ifp_plugin_force_unload (ifp_pluginref_t plugin);

typedef struct ifp_header *ifp_headerref_t;
extern ifp_headerref_t ifp_plugin_get_header (ifp_pluginref_t plugin);

extern void ifp_self_set_plugin (ifp_pluginref_t plugin);
extern ifp_pluginref_t ifp_self (void);
extern int  ifp_self_inside_plugin (void);
extern void ifp_chain_set_chained_plugin (ifp_pluginref_t plugin);
extern ifp_pluginref_t ifp_chain_get_chained_plugin (void);

extern void ifp_pref_use_foreign_data (ifp_prefref_t prefs_list);
extern ifp_prefref_t ifp_pref_get_local_data (void);
extern const char *ifp_cache_find_entry (const char *url_path);
extern void ifp_cache_release_entry (const char *url_path);
extern void ifp_cache_remove_entry (const char *url_path);
extern int  ifp_cache_add_entry (const char *url_path, const char *data_file);
extern void ifp_http_sigio_handler (void);
extern void ifp_http_poll_handler (void);
extern void ifp_http_cancel_download (void);
extern int  ifp_http_download (int tofd, const char *host,
                               int port, const char *document, int *progress,
                               int *status);
extern void ifp_ftp_sigio_handler (void);
extern void ifp_ftp_poll_handler (void);
extern void ifp_ftp_cancel_download (void);
extern int  ifp_ftp_download (int tofd, const char *host,
                              int port, const char *document, int *progress,
                              int *status);
extern void ifp_register_finalizer (void (*finalizer) (void));
extern strid_t ifp_glkstream_open_pathname (char *pathname,
                                            glui32 textmode, glui32 rock);
extern void ifp_glkstream_close (strid_t glk_stream, stream_result_t *result);
extern void *ifp_dlopen (const char *filename);
extern const char *ifp_dlerror (void);
extern void *ifp_dlsym (void *handle, const char *symbol);
extern int  ifp_dlclose (void *handle);
extern void *ifp_malloc (size_t size) __attribute__ ((malloc));
extern void *ifp_realloc (void *ptr, size_t size) __attribute__ ((malloc));
extern void ifp_free (void *pointer);
extern int ifp_split_string (const char *string,
                             char separator, char ***elements);
extern void ifp_free_split_string (char **elements, int count);

#endif
#endif
