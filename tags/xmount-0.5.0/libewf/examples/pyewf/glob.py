#! /usr/bin/env python
#
# Python script to glob Expert Witness Compression format file(s) using pyewf
#
# Author:            Joachim Metz
# Creation date:     October 14, 2010
# Modification date: January 4, 2011
#

__author__    = "Joachim Metz"
__version__   = "20110104"
__date__      = "Jan 4, 2011"
__copyright__ = "Copyright (c) 2006-2012, Joachim Metz <jbmetz@users.sourceforge.net>"
__license__   = "GNU LGPL version 3"

import sys
import pyewf

# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

print "glob.py " + __version__ + " (libewf " + pyewf.get_version() + ")\n"

argc = len( sys.argv )

if argc != 2:
	print "Usage: glob.py filename\n"

	sys.exit( 1 )

try:
	filenames = pyewf.glob(
	             sys.argv[ 1 ] )

except:
	print "Unable to glob filename(s)\n"
	print sys.exc_info()[ 1 ]

	sys.exit( 1 )

if len( filenames ) > 0:
	print "Filenames:"

	for filename in filenames:
		print filename.encode( "utf8" )

	print ""

sys.exit( 0 )

