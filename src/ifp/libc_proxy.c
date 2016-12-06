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

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <dirent.h>

#include "ifp.h"
#include "ifp_internal.h"


/*
 * Function prototype, present only to silence compiler warnings.  Ordinarily
 * we'd include string.h to get this, but strdup is a macro definition, at
 * least in Linux, so that's not an option here.
 */
char *strdup (const char *s);

/* Pointer to our interface table. */
static ifp_libc_interfaceref_t libc_interface = NULL;


/*
 * ifpi_attach_libc_interface()
 *
 * Attach a Libc interface to the plugin.  This function is called on
 * plugin load to inform it of how all of its intercepted libc requests
 * will work.
 *
 * It must be called before any plugin code is executed.  The function
 * returns TRUE if the interface is usable (that is, has the expected
 * version number), otherwise FALSE.
 */
int
ifpi_attach_libc_interface (ifp_libc_interfaceref_t interface)
{
  if (interface && interface->version != IFP_LIBC_VERSION)
    return FALSE;

  libc_interface = interface;
  return TRUE;
}


/*
 * ifpi_retrieve_libc_interface()
 *
 * Return the current attached Libc interface.  The function returns NULL
 * if no Libc interface is attached.
 */
ifp_libc_interfaceref_t
ifpi_retrieve_libc_interface (void)
{
  return libc_interface;
}


/*
 * ifp_check_interface()
 *
 * Ensure the plugin has a Libc interface to work with.  Since this interface
 * gives us fundamental libc functions like malloc, it's pretty terminal if
 * there is none when the plugin calls malloc.
 */
static void
ifp_check_interface (void)
{
  if (!libc_interface)
    {
      fprintf (stderr,
               "IFP plugin library error: "
               "libc_proxy: no usable libc interface\n");
      raise (SIGABRT);
      _exit (1);
    }
}


/**
 * malloc(), calloc(), realloc(), strdup(), getcwd(), scandir(), free()
 * open(), close(), creat(), dup(), dup2()
 * fopen(), fdopen(), freopen(), fclose()
 *
 * Proxy Libc functions.  These functions handle the calls from the
 * interpreter itself, communicating them to the outside world through
 * the libc interface passed in when attaching the plugin.  They need
 * to be global, but not necessarily visible outside the library.
 *
 * Calls to these libc functions from within a plugin bind to these
 * definitions, and when called, these proxies forward the actual call
 * through the libc interface and back into the main program.  Redefining
 * libc functions here relies on these same functions being "weak"
 * functions in libc.
 *
 * The main program handles calls to these functions in the most
 * appropriate manner.  Usually, it will track usages of various system
 * resources, so that it can free those resources when the plugin exits.
 */
void *
malloc (size_t size)
{
  ifp_check_interface ();
  return libc_interface->malloc (size);
}

void *
calloc (size_t nmemb, size_t size)
{
  ifp_check_interface ();
  return libc_interface->calloc (nmemb, size);
}

void *
realloc (void *ptr, size_t size)
{
  ifp_check_interface ();
  return libc_interface->realloc (ptr, size);
}

char *
strdup (const char *s)
{
  ifp_check_interface ();
  return libc_interface->strdup (s);
}

char *
getcwd (char *buf, size_t size)
{
  ifp_check_interface ();
  return libc_interface->getcwd (buf, size);
}

//int
//scandir (const char *dir, struct dirent ***namelist,
//         int (*select_) (const struct dirent *),
//         int (*compar_) (const void *, const void *))
//{
//  ifp_check_interface ();
//  return libc_interface->scandir (dir, namelist, select_, compar_);
//}

void
free (void *ptr)
{
  ifp_check_interface ();
  libc_interface->free (ptr);
}

int
open (const char *pathname, int flags, ...)
{
  int fd;

  /* Split into two and three arg variants. */
  if (flags & O_CREAT)
    {
      va_list ap;
      va_start (ap, flags);
      fd = libc_interface->open_3 (pathname, flags, va_arg (ap, int));
      va_end (ap);
    }
  else
    fd = libc_interface->open_2 (pathname, flags);
  return fd;
}

int
close (int fd)
{
  ifp_check_interface ();
  return libc_interface->close (fd);
}

int
creat (const char *pathname, mode_t mode)
{
  ifp_check_interface ();
  return libc_interface->creat (pathname, mode);
}

