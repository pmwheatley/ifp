# vi: set ts=8 shiftwidth=8:
#
# Copyright (C) 2006-2007  Simon Baldwin (simon_baldwin@yahoo.com)
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
# USA
#

include ../../Makefile.inc
SHELL = /bin/bash

# Portability options.
CC           = $(AC_CC)
IFP_DEBUG    =
IFP_OPTIMIZE = -O2 -D__NO_STRING_INLINES
CFLAGS       = -std=gnu99 -fPIC -I. -I../ifp -I/usr/include/libxml2 -Wall -W  \
               -Wshadow -Wpointer-arith -Wstrict-prototypes                   \
               -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls  \
               -Wwrite-strings -Wundef -Wbad-function-cast -Wnested-externs   \
               $(IFP_OPTIMIZE) $(IFP_DEBUG)
LDFLAGS      = $(IFP_DEBUG)

LD     = $(AC_LD)
RM     = $(AC_RM)
IFPHDR = ../ifp/ifphdr

# Plugin.
GAMEBOX_PLUGIN = gamebox-0.0.4.so

# Default target is the Gamebox plugin.
all: $(GAMEBOX_PLUGIN)

# List of objects for the plugin.
GAMEBOX_OBJECTS	= memory.o vector.o gameset.o message.o display.o	\
                  xmlparser.o urlhandler.o utfhandler.o inifile.o	\
                  iniparser.o gamegroup.o interpreters.o gamepage.o	\
                  terppage.o aboutpage.o hash.o gamebox.o gamebox_plugin.o

$(GAMEBOX_OBJECTS): protos.h

# Build the Gamebox plugin.
gamebox_plugin.c: gamebox.hdr
	$(RM) -f gamebox_plugin.c
	$(IFPHDR) gamebox.hdr gamebox_plugin.c

$(GAMEBOX_PLUGIN): $(GAMEBOX_OBJECTS)
	$(LD) $(IFP_DEBUG) -u ifpi_force_link -static -shared -Bsymbolic\
		-o $@ $(GAMEBOX_OBJECTS) -Bstatic -lxml2 -lz -lm	\
		-Bdynamic -L../ifp -lifppi -lifp -ldl -lc --wrap sigaction

# Cleanup targets.
clean:
	$(RM) -f gamebox_plugin.c
	$(RM) -f *.so *.o core core.* gmon.out

distclean mostlyclean: clean

maintainer-clean: distclean

# Installation targets.
install:
	$(INSTALL) -d $(libdir)/ifp
	$(INSTALL_PROGRAM) $(GAMEBOX_PLUGIN) $(libdir)/ifp
	$(INSTALL_PROGRAM) catalog.py $(libdir)/ifp
	$(INSTALL_DATA) catalog.ifiction $(libdir)/ifp
	$(INSTALL_DATA) catalog.ifmes $(libdir)/ifp
	$(INSTALL_DATA) catalog.ini $(libdir)/ifp

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall:
	$(RM) -f $(libdir)/ifp/$(GAMEBOX_PLUGIN)
	$(RM) -f $(libdir)/ifp/catalog.py
	$(RM) -f $(libdir)/ifp/catalog.ifiction
	$(RM) -f $(libdir)/ifp/catalog.ifmes
	$(RM) -f $(libdir)/ifp/catalog.ini

# Unimplemented targets
dist:
TAGS:
info:
dvi:
check:
