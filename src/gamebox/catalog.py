#!/usr/bin/python2.2
#
# Copyright (C) 2006-2007  Simon Baldwin (simon_baldwin@yahoo.com)
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

# Simple script to catalog IF games in IFMES or iFiction format for IFP
# Gamebox.  Input arguments are a list of files to catalog.  File paths are
# canonicalized, and simple file:// URIs created for them, and a basic game
# name is generated.  For long lists of files, the script can read from stdin.
# Output is in either iFiction XML, IFMES RDF+XML, or IFMES INI format.
#
# Example usages:
#
#  catalog.py /usr/lib/games/ifp/*
#  catalog.py -b -v -o /tmp/ifp_catalog.ifiction /usr/lib/games/ifp/*
#  find /path/to/games -print | catalog.py -i -f -v -o /tmp/all_catalog.ifmes

import sys
import getopt
import os.path


# Create a simple title from a path to a game.
def get_title (path):
  root, ext = os.path.splitext (path)
  return os.path.basename (root).replace ('_', ' ').title ()


# Format-specific generators.
def generate_ifmes_xml (file, paths, is_verbose):
  file.write ('''\
<?xml version="1.0" encoding="UTF-8"?>
<!--
    Autogenerated by catalog.py

    You can edit or augment this file as required to flesh it out into a
    more complete IFMES catalog definition.
    
    This catalog conforms to IFMES version 1.1 working draft "2 September
    2005".
    -->

<RDF:RDF xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
         xmlns:IF="http://purl.org/int-fiction/metadata/1.1/"
         xmlns:DC="http://purl.org/dc/elements/1.1/"
         xmlns:BAF="http://wurb.com/if/"
         xmlns:IFM="http://www.logicalshift.org.uk/IF/metadata/">

<!--
    Template entry.  Only RDF:about is mandatory for Gamebox.  'IFM:story'
    may be used in place of 'BAF:game'.  If you need multiple values for an
    entry (author, say), use multiple IF: elements instead of attributes.

    <BAF:game RDF:about=""              *mandatory*
                     IF:title=""        recommended
                     IF:byline=""       or
                     IF:author=""       or
                     IF:publisher=""
                     IF:desc=""
                     IF:genre=""
                     IF:version=""
                     IF:releaseDate=""  in format "YYYY[-MM[-DD]]"
                     IF:length="">      in format "h:mm"
    </BAF:game>
    -->
''')
  for path in paths:
    file.write ('  <BAF:game RDF:about="file://%s"\n'
                '      IF:title="%s">\n'
                '  </BAF:game>\n\n' % (path, get_title (path)))
    if is_verbose:
      print >>sys.stderr, path
  file.write ('</RDF:RDF>\n')


def generate_ifmes_ini (file, paths, is_verbose):
  file.write ('''\
;GAMEBOX_0.4
;
; Autogenerated by catalog.py
;
; You can edit or augment this file as required to flesh it out into a
; more complete IFMES catalog definition.
;
; This catalog conforms to IFMES version 1.1 working draft "2 September
; 2005".


; Template entry.  In addition to standard IFMES fields, Gamebox uses
; any 'about' property as the full path to the game.  If not given,
; Gamebox will take this from the section name.
;
; [game_name]
; about=full_path_to_game   mandatory unless in the section name
; title=game_title          recommended
; byline=                   or
; author=                   or
; publisher=
; desc=
; genre=
; version=
; releaseDate=              in format YYYY[-MM[-DD]]
; length=                   in format h:mm

''')
  for path in paths:
    file.write ('[%s]\n'
                'about=file://%s\n'
                'title=%s\n\n' % (path, path, get_title (path)))
    if is_verbose:
      print >>sys.stderr, path


def generate_ifiction_xml (file, paths, is_verbose):
  file.write ('''\
<?xml version="1.0" encoding="UTF-8"?>
<!--
    Autogenerated by catalog.py

    You can edit or augment this file as required to flesh it out into a
    more complete iFiction catalog definition.
    
    This catalog conforms to the 'sparse' format described in "The Treaty of
    Babel, A community standard for IF bibliography", Draft 7, 13 April 2006
    ('sparse' because it lacks the normally mandatory <identification> element,
    unused by Gamebox.
    -->

<!--
    Note: the Babel download URL is expected to be "http://babel.ifarchive.org
    /download", but can be reset by setting GAMEBOX_BABEL_URL.
    -->

<ifindex version="1.0" xmlns="http://babel.ifarchive.org/protocol/iFiction/">

<!--
    Template entry.  <identification><ifid> is mandatory for the treaty, and
    Gamebox generates a game's location from that, prefixed with the Babel
    download URL.  Because this may not be what you want (for example, games
    held locally on disk), Gamebox implements <annotation><gamebox><about>,
    letting you to specify the game location.

    <story>
      <identification>
        <ifid>...</ifid>                *mandatory* (for correctness)
        <format>...</format>            *mandatory* (for correctness)
        <bafn>...</bafn>
      </identification>
      <bibliographic>
        <title>...</title>              *mandatory* (for correctness)
        <author>...</author>            *mandatory* (for correctness)
        <firstpublished>...</firstpublished>
        <genre>...</genre>
        <description>
          ...
        </description>
        <group>...</group>
      </bibliographic>
      <annotation>
        <gamebox>
          <about>...</about>            overrides url+IFID game location
          <group>...</group>
        </gamebox>
      </annotation>
      <zcode>
        <release>...</release>          recommended
      </zcode>
      <tads[2|3]>
        <version>...</version>          recommended
        <releasedate>...</releasedate>  recommended, overrides <firstpublished>
      </tads[2|3]>
    </story>
-->
''')
  for path in paths:
    file.write ('  <story>\n'
                '    <bibliographic>\n'
                '      <title>%s</title>\n'
                '      <author>Anonymous</author>\n'
                '    </bibliographic>\n'
                '    <annotation>\n'
                '      <gamebox>\n'
                '        <about>file://%s</about>\n'
                '      </gamebox>\n'
                '    </annotation>\n'
                '  </story>\n\n' % (get_title (path), path))
    if is_verbose:
      print >>sys.stderr, path
  file.write ('</ifindex>\n')


# Simple filename extension filter to guess if a file contains a game.
def is_gamelike (path):
  root, ext = os.path.splitext (path)
  return ext in ('.z1','.z2', '.z3', '.z4', '.z5', '.z6', '.z7', '.z8',
                 '.z9', '.dat', '.acd', '.gam', '.taf', '.blb', '.ulx',
                 '.zip', '.hex', '.sna', '.mag', '.lev', '.d$$', '.xml')


def usage ():
  print >>sys.stderr, ('Usage: %s [options] [files]\n\n'
  '  -o outfile  Specify the catalog output file (default: stdout)\n'
  '  -a          Write out IFMES INI format rather than IFMES RDF+XML\n'
  '  -b          Write out iFiction XML format rather than IFMES RDF+XML\n'
  '  -i          Read files to catalog from stdin (default: "files" args)\n'
  '  -f          Filter for likely IF games only (.z[1-9], .dat, .acd, .gam,\n'
  '              .taf, .blb, .ulx, .zip, .hex, .sna, .mag, .lev, .d$$, .xml)\n'
  '  -v          List each file cataloged to stderr\n'
  '  -h          Print this message\n' % sys.argv[0])
  sys.exit (0)


def fatal (details):
  print >>sys.stderr, '%s: %s' % (sys.argv[0], details)
  print >>sys.stderr, "Try '%s -h' for more information.\n" % sys.argv[0]
  sys.exit (1)


# Main program entry point.
def main ():
  outfile = sys.stdout
  is_iniformat = is_ifictionformat = False
  use_infiles = is_filtered = is_verbose = False

  # Handle command line options.
  try:
    options, args = getopt.getopt (sys.argv[1:], 'o:abifvh')
  except getopt.GetoptError, details:
    fatal (details)

  for option, value in options:
    if option == '-o':
      try:
        outfile = open (value, 'w')
      except IOError, details:
        fatal (details)

    elif option == '-a':
      is_iniformat = True
    elif option == '-b':
      is_ifictionformat = True
    elif option == '-i':
      use_infiles = True
    elif option == '-f':
      is_filtered = True
    elif option == '-v':
      is_verbose = True
    elif option == '-h':
      usage ()

  # Complain about any invalid combinations.
  if is_iniformat and is_ifictionformat:
    fatal ('%s: -a and -b are mutually exclusive')
  if use_infiles:
    if args:
      fatal ('%s: files given, but -i option used')
  else:
    if not args:
      fatal ('%s: no files given, and no -i option used')

  # Create a basic list of files to catalog.
  if use_infiles:
    files = []
    for line in sys.stdin:
      files.append (line.strip ('\r\n'))
  else:
    files = args[:]

  # Filter out any non-gamelike files if required.
  if is_filtered:
    files = [file for file in files if is_gamelike (file)]

  # Filter out any non-existent files.
  paths = []
  for file in files:
    path = os.path.abspath (file)
    if os.path.exists (path):
      paths.append (path)
    else:
      print >>sys.stderr, ('%s: file %s could not be cataloged, not found'
                            % (sys.argv[0], file))

  # Write formatted output.
  if paths:
    if is_iniformat:
      generate_ifmes_ini (outfile, paths, is_verbose)
    elif is_ifictionformat:
      generate_ifiction_xml (outfile, paths, is_verbose)
    else:
      generate_ifmes_xml (outfile, paths, is_verbose)

  # Close file if opened.
  if outfile != sys.stdout:
    outfile.close ()

if __name__ == '__main__':
  main ()
