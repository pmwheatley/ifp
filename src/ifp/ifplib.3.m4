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
ifplib \- client-side Interactive Fiction plugin library
.\"
.\"
.\"
.SH SYNOPSIS
.\"
.nf
.B #include <ifp.h>
.PP
.\"
.\"
.\"
.SH DESCRIPTION
.\"
.B ifplib
is the client-side library for the IFP Interactive Fiction plugin scheme.
The library offers functions to load and control plugins containing
Interactive Fiction interpreter engines, and is intended to be linked into
an IFP main program executable.
.PP
Note that in addition to the functions listed below, \fBifplib\fP also acts
as a full Glk library.  That is, it contains symbols to resolve the full
set of Glk functions, and uses dynamic loading and late binding of a real
Glk library to do the actual work.  When linking with this library, do not
also link in a Glk library; the Glk symbols in \fBifplib\fP are weak, so you
may not see any link errors, but nevertheless things will probably not work
as they should.
.PP
.\"
.\"
.\"
.SH FUNCTIONS
The following list describes the functions available to an \fBifplib\fP client
(excluding Glk functions in the interest of brevity):
include(functions)
.PP
.\"
.\"
.\"
.SH NOTES
When linking a main IFP client program, as well as not linking in an additional
Glk library, you also need to ensure that three symbols are exported so that
Glk library plugins can access them.  To do this, link the program with
something like
.IP
.nf
gcc -Wl,-E,-version-script,version_script -o main_program \\
  ...main objects... -ldl -lifp
.fi
.PP
where the version_script file contains
.IP
.nf
{ global: glk_main; glkunix_startup_code; glkunix_arguments; \\
  local: *; };
.fi
.PP
.\"
.\"
.\"
.SH SEE ALSO
.\"
Man page for \fBifppilib\fP(3).
.\"
