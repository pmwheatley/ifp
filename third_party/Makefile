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

include ../Makefile.inc
include Makefile.inc

SHELL	= /bin/bash

GLKS = cheapglk glkterm xglk garglk gtkglk
INTERPRETERS = advsys agility alan2 alan3 git glulxe hugo level9 magnetic \
               nitfol scare tads2 tads3 frotz geas
GAMES = games
TARGETS = $(GLKS) $(INTERPRETERS) $(GAMES)

# Master makefile - invokes individual makefiles iteratively.

# The manifest file lists everything that's usually in the repository.  If the
# repository is empty, then this is the core IFP distribution, and lacks
# the third_party sources to build plugins and example games.
#
# To populate the repository, either download the resources distribution
# associated with this core IFP release, or locate the files separately.
UPDATE_MANIFEST = \
	$(RM) MANIFEST; \
	echo -e "MD5 sums of repository directory contents:\n" >MANIFEST; \
	md5sum $(REPOSITORY)/*/* >>MANIFEST

# Macro to support iterating makefiles.
ITERATE	= for target in $(TARGETS); do				\
		$(MAKE) -f $${target}.make $@;			\
	  done

# If there is a repository, most targets do nothing other than update the
# manifest, then iterate.  If no repository, they do nothing.
all install install-strip uninstall clean TAGS info dvi check:
	@if [ -d $(REPOSITORY) ]; then				\
		$(UPDATE_MANIFEST);				\
		$(ITERATE);					\
	else							\
		echo "[ Core IFP source only -- no resources ]";\
	fi

distclean mostlyclean maintainer-clean:
	@if [ -d $(REPOSITORY) ]; then				\
		$(UPDATE_MANIFEST);				\
		$(ITERATE);					\
		$(RM) -rf plugins;				\
	else							\
		echo "[ Core IFP source only -- no resources ]";\
	fi
