# vi: set ts=8:
#
# Copyright (C) 2001-2007  Simon Baldwin (simon_baldwin@yahoo.com)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307
# USA.
#

include ../../Makefile.inc
SHELL = /bin/bash

# Portability options.
CC           = $(AC_CC)
IFP_DEBUG    =
IFP_OPTIMIZE = -O2 -D__NO_STRING_INLINES
CFLAGS       = -std=gnu99 -fPIC -I. -Wall -W \
               -Wshadow -Wpointer-arith -Wstrict-prototypes                   \
               -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls  \
               -Wwrite-strings -Wundef -Wbad-function-cast -Wnested-externs   \
               $(IFP_OPTIMIZE) $(IFP_DEBUG)
LDFLAGS      = $(IFP_DEBUG)
IFP_LDFLAGS  = -Wl,-E,-version-script,ifp_versions

AWK    = $(AC_AWK)
RANLIB = $(AC_RANLIB)
GZIP   = $(AC_GZIP)
LD     = $(AC_LD)
AR     = $(AC_AR)
RM     = $(AC_RM)

# Definitions of the libraries built.
IFP_LIBRARY   = libifp.a
IFPPI_LIBRARY = libifppi.a

# Definition of documentation.
MAN_PAGES = ifplib.3 ifppilib.3

# Definition of the utility plugins built
UNARCHIVE_PLUGIN  = unarchive-0.0.5.so
UNCOMPRESS_PLUGIN = uncompress-0.0.5.so

# Definition of the standard player.
IFPE        = ifpe

# Definition of the Legion experimental player.
LEGION = legion

# List of objects for the libraries, the main players, and the plugins.
IFP_OBJECTS        = ifp_utils.o ifp_tracer.o ifp_plugin.o ifp_loader.o	   \
                     ifp_manager.o ifp_header.o ifp_recognizer.o ifp_url.o \
                     ifp_cache.o ifp_http.o ifp_ftp.o ifp_pref.o	   \
                     glk_loader.o libc_handler.o ifp_chain.o ifp_blorb.o   \
                     ifp_glkstream.o mem_intercept.o file_intercept.o	   \
                     ifp_finalizer.o ifp_config.o ifp_main.o
IFPPI_OBJECTS      = glk_proxy.o libc_proxy.o force_link.o finalizer.o
UNARCHIVE_OBJECTS  = unarchive_plugin.o
UNCOMPRESS_OBJECTS = uncompress_plugin.o
DEMO_OBJECTS       = ifp_e.o
LEGION_OBJECTS     = legion.o

# Default target is the libraries and doc, utility plugins, the standard,
# player, and Legion.
all: $(IFP_LIBRARY) $(IFPPI_LIBRARY) $(MAN_PAGES)			\
	$(UNARCHIVE_PLUGIN) $(UNCOMPRESS_PLUGIN) $(IFPE) $(LEGION)

# Simple dependencies - all include ifp.h and ifp_internal.h.
$(IFPPI_OBJECTS) $(IFP_OBJECTS): ifp.h ifp_internal.h
$(UNARCHIVE_OBJECTS) $(UNCOMPRESS_OBJECTS): ifp.h ifp_internal.h

# More simple dependencies.
$(LEGION_OBJECTS) $(DEMO_OBJECTS): ifp.h $(IFP_LIBRARY)

# Build the library for use by plugin clients.
$(IFP_LIBRARY): $(IFP_OBJECTS)
	$(RM) -rf $(IFP_LIBRARY)
	$(AR) -cr $(IFP_LIBRARY) $(IFP_OBJECTS)
	$(RANLIB) $(IFP_LIBRARY)

# Build the library for use in plugins.
$(IFPPI_LIBRARY): $(IFPPI_OBJECTS)
	$(RM) -rf $(IFPPI_LIBRARY)
	$(AR) -cr $(IFPPI_LIBRARY) $(IFPPI_OBJECTS)
	$(RANLIB) $(IFPPI_LIBRARY)

# Build the utility plugins.
$(UNARCHIVE_PLUGIN): $(IFPPI_LIBRARY) $(IFP_LIBRARY) $(UNARCHIVE_OBJECTS)
	$(LD) $(IFP_DEBUG) -u ifpi_force_link -shared -Bsymbolic	\
		-o $@ $(UNARCHIVE_OBJECTS) -L.				\
		 $(IFPPI_LIBRARY) $(IFP_LIBRARY) -ldl -lc

$(UNCOMPRESS_PLUGIN): $(IFPPI_LIBRARY) $(IFP_LIBRARY) $(UNCOMPRESS_OBJECTS)
	$(LD) $(IFP_DEBUG) -u ifpi_force_link -shared -Bsymbolic	\
		-o $@ $(UNCOMPRESS_OBJECTS) -L.				\
		 $(IFPPI_LIBRARY) $(IFP_LIBRARY) -ldl -lc

# Build the linker version-script file.
ifp_versions:
	echo "{ global: glk_main; glkunix_startup_code; glkunix_arguments; \
	     local: *; };" >ifp_versions

# Build the standard player.
$(IFPE): $(IFP_LIBRARY) $(DEMO_OBJECTS) ifp_versions
	$(CC) $(LDFLAGS) $(IFP_LDFLAGS) -o $@ $(DEMO_OBJECTS) -ldl -L. -lifp

# Build the Legion experimental player.
$(LEGION): $(LEGION_OBJECTS) ifp_versions
	$(CC) $(LDFLAGS) $(IFP_LDFLAGS) -o $@ $(LEGION_OBJECTS) -ldl -L. -lifp

# Build the documentation.
ifplib.3: ifplib.3.m4
	for file in $$(echo $(IFP_OBJECTS) | sed -e 's;\.o;\.c;g;');	\
	do $(AWK) -f libdoc.awk $$file;					\
	done | $(AWK) -f libfuncs.awk >functions
	$(AWK) -f include.awk <ifplib.3.m4 >ifplib.3
	$(RM) -f functions

ifppilib.3: ifppilib.3.m4
	for file in $$(echo $(IFPPI_OBJECTS) | sed -e 's;\.o;\.c;g;');	\
	do $(AWK) -f libdoc.awk $$file;					\
	done | $(AWK) -f libfuncs.awk >functions
	$(AWK) -f include.awk <ifppilib.3.m4 >ifppilib.3
	$(RM) -f functions

# Cleanup targets.
clean:
	$(RM) -f $(IFP_LIBRARY) $(IFPPI_LIBRARY)
	$(RM) -f $(IFPE) $(LEGION)
	$(RM) -f $(MAN_PAGES)
	$(RM) -f ifp_versions functions *.so *.o core core.* gmon.out

distclean mostlyclean: clean

maintainer-clean: distclean

# Installation targets.
install:
	$(INSTALL) -d $(libdir)/ifp $(includedir) $(bindir) $(mandir)/man3
	$(INSTALL) -d $(mandir)/man1 $(sysconfdir)
	$(INSTALL_DATA) ifp.h $(includedir)
	$(INSTALL_DATA) ifp_internal.h $(includedir)
	$(INSTALL_DATA) glk.h $(includedir)
	$(INSTALL_DATA) glkstart.h $(includedir)
	$(INSTALL_PROGRAM) libifp.a $(libdir)
	$(INSTALL_PROGRAM) libifppi.a $(libdir)
	$(INSTALL_PROGRAM) ifpe $(bindir)
	$(INSTALL_PROGRAM) legion $(bindir)
	$(INSTALL_PROGRAM) ifphdr $(bindir)
	$(INSTALL_PROGRAM) unarchive-0.0.5.so $(libdir)/ifp
	$(INSTALL_PROGRAM) uncompress-0.0.5.so $(libdir)/ifp
	-$(INSTALL_DATA) ifplib.3 $(mandir)/man3
	-$(GZIP) -f -9 $(mandir)/man3/ifplib.3
	-$(INSTALL_DATA) ifppilib.3 $(mandir)/man3
	-$(GZIP) -f -9 $(mandir)/man3/ifppilib.3
	-$(INSTALL_DATA) ifpe.1 $(mandir)/man1
	-$(GZIP) -f -9 $(mandir)/man1/ifpe.1
	-$(INSTALL_DATA) legion.1 $(mandir)/man1
	-$(GZIP) -f -9 $(mandir)/man1/legion.1
	-$(INSTALL_DATA) ifphdr.1 $(mandir)/man1
	-$(GZIP) -f -9 $(mandir)/man1/ifphdr.1
	$(INSTALL_DATA) ifprc $(sysconfdir)

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall:
	$(RM) -f $(includedir)/ifp.h $(includedir)/ifp_internal.h
	$(RM) -f $(includedir)/glk.h $(includedir)/glkstart.h
	$(RM) -f $(libdir)/libifp.a $(libdir)/libifppi.a
	$(RM) -f $(bindir)/ifpe $(bindir)/legion $(bindir)/ifphdr
	$(RM) -f $(libdir)/ifp/unarchive-0.0.5.so
	$(RM) -f $(libdir)/ifp/uncompress-0.0.5.so
	$(RM) -f $(mandir)/man3/ifplib.3 $(mandir)/man3/ifplib.3.gz
	$(RM) -f $(mandir)/man3/ifppilib.3 $(mandir)/man3/ifppilib.3.gz
	$(RM) -f $(mandir)/man1/ifpe.1 $(mandir)/man1/ifpe.1.gz
	$(RM) -f $(mandir)/man1/legion.1 $(mandir)/man1/legion.1.gz
	$(RM) -f $(mandir)/man1/ifphdr.1 $(mandir)/man1/ifphdr.1.gz
	$(RM) -f $(sysconfdir)/ifprc

# Unimplemented targets
dist:
TAGS:
info:
dvi:
check:
