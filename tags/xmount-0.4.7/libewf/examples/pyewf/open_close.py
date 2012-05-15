#! /usr/bin/env python
#
# Python script to open and close Expert Witness Compression format file(s) using pyewf
#
# Author:            Joachim Metz
# Creation date:     September 29, 2010
# Modification date: January 4, 2011
#

__author__    = "Joachim Metz"
__version__   = "20110104"
__date__      = "Jan 4, 2011"
__copyright__ = "Copyright (c) 2010-2011, Joachim Metz <jbmetz@users.sourceforge.net>"
__license__   = "GNU LGPL version 3"

import sys
import pyewf

# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

print "open_close.py " + __version__ + " (libewf " + pyewf.get_version() + ")\n"

argc = len( sys.argv )

if argc < 2:
	print "Usage: open_close.py filename(s)\n"

	sys.exit( 1 )

handle = pyewf.new_handle();

if handle == None:
	print "Missing handle object\n"

	sys.exit( 1 )

try:
	handle.open(
	 sys.argv[ 1: ],
	 pyewf.get_access_flags_read() )

except:
	print "Unable to open file(s)\n"
	print sys.exc_info()[ 1 ]

	sys.exit( 1 )

try:
	handle.close()
except:
	print "Unable to close file(s)\n"
	print sys.exc_info()
 
	sys.exit( 1 )

sys.exit( 0 )

