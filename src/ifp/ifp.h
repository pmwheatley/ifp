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

#ifndef IFP_H
#define IFP_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "glk.h"
#include "glkstart.h"

/* Plugin trace and error reporting function definitions. */
extern void ifp_trace_select (const char *selector);
extern void ifp_messages (int enabled);
extern int ifp_messages_enabled (void);

/* URL and related "class" function definitions. */
typedef struct ifp_url *ifp_urlref_t;
extern int ifp_url_is_valid (ifp_urlref_t url);
extern void ifp_url_set_pause_timeout (int timeout);
extern int ifp_url_get_pause_timeout (void);
extern ifp_urlref_t ifp_url_new (void);
extern void ifp_url_destroy (ifp_urlref_t url);
extern const char *ifp_url_get_url_path (ifp_urlref_t url);
extern const char *ifp_url_get_data_file (ifp_urlref_t url);
extern int ifp_url_is_remote (ifp_urlref_t url);
extern void ifp_url_scrub (ifp_urlref_t url);
extern int ifp_url_poll_resolved_async (ifp_urlref_t url);
extern int ifp_url_poll_progress_async (ifp_urlref_t url);
extern int ifp_url_get_status_async (ifp_urlref_t url);
extern void ifp_url_pause_async (ifp_urlref_t url);
extern int ifp_url_resolve_async (ifp_urlref_t url, const char *urlpath);
extern int ifp_url_resolve (ifp_urlref_t url, const char *urlpath);
extern ifp_urlref_t ifp_url_new_resolve (const char *urlpath);
extern ifp_urlref_t ifp_url_new_resolve_async (const char *urlpath);
extern void ifp_url_forget (ifp_urlref_t url);

/* Plugin "class" function definitions. */
typedef struct ifp_plugin *ifp_pluginref_t;
typedef struct ifp_glk_interface *ifp_glk_interfaceref_t;
typedef struct ifp_libc_interface *ifp_libc_interfaceref_t;
extern int ifp_plugin_is_valid (ifp_pluginref_t plugin);
extern int ifp_plugin_is_loaded (ifp_pluginref_t plugin);
extern int ifp_plugin_is_initializable (ifp_pluginref_t plugin);
extern int ifp_plugin_is_runnable (ifp_pluginref_t plugin);
extern glkunix_argumentlist_t
    *ifp_plugin_get_arguments (ifp_pluginref_t plugin);
extern const char *ifp_plugin_get_filename (ifp_pluginref_t plugin);
extern void ifp_plugin_chain_set_self (ifp_pluginref_t plugin);
extern int ifp_plugin_can_chain (ifp_pluginref_t plugin);
extern ifp_pluginref_t ifp_plugin_get_chain (ifp_pluginref_t plugin);
extern ifp_pluginref_t ifp_plugin_new (void);
extern void ifp_plugin_destroy (ifp_pluginref_t plugin);
extern int ifp_plugin_is_unloadable (ifp_pluginref_t plugin);
extern void ifp_plugin_unload (ifp_pluginref_t plugin);
extern int ifp_plugin_load (ifp_pluginref_t plugin, const char *filename);
extern ifp_pluginref_t ifp_plugin_new_load (const char *filename);
extern int
    ifp_plugin_attach_glk_interface (ifp_pluginref_t plugin,
                                     ifp_glk_interfaceref_t glk_interface);
extern ifp_glk_interfaceref_t
    ifp_plugin_retrieve_glk_interface (ifp_pluginref_t plugin);
extern int
    ifp_plugin_attach_libc_interface (ifp_pluginref_t plugin,
                                      ifp_libc_interfaceref_t libc_interface);
extern ifp_libc_interfaceref_t
   ifp_plugin_retrieve_libc_interface (ifp_pluginref_t plugin);
extern int ifp_plugin_initialize (ifp_pluginref_t plugin,
                                  glkunix_startup_t *data);
extern void ifp_plugin_run (ifp_pluginref_t plugin);
extern void ifp_plugin_cancel (ifp_pluginref_t plugin);

/*
 * Synonyms for plugin control functions.  These are equivalent to the calls
 * ifp_initialize_plugin() and ifp_run_plugin(), and exist to parallel the Glk
 * interface functions glkunix_startup_code() and glk_main().
 */
extern int ifp_plugin_glkunix_startup_code (ifp_pluginref_t plugin,
                                            glkunix_startup_t *data);
extern void ifp_plugin_glk_main (ifp_pluginref_t plugin);

/* Engine header convenience functions. */
extern const char *ifp_plugin_engine_type (ifp_pluginref_t plugin);
extern const char *ifp_plugin_engine_name (ifp_pluginref_t plugin);
extern const char *ifp_plugin_engine_version (ifp_pluginref_t plugin);
extern const char *ifp_plugin_blorb_pattern (ifp_pluginref_t plugin);
extern int ifp_plugin_acceptor_offset (ifp_pluginref_t plugin);
extern int ifp_plugin_acceptor_length (ifp_pluginref_t plugin);
extern const char *ifp_plugin_acceptor_pattern (ifp_pluginref_t plugin);
extern ifp_pluginref_t ifp_plugin_find_chain_end (ifp_pluginref_t plugin);
extern int ifp_plugin_is_equal (ifp_pluginref_t plugin, ifp_pluginref_t check);
extern void ifp_plugin_dissect_header (ifp_pluginref_t plugin,
                                       int *ifp_version,
                                       const char **engine_type,
                                       const char **engine_name,
                                       const char **engine_version,
                                       const char **build_timestamp,
                                       const char **blorb_type,
                                       int *acceptor_offset,
                                       int *acceptor_length,
                                       const char **acceptor_pattern,
                                       const char **author_name,
                                       const char **author_email,
                                       const char **engine_home_url,
                                       const char **builder_name,
                                       const char **builder_email,
                                       const char **engine_description,
                                       const char **engine_copyright);

/* Plugin loader function definitions. */
extern ifp_pluginref_t ifp_loader_iterate_plugins (ifp_pluginref_t current);
extern int ifp_loader_count_plugins (void);
extern ifp_pluginref_t ifp_loader_replace_with_clone (ifp_pluginref_t plugin);
extern ifp_pluginref_t ifp_loader_load_plugin (const char *filename);
extern int ifp_loader_search_plugins_directory (const char *dir_path);
extern int ifp_loader_search_plugins_path (const char *load_path);
extern void ifp_loader_forget_plugin (ifp_pluginref_t plugin);
extern int ifp_loader_forget_all_plugins (void);

/* Glk handler function definition. */
extern ifp_glk_interfaceref_t ifp_glk_get_interface (void);

/* Libc functions handler function definition. */
extern ifp_libc_interfaceref_t ifp_libc_get_interface (void);

/* Libc memory and file cleanup functions. */
extern void ifp_memory_malloc_garbage_collect (void);
extern void ifp_file_open_files_cleanup (void);

/* Game data recognizer function definitions. */
extern int ifp_recognizer_match_string (const char *string,
                                        const char *pattern);
extern int ifp_recognizer_match_binary (const char *buffer,
                                        int length, const char *pattern);

/* Blorb data helper function definitions. */
extern int ifp_blorb_is_file_blorb (strid_t glk_stream);
extern int ifp_blorb_first_exec_type (strid_t glk_stream, glui32 *blorb_type);
extern char *ifp_blorb_id_to_string (glui32 blorb_type);

/* Plugin preferences builder module functions. */
extern glkunix_argumentlist_t
    *ifp_pref_list_arguments (const char *engine_name,
                              const char *engine_version);
extern void ifp_pref_unregister (const char *engine_name,
                                 const char *engine_version,
                                 const char *preference);
extern int ifp_pref_register (const char *engine_name,
                              const char *engine_version,
                              const char *preference);
extern glkunix_startup_t
    *ifp_pref_create_startup_data (ifp_pluginref_t plugin,
                                   const char *filename);
extern glkunix_startup_t
    *ifp_pref_create_startup_data_url (ifp_pluginref_t plugin,
                                       ifp_urlref_t url);
extern void ifp_pref_forget_startup_data (glkunix_startup_t *data);

/* Plugin manager function definitions. */
extern const char *ifp_manager_build_timestamp (void);
extern void ifp_manager_clone_selected_plugins (int flag);
extern int ifp_manager_cloning_selected (void);
extern void ifp_manager_set_plugin_path (const char *new_path);
extern const char *ifp_manager_get_plugin_path (void);
extern int ifp_manager_reset_glk_library_partial (void);
extern int ifp_manager_reset_glk_library_full (void);
extern int ifp_manager_collect_plugin_garbage (void);
extern ifp_pluginref_t ifp_manager_locate_plugin (const char *filename);
extern ifp_pluginref_t ifp_manager_locate_plugin_url (ifp_urlref_t url);
extern void ifp_manager_run_plugin (ifp_pluginref_t plugin);

/* URL cache function definitions. */
extern void ifp_cache_set_limit (int limit);
extern int ifp_cache_get_limit (void);
extern int ifp_cache_size (void);

/* Const-correct Glk convenience wrapper functions. */
extern strid_t glk_c_stream_open_memory (const char *buf, glui32 buflen,
                                         glui32 fmode, glui32 rock);
extern void glk_c_put_string (const char *s);
extern void glk_c_put_string_stream (strid_t str, const char *s);
extern void glk_c_put_buffer (const char *buf, glui32 len);
extern void glk_c_put_buffer_stream (strid_t str, const char *buf, glui32 len);
extern strid_t glk_c_stream_open_memory_uni (const glui32 *buf, glui32 buflen,
                                             glui32 fmode, glui32 rock);
extern void glk_c_put_string_uni (const glui32 *s);
extern void glk_c_put_buffer_uni (const glui32 *buf, glui32 len);
extern void glk_c_put_string_stream_uni (strid_t str, const glui32 *s);
extern void glk_c_put_buffer_stream_uni (strid_t str,
                                         const glui32 *buf, glui32 len);

#if defined(__cplusplus)
}
#endif
#endif
