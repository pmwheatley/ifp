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

DIRECTORY = gtkglk
LIBRARY = libgtkglk.a
DSO = libgtkglk.so.0.0.3
ARCHIVE = $(GLK_SOURCES)/gtkglk-0.3.tar.gz

default: all

$(DIRECTORY)/$(DSO): $(DIRECTORY)
	cd $(DIRECTORY); \
		./configure; $(MAKE)

$(DIRECTORY): $(ARCHIVE)
	mkdir -p $(DIRECTORY)
	cd $(DIRECTORY); \
		gunzip -c <../$(ARCHIVE) | tar xvf -; \
		mv gtkglk-0.3/* .; rmdir gtkglk-0.3
	cd $(DIRECTORY); \
		patch -i ../$(PATCHES)/gtkglk_unicode.patch -Np1

$(PLUGINS)/$(DSO): $(DIRECTORY)/$(DSO)
	mkdir -p $(PLUGINS)
	cp $(DIRECTORY)/$(DSO) $(PLUGINS)/$(DSO)

all: $(PLUGINS)/$(DSO)

clean:
	$(RM) -rf $(DIRECTORY) $(PLUGINS)/$(DSO)

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
