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

DIRECTORY = tads2_build
HEADER = tads2
DSO = tads-2.5.10.so
ARCHIVE = $(INTERPRETERS)/tads_src_2510.zip

default: all

$(HEADER)_plugin.o: $(HEADER).hdr
	$(IFPHDR) $(HEADER).hdr $(HEADER)_plugin.c
	$(CC) $(CFLAGS) -c $(HEADER)_plugin.c

$(DIRECTORY): $(ARCHIVE)
	mkdir -p $(DIRECTORY)
	cd $(DIRECTORY); \
		unzip -aa ../$(ARCHIVE); mv tads2/* .; rmdir tads2; \
		patch -i ../$(PATCHES)/tads2.patch -Np1

$(PLUGINS)/$(DSO): $(HEADER)_plugin.o $(DIRECTORY)
	mkdir -p $(PLUGINS)
	cd $(DIRECTORY)/glk; \
		$(MAKE) OSFLAGS="-DLINUX -fPIC -D__NO_STRING_INLINES" $(DSO)
	cp $(DIRECTORY)/glk/$(DSO) $(PLUGINS)
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
