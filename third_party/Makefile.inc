# vim: set syntax=make:
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
SHELL = /bin/bash

# Portability.
CC = $(AC_CC)
CXX = $(AC_CXX)
RM = $(AC_RM)

# Subdirectories containing source elements.
REPOSITORY = repository
GLK_SOURCES = $(REPOSITORY)/glk_sources
INTERPRETERS = $(REPOSITORY)/interpreters
GAMES = $(REPOSITORY)/games

# Subdirectories built by other make files.
PATCHES = patches
PLUGINS = plugins
DERIVATIVES = derivatives
SHIPPED_GAMES = distributed

# Miscellaneous common stuff.
IFPHDR = ../src/ifp/ifphdr
CFLAGS = -O2 -fPIC -Wall -W -I../src/ifp