int
dup (int oldfd)
{
  ifp_check_interface ();
  return libc_interface->dup (oldfd);
}

int
dup2 (int oldfd, int newfd)
{
  ifp_check_interface ();
  return libc_interface->dup2 (oldfd, newfd);
}

FILE *
fopen (const char *path, const char *mode)
{
  ifp_check_interface ();
  return libc_interface->fopen (path, mode);
}

FILE *
fdopen (int filedes, const char *mode)
{
  ifp_check_interface ();
  return libc_interface->fdopen (filedes, mode);
}

FILE *
freopen (const char *path, const char *mode, FILE * stream)
{
  ifp_check_interface ();
  return libc_interface->freopen (path, mode, stream);
}

int
fclose (FILE * stream)
{
  ifp_check_interface ();
  return libc_interface->fclose (stream);
}


/*
 * exit()
 *
 * The Glk specification requires a Glk interpreter to exit with a call
 * to glk_exit() rather than to exit().  However, this may not always be
 * the case, and an interpreter that calls exit() will stop the entire
 * IFP process, which may not be what we want to happen.  So the IFP
 * plugin library defines exit() here, and redirects it to be a call to
 * glk_exit().  If an interpreter _must_ exit, it should call _exit();
 * this is not trapped by the IFP library.
 */
void
exit (int status)
{
  glk_exit ();

  fprintf (stderr,
           "IFP plugin library error: "
           "libc_proxy: failed to halt interpreter [%d]\n", status);
  raise (SIGABRT);
  _exit (status);
}


/*
 * atexit()
 *
 * Catch calls an interpreter makes to atexit(), and save the handler
 * details.  The given handlers are called on library unload (the IFP
 * equivalent of exit()), in reverse order, using the IFP finalizer.
 */
int
atexit (void (*function) (void))
{
  ifp_register_finalizer (function);

  /* How can atexit() fail?  The man page for it doesn't say. */
  return 0;
}


/*
 * abort()
 *
 * Catch calls an interpreter makes to abort(), either directly or as a
 * result of assertion failure.  Print an error message to stderr, then
 * handle as a call to glk_exit().
 */
void
abort (void)
{
  fprintf (stderr,
           "IFP plugin library error: "
           "libc_proxy: interpreter called abort(), handling as glk_exit()\n");
  glk_exit ();

  fprintf (stderr,
           "IFP plugin library error: "
           "libc_proxy: failed to halt interpreter\n");
  raise (SIGABRT);
  _exit (1);
}



/*
 * ifp_unsupported_libc_error()
 *
 * Issue an error message for the unsupported, or unsupportable, Libc
 * function, and stop the interpreter plugin by calling glk_exit().  The
 * ellipsis argument is a trick to allow otherwise unused function arguments
 * to be "used" by being passed to this function (which then ignores them).
 */
static void __attribute__ ((noreturn))
ifp_unsupported_libc_error (const char *function_name, ...)
{
  fprintf (stderr,
           "IFP plugin library error: "
           "libc_proxy: libc function %s() is not supported, sorry\n",
           function_name);
  glk_exit ();

  fprintf (stderr,
           "IFP plugin library error: "
           "libc_proxy: failed to halt interpreter\n");
  raise (SIGABRT);
  _exit (1);
}


/*
 * on_exit()
 * signal()
 * sigaction()
 *
 * There are several Libc functions that it is awkward for IFP to handle
 * if an interpreter calls them.  Generally, these are functions that
 * register callbacks.  If interpreter function addresses are stored in
 * Libc, there are no simple ways to get them out again when the interpreter
 * plugin is unloaded.  These function definitions override such troublesome
 * routines.  If an interpreter plugin calls one of these functions, IFP
 * issues an error and stops the interpreter.  At present, no interpreter
 * is known to call these functions.
 */
int
on_exit (void (*function) (int, void *), void *argument)
{
  ifp_unsupported_libc_error ("on_exit", function, argument);
}

typedef void (*ifp_sighandler_t) (int);

ifp_sighandler_t
signal (int signum, void (*sighandler) (int))
{
  ifp_unsupported_libc_error ("signal", signum, sighandler);
}

int
sigaction (int signum, const struct sigaction *act, struct sigaction *oldact)
{
  ifp_unsupported_libc_error ("sigaction", signum, act, oldact);
}
