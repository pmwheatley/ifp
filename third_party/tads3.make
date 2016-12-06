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

DIRECTORY = tads3_build
HEADER = tads3
DSO = tads-3.0.12.so
ARCHIVE = $(INTERPRETERS)/gargoyle-tads-2006-09-17-source.zip
ARCHIVE_2 = $(INTERPRETERS)/tads_src_2510.zip
ARCHIVE_3 = $(INTERPRETERS)/tads_src_3012.zip

TADS_CFLAGS = -O2 -DLINUX -fPIC -D__NO_STRING_INLINES \
              -I.. -I../tads2 -I../tads3 -I../../../src/ifp \
              -DGLK_ANSI_ONLY -DGARGOYLE -DVMGLOB_STRUCT \
              -DOS_USHORT_DEFINED -DOS_UINT_DEFINED -DOS_ULONG_DEFINED

TADS2_OBJECTS = osifc.o osrestad.o oem.o \
                argize.o bif.o bifgdum.o cmap.o cmd.o dat.o dbgtr.o errmsg.o \
                execmd.o fio.o fioxor.o getstr.o ler.o linfdum.o lst.o mch.o \
                mcm.o mcs.o obj.o oserr.o os0.o out.o output.o ply.o \
                qas.o regex.o run.o runstat.o suprun.o trd.o voc.o vocab.o

TADS3_OBJECTS = vmcrc.o vmmain.o std.o std_dbg.o charmap.o resload.o \
                resldexe.o vminit.o vmini_nd.o vmconsol.o vmconnom.o \
                vmconhmp.o vminitim.o vmcfgmem.o vmobj.o vmundo.o vmtobj.o \
                vmpat.o vmstrcmp.o vmdict.o vmgram.o vmstr.o vmcoll.o \
                vmiter.o vmlst.o vmsort.o vmsortv.o vmbignum.o vmvec.o \
                vmintcls.o vmanonfn.o vmlookup.o vmbytarr.o vmcset.o \
                vmfilobj.o vmstack.o vmerr.o vmerrmsg.o vmpool.o vmpoolim.o \
                vmtype.o vmtypedh.o utf8.o vmglob.o vmrun.o vmfunc.o vmmeta.o \
                vmsa.o vmbiftio.o vmbif.o vmbifl.o vmimage.o vmimg_nd.o \
                vmrunsym.o vmsrcf.o vmfile.o vmbiftad.o vmsave.o vmbift3.o \
                vmbt3_nd.o vmregex.o vmhosttx.o vmhostsi.o vmhash.o vmmcreg.o \
                vmbifreg.o

GARGOYLE_CFLAGS = -O2 -DLINUX -fPIC -D__NO_STRING_INLINES \
                  -I. -Itads2 -Itads3 -I../../src/ifp \
                  -DGLK_ANSI_ONLY -DGARGOYLE -DVMGLOB_STRUCT \
                  -DOS_USHORT_DEFINED -DOS_UINT_DEFINED -DOS_ULONG_DEFINED

GARGOYLE_OBJECTS = t23run.o osansi1.o osansi2.o osansi3.o osglk.o osnoban.o \
                   t2askf.o t2indlg.o t3askf.o t3indlg.o memicmp.o vmuni_cs.o

default: all

$(HEADER)_plugin.o: $(HEADER).hdr
	$(IFPHDR) $(HEADER).hdr $(HEADER)_plugin.c
	$(CC) $(CFLAGS) -c $(HEADER)_plugin.c

$(DIRECTORY): $(ARCHIVE) $(ARCHIVE_2) $(ARCHIVE_3)
	mkdir -p $(DIRECTORY)
	cd $(DIRECTORY); \
		unzip -aa ../$(ARCHIVE); \
		mv gargoyle/tads/* .; \
		rmdir gargoyle/tads gargoyle; \
		patch -i ../$(PATCHES)/tads3_askf.patch -Np1; \
		unzip -aa ../$(ARCHIVE_2); \
		rm tads2/os.h; \
		unzip -aa ../$(ARCHIVE_3); \
		echo "#include <sys/types.h>" >> os.h
	cd $(DIRECTORY); \
		$(MAKE) CXXFLAGS='$(GARGOYLE_CFLAGS)' \
		     CFLAGS='$(GARGOYLE_CFLAGS)' $(GARGOYLE_OBJECTS)

$(DIRECTORY)/tads2: $(DIRECTORY)

$(DIRECTORY)/tads3: $(DIRECTORY)

$(DIRECTORY)/libtads2.a: $(DIRECTORY)/tads2
	cd $(DIRECTORY)/tads2; \
		$(MAKE) CFLAGS='$(TADS_CFLAGS)' $(TADS2_OBJECTS); \
		$(AR) q ../libtads2.a $(TADS2_OBJECTS)

$(DIRECTORY)/libtads3.a: $(DIRECTORY)/tads3
	cd $(DIRECTORY)/tads3; \
		$(MAKE) CXXFLAGS='$(TADS_CFLAGS)' $(TADS3_OBJECTS); \
		$(AR) q ../libtads3.a $(TADS3_OBJECTS)

$(PLUGINS)/$(DSO): $(HEADER)_plugin.o \
		$(DIRECTORY)/libtads2.a $(DIRECTORY)/libtads3.a
	mkdir -p $(PLUGINS)
	cd $(DIRECTORY); \
		$(CXX) -shared -Wl,-u,ifpi_force_link -Wl,-Bsymbolic \
			$(GARGOYLE_OBJECTS) ../$(HEADER)_plugin.o \
			-L. -ltads2 -ltads3 \
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
