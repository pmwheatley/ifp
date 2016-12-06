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

DIRECTORY = frotz_build
HEADER = frotz
DSO = frotz-2.4.3.so
ARCHIVE = $(INTERPRETERS)/gargoyle-frotz-2006-09-17-source.zip

FROTZ_OBJECTS = buffer.o err.o fastmem.o files.o glkmisc.o glkscreen.o \
                input.o main.o math.o object.o process.o quetzal.o \
                random.o redirect.o sound.o stream.o table.o text.o variable.o

FROTZ_CFLAGS = -O2 -fPIC -D__NO_STRING_INLINES -I../../src/ifp

default: all

$(HEADER)_plugin.o: $(HEADER).hdr
	$(IFPHDR) $(HEADER).hdr $(HEADER)_plugin.c
	$(CC) $(CFLAGS) -c $(HEADER)_plugin.c

$(DIRECTORY): $(ARCHIVE)
	mkdir -p $(DIRECTORY)
	cd $(DIRECTORY); \
		unzip ../$(ARCHIVE); \
		mv gargoyle/terps/frotz/* .; \
		rmdir gargoyle/terps/frotz gargoyle/terps gargoyle; \
		patch -i ../$(PATCHES)/frotz_cleanup.patch -Np1

$(PLUGINS)/$(DSO): $(HEADER)_plugin.o $(DIRECTORY)
	mkdir -p $(PLUGINS)
	cd $(DIRECTORY); \
		$(MAKE) CFLAGS='$(FROTZ_CFLAGS)' $(FROTZ_OBJECTS)
	cd $(DIRECTORY); \
		$(CC) -shared -Wl,-u,ifpi_force_link -Wl,-Bsymbolic \
			$(FROTZ_OBJECTS) ../$(HEADER)_plugin.o \
			-L../../src/ifp -lifppi -o $(DSO)
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
