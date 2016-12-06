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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

include Makefile.inc

SHELL = /bin/sh
SUBDIRS = src third_party

DIST = ifp-1.4

RESOURCES_DIST = $(DIST)-resources
CORE_DIST = $(DIST)-core

# Top level makefile - descends into subdirectories and executes the make in
# these one at a time.

# Macro to support descending into selected subdirectories.
DESCEND	= for subdir in $(SUBDIRS); do				\
		( cd $$subdir && $(MAKE) $@ );			\
	  done

# Targets that do nothing other than descend.
all install install-strip uninstall clean TAGS info dvi check:
	$(DESCEND)

# Targets that do just a little more than this.
distclean mostlyclean:
	$(DESCEND)
	rm -f $(CORE_DIST).tar $(CORE_DIST).tar.gz $(CORE_DIST).tgz
	rm -f $(RESOURCES_DIST).tar \
		$(RESOURCES_DIST).tar.gz $(RESOURCES_DIST).tgz
	rm -f Makefile.inc
	rm -f config.status config.cache config.log
	rm -rf autom4te.cache

maintainer-clean: distclean
	rm -f configure aclocal.m4 configure.scan autoscan.log

dist:	distclean
	rm -f $(CORE_DIST).tar $(CORE_DIST).tar.gz $(CORE_DIST).tgz
	rm -f $(RESOURCES_DIST).tar \
		$(RESOURCES_DIST).tar.gz $(RESOURCES_DIST).tgz
	rm -f $(DIST); ln -s . $(DIST)
	tar zcvf $(DIST).tgz \
		--exclude $(DIST)/$(DIST) \
		$(DIST)/*
	tar zcvf $(CORE_DIST).tgz \
		--exclude $(DIST)/$(DIST) \
		--exclude $(DIST).tgz \
		--exclude $(DIST)/third_party/repository \
		$(DIST)/*
	tar zcvf $(RESOURCES_DIST).tgz \
		--exclude $(DIST)/$(DIST) \
		--exclude $(DIST).tgz \
		$(DIST)/third_party/repository
	rm $(DIST)
