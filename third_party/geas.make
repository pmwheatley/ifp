# vim: set syntax=make:
# vi: set ts=8 shiftwidth=8:
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
# USA
#

include Makefile.inc

DIRECTORY = geas_build
HEADER = geas
DSO = geas-0.4.so
ARCHIVE = $(INTERPRETERS)/geas-src-0.4.tgz

COMPILE_FLAGS = -I../geas-core -I../../../src/ifp -O2 -fPIC \
                -D__NO_STRING_INLINES

default: all

$(HEADER)_plugin.o: $(HEADER).hdr
	$(IFPHDR) $(HEADER).hdr $(HEADER)_plugin.c
	$(CC) $(CFLAGS) -c $(HEADER)_plugin.c

$(DIRECTORY): $(ARCHIVE)
	mkdir -p $(DIRECTORY)
	cd $(DIRECTORY); gunzip -c <../$(ARCHIVE) | tar xvf -
	cd $(DIRECTORY); \
		patch -i ../$(PATCHES)/geas_cheapglk.patch -Np1; \
		patch -i ../$(PATCHES)/geas.patch -Np1

$(PLUGINS)/$(DSO): $(HEADER)_plugin.o $(DIRECTORY)
	mkdir -p $(PLUGINS)
	cd $(DIRECTORY); \
		$(MAKE) \
		CFLAGS="$(COMPILE_FLAGS)" CXXFLAGS="$(COMPILE_FLAGS)" $(DSO)
	cp $(DIRECTORY)/$(DSO) $(PLUGINS)
	strip -g $(PLUGINS)/$(DSO)

all: $(PLUGINS)/$(DSO)

clean:
	$(RM) -rf $(DIRECTORY) $(PLUGINS)/$(DSO)
	$(RM) -f $(HEADER)_plugin.c $(HEADER)_plugin.o

distclean: clean

mostlyclean: clean

maintainer-clean: distclean

install:
	$(INSTALL) -d $(libdir)/ifp
	$(INSTALL_PROGRAM) $(PLUGINS)/$(DSO) $(libdir)/ifp

uninstall:
	$(RM) -f $(libdir)/ifp/$(DSO)

install-strip:
TAGS:
info:
dvi:
check:
