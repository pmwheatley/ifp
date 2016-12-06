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

DIRECTORY = xglk
LIBRARY = libxglk.a
DSO = libxglk.so.0.4.11
ARCHIVE = $(GLK_SOURCES)/xglk-0411.tar.Z

default: all

$(DIRECTORY)/$(LIBRARY): $(DIRECTORY)
	cd $(DIRECTORY); $(MAKE) SYSTEMFLAGS="-O2 -fPIC" all

$(DIRECTORY): $(ARCHIVE)
	gunzip -c <$(ARCHIVE) | tar xvf -
	cd $(DIRECTORY); \
		patch -i ../$(PATCHES)/xglk.patch -Np1; \
		patch -i ../$(PATCHES)/xglk_8bit.patch -Np1; \
		patch -i ../$(PATCHES)/xglk_bs.patch -Np1

$(PLUGINS)/$(DSO): $(DIRECTORY)/$(LIBRARY)
	mkdir -p $(PLUGINS)
	$(CC) -shared -o $(PLUGINS)/$(DSO) \
		-Wl,-soname,$$(echo $(DSO) | sed "s;\.[0-9]*\.[0-9]*$$;;") \
		$$($(AR) t $(DIRECTORY)/$(LIBRARY) | sed "s;^;$(DIRECTORY)/;") \
	-lpng -lz -lm -ljpeg -ldl -L/usr/X11R6/lib -lX11

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
