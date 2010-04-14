#!/bin/sh
# 
# test to make sure we haven't broken encryption...

if [ x${srcdir} == "x" ] ; 
  then 
     TDIR="../tests/"
  else 
     TDIR=$srcdir/../tests/
fi

if ! ./afcompare $TDIR/encrypted.iso file://:password@/$TDIR/encrypted.aff ; then exit 1 ; fi
exit 0
