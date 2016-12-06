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

default: all

$(DERIVATIVES):
	mkdir -p $(DERIVATIVES)
	gzip -9 <$(GAMES)/weather.z5 >$(DERIVATIVES)/weather.z5.gz
	gzip -9 <$(GAMES)/advent.ulx >$(DERIVATIVES)/advent.ulx.gz
	cd $(DERIVATIVES); tar cvf advent.ulx.gz.tar advent.ulx.gz
	gzip <$(GAMES)/advent.blb >$(DERIVATIVES)/advent.blb.gz
	bzip2 <$(GAMES)/toonesia.gam >$(DERIVATIVES)/toonesia.gam.bz2
	gzip <$(GAMES)/spur.hex >$(DERIVATIVES)/spur.hex.gz
	cd $(GAMES); \
		zip -9 ../../$(DERIVATIVES)/bugged.zip bugged.acd bugged.dat
	cd $(GAMES); \
		zip -9 ../../$(DERIVATIVES)/cosmosrv.zip \
			'cosmos.d$$$$' cosmos.da1 cosmos.da2 cosmos.da3 \
			cosmos.da4 cosmos.da5 cosmos.ins cosmos.ttl \
			cosmos.bat runc.exe
	cd $(GAMES); echo "busted.dat" \
			| cpio -ocv >../../$(DERIVATIVES)/busted.cpio
	bzip2 <$(GAMES)/time.sna >$(DERIVATIVES)/time.sna.bz2
	bzip2 <$(GAMES)/hamper.taf >$(DERIVATIVES)/hamper.taf.bz2
	cd $(GAMES); \
		ar qv ../../$(DERIVATIVES)/EnterTheDark.a EnterTheDark.a3c
	cd $(GAMES); \
		ar qv ../../$(DERIVATIVES)/ericgift.a ericgift.t3
	bzip2 <$(GAMES)/elvis.cas >$(DERIVATIVES)/elvis.cas.bz2

$(SHIPPED_GAMES):
	mkdir -p $(SHIPPED_GAMES)
	cp $(GAMES)/weather.z5 $(SHIPPED_GAMES)
	cp $(GAMES)/advent.ulx $(SHIPPED_GAMES)
	cp $(GAMES)/advent.blb $(SHIPPED_GAMES)
	cp $(GAMES)/toonesia.gam $(SHIPPED_GAMES)
	cp $(GAMES)/spur.hex $(SHIPPED_GAMES)
	cd $(GAMES); \
		zip -9 ../../$(SHIPPED_GAMES)/bugged.zip bugged.acd bugged.dat
	cd $(GAMES); \
		zip -9 ../../$(SHIPPED_GAMES)/cosmos.zip cosmos.d?? cosmos.ins \
		cosmos.ttl
	cp $(GAMES)/busted.dat $(SHIPPED_GAMES)
	cp $(GAMES)/time.sna $(SHIPPED_GAMES)
	cp $(GAMES)/hamper.taf $(SHIPPED_GAMES)
	cp $(GAMES)/EnterTheDark.a3c $(SHIPPED_GAMES)
	cp $(GAMES)/ericgift.t3 $(SHIPPED_GAMES)
	cp $(GAMES)/elvis.cas $(SHIPPED_GAMES)

all: $(DERIVATIVES) $(SHIPPED_GAMES)

clean:
	$(RM) -rf $(DERIVATIVES) $(SHIPPED_GAMES)

distclean: clean

mostlyclean: clean

maintainer-clean: distclean

install:
	$(INSTALL) -d $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/weather.z5 $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/advent.ulx $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/advent.blb $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/toonesia.gam $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/spur.hex $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/bugged.zip $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/cosmos.zip $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/busted.dat $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/time.sna $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/hamper.taf $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/EnterTheDark.a3c $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/ericgift.t3 $(libdir)/games/ifp
	$(INSTALL_DATA) $(SHIPPED_GAMES)/elvis.cas $(libdir)/games/ifp
	$(INSTALL) -d $(libdir)/ifp

uninstall:
	$(RM) -f $(libdir)/games/ifp/weather.z5
	$(RM) -f $(libdir)/games/ifp/advent.ulx
	$(RM) -f $(libdir)/games/ifp/advent.blb
	$(RM) -f $(libdir)/games/ifp/toonesia.gam
	$(RM) -f $(libdir)/games/ifp/spur.hex
	$(RM) -f $(libdir)/games/ifp/bugged.zip
	$(RM) -f $(libdir)/games/ifp/cosmos.zip
	$(RM) -f $(libdir)/games/ifp/busted.dat
	$(RM) -f $(libdir)/games/ifp/time.sna
	$(RM) -f $(libdir)/games/ifp/hamper.taf
	$(RM) -f $(libdir)/games/ifp/EnterTheDark.a3c
	$(RM) -f $(libdir)/games/ifp/ericgift.t3
	$(RM) -f $(libdir)/games/ifp/elvis.cas

install-strip:
TAGS:
info:
dvi:
check:
