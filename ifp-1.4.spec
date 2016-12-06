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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307
# USA
#

# Normal descriptive preamble.
Summary:	IFP Interactive Fiction Plugin scheme
Name:		ifp
Version:	1.4
Release:	1
Group:		Games
Copyright:	GPL
Packager:	Simon Baldwin <simon_baldwin@yahoo.com>
URL:		http://www.geocities.com/simon_baldwin/
Source:		ftp://www.ifarchive.org/if-archive/programming/ifp/ifp-1.4.tgz
BuildRoot:	/tmp/ifp-1.4

# Suppress strip and debug package.
%define debug_package %{nil}
%define __spec_install_post /usr/lib/rpm/brp-strip

# More preamble.
%description
IFP is a plugin-enabled Interactive Fiction game player.  It comes with
plugins for Z-Machine, TADS, Alan, Hugo, Glulx, AGT, Advsys, Level9,
Magnetic Scrolls, Adrift, and Quest game formats, and can also directly
handle any of these formats as compressed files, and as URL references to
remote sites such as www.ifarchive.org.

# Straightforward preparation for building.
%prep
%setup

# To build, first configure, then make.
%build
./configure
make IFP_DEBUG=""

# Tweaked install.  Here we set "prefix" to the build root, suffixed with
# "/usr".  So, unlike our natural locations under "/usr/local", we install
# the RPM packaged version in "/usr", leaving "/usr/local" free for, well,
# the usual "/usr/local" stuff.
%install
make prefix="$RPM_BUILD_ROOT/usr" sysconfdir="$RPM_BUILD_ROOT/etc" install

# Clean up our build root.
%clean
rm -rf $RPM_BUILD_ROOT

# List of packaged files.
%files
%{_bindir}/ifpe
%{_bindir}/legion
%{_bindir}/ifphdr

%{_sysconfdir}/ifprc
%{_sysconfdir}/garglk.ini

%{_includedir}/glk.h
%{_includedir}/glkstart.h

%{_includedir}/ifp.h
%{_includedir}/ifp_internal.h

%{_libdir}/libifp.a
%{_libdir}/libifppi.a

%{_libdir}/ifp/advsys-1.2.so
%{_libdir}/ifp/agility-1.1.1.so
%{_libdir}/ifp/alan-2.8.6.so
%{_libdir}/ifp/alan-3.0.5.so
%{_libdir}/ifp/frotz-2.4.3.so
%{_libdir}/ifp/geas-0.4.so
%{_libdir}/ifp/git-1.1.3.so
%{_libdir}/ifp/glulxe-0.4.2.so
%{_libdir}/ifp/hugo-3.1.so
%{_libdir}/ifp/level9-4.1.so
%{_libdir}/ifp/magnetic-2.2.so
%{_libdir}/ifp/nitfol-0.5.so
%{_libdir}/ifp/scare-1.3.7.so
%{_libdir}/ifp/tads-2.5.10.so
%{_libdir}/ifp/tads-3.0.12.so

%{_libdir}/ifp/unarchive-0.0.5.so
%{_libdir}/ifp/uncompress-0.0.5.so

%{_libdir}/ifp/libcheapglk.so.0.9.0
%{_libdir}/ifp/libglkterm.so.0.7.8
%{_libdir}/ifp/libxglk.so.0.4.11

%{_libdir}/ifp/libgarglk.so.6.9.17
%{_libdir}/ifp/libgtkglk.so.0.0.3

%{_libdir}/ifp/gamebox-0.0.4.so
%{_libdir}/ifp/catalog.ifiction
%{_libdir}/ifp/catalog.ifmes
%{_libdir}/ifp/catalog.ini
%{_libdir}/ifp/catalog.py

%{_prefix}/man/man1/ifpe.1.*
%{_prefix}/man/man1/legion.1.*
%{_prefix}/man/man1/ifphdr.1.*

%{_prefix}/man/man3/ifplib.3.*
%{_prefix}/man/man3/ifppilib.3.*

%{_libdir}/games/ifp/advent.blb
%{_libdir}/games/ifp/advent.ulx
%{_libdir}/games/ifp/bugged.zip
%{_libdir}/games/ifp/busted.dat
%{_libdir}/games/ifp/cosmos.zip
%{_libdir}/games/ifp/elvis.cas
%{_libdir}/games/ifp/EnterTheDark.a3c
%{_libdir}/games/ifp/ericgift.t3
%{_libdir}/games/ifp/hamper.taf
%{_libdir}/games/ifp/spur.hex
%{_libdir}/games/ifp/time.sna
%{_libdir}/games/ifp/toonesia.gam
%{_libdir}/games/ifp/weather.z5

%doc AUTHORS
%doc ChangeLog
%doc COPYING
%doc INSTALLING
%doc INTERNALS
%doc README
%doc RUNNING
%doc USING
%doc GAMEBOX
%doc Xglk.defs
