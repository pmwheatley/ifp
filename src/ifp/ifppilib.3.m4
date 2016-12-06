.\" vim: set syntax=nroff:
.\"
.\" Copyright (C) 2001-2007  Simon Baldwin (simon_baldwin@yahoo.com)
.\" 
.\" This program is free software; you can redistribute it and/or
.\" modify it under the terms of the GNU General Public License
.\" as published by the Free Software Foundation; either version 2
.\" of the License, or (at your option) any later version.
.\" 
.\" This program is distributed in the hope that it will be useful,
.\" but WITHOUT ANY WARRANTY; without even the implied warranty of
.\" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
.\" GNU General Public License for more details.
.\" 
.\" You should have received a copy of the GNU General Public License
.\" along with this program; if not, write to the Free Software
.\" Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307
.\" USA
.\"
.\"
.TH IFPLIB 3 "Interactive Fiction Plugins" "IFP" \" -*- nroff -*-
.SH NAME
.\"
ifppilib \- plugin-side Interactive Fiction plugin library
.\"
.\"
.\"
.SH SYNOPSIS
.\"
.nf
.B #include <ifp.h>
.B #include <ifp_internal.h>
.PP
.\"
.\"
.\"
.SH DESCRIPTION
.\"
.B ifppilib
is the plugin-side library for the IFP Interactive Fiction plugin scheme.
The library contains functions required by the IFP loader (client-side)
library in order to load and control plugins containing Interactive Fiction
interpreter engines.
.PP
Note that \fBifppilib\fP acts as a full Glk library.  That is, it contains
symbols to resolve the full set of Glk functions, and forwards Glk function
calls to its attached \fBifplib\fP when active.  The idea is to
link \fBifppilib\fP to an interpreter as if it were a real Glk, and let the
client \fBifplib\fP supply the real Glk functions when it attaches to the
plugin.
.PP
.\"
.\"
.\"
.SH FUNCTIONS
The following list describes the functions available in \fBifppilib\fP:
include(functions)
.PP
.\"
.\"
.\"
.SH NOTES
When linking an IFP plugin, as well as not linking in an additional Glk
library, you also need to ensure that the IFP plugin header and other required
IFP functions appear in the shared object.  To do this, build and link
with \fI-u ifpi_force_link\fP as a linker option, and with the plugin's header
module included on the link line, something like
.IP
.nf
ifphdr myplugin.hdr myplugin_header.c
gcc -c myplugin_header.c
ld -u ifpi_force_link -shared -Bsymbolic -o myplugin-0.0.0.so \\
  ...usual interpreter objects... -lifppi myplugin_header.o -lc
.fi
.PP
.\"
.\"
.\"
.SH SEE ALSO
.\"
Man page for \fBifplib\fP(3).
.\"
