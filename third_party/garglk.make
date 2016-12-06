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

DIRECTORY = garglk
LIBRARY = libgarglk.a
DSO = libgarglk.so.6.9.17
ARCHIVE = $(GLK_SOURCES)/gargoyle-garglk-2006-09-17-source.zip

GARGLK_OBJECTS = gi_blorb.o gi_dispa.o nohyper.o cggestal.o cgblorb.o \
                 cgfref.o cgmisc.o cgstyle.o cgstream.o window.o winblank.o \
                 winpair.o wingrid.o wintext.o wingfx.o event.o draw.o \
                 config.o imgload.o imgscale.o fontdata.o \
                 sndnull.o sysgtk.o main.o

GARGLK_CFLAGS = -O2 -fPIC -I/usr/include/freetype2 \
                `pkg-config --cflags gtk+` `pkg-config --cflags glib`

# For example:
#  GARGLK_CFLAGS = -O2 -fPIC -I/usr/include/freetype2 \
#                  -I/usr/include/gtk-1.2 \
#                  -I/usr/X11R6/include -I/usr/include/glib-1.2 \
#                  -I/usr/lib/glib/include

default: all

$(DIRECTORY)/$(LIBRARY): $(DIRECTORY)
	cd $(DIRECTORY); \
		$(MAKE) CFLAGS='$(GARGLK_CFLAGS)' $(GARGLK_OBJECTS)
	cd $(DIRECTORY); \
		$(AR) q $(LIBRARY) $(GARGLK_OBJECTS)

$(DIRECTORY): $(ARCHIVE)
	mkdir -p $(DIRECTORY)
	cd $(DIRECTORY); \
		unzip ../$(ARCHIVE); \
		mv gargoyle/garglk/* .; \
		rmdir gargoyle/garglk gargoyle; \
		patch -i ../$(PATCHES)/garglk_title.patch -Np1; \
		patch -i ../$(PATCHES)/garglk_ini.patch -Np1 

$(PLUGINS)/$(DSO): $(DIRECTORY)/$(LIBRARY)
	mkdir -p $(PLUGINS)
	$(CC) -shared -o $(PLUGINS)/$(DSO) \
		-Wl,-soname,$$(echo $(DSO) | sed "s;\.[0-9]*\.[0-9]*$$;;") \
		$$($(AR) t $(DIRECTORY)/$(LIBRARY) | sed "s;^;$(DIRECTORY)/;") \
	-L/usr/X11R6/lib \
	-lfreetype -lgtk -lgdk -lgmodule -lXi -lXext -lX11 -lm -lglib \
	-ljpeg -lpng -lz

all: $(PLUGINS)/$(DSO)

clean:
	$(RM) -rf $(DIRECTORY) $(PLUGINS)/$(DSO)

distclean: clean

mostlyclean: clean

maintainer-clean: distclean

install:
	$(INSTALL) -d $(libdir)/ifp $(sysconfdir)
	$(INSTALL_PROGRAM) $(PLUGINS)/$(DSO) $(libdir)/ifp
	$(INSTALL_DATA) $(DIRECTORY)/garglk.ini $(sysconfdir)

uninstall:
	$(RM) -f $(libdir)/ifp/$(DSO)
	$(RM) -f $(sysconfdir)/garglk.ini

install-strip:
TAGS:
info:
dvi:
check:
