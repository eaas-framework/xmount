#!/bin/sh
#
echo Bootstrap script to create configure script using autoconf
echo
# use the installed ones first, not matter what the path says.
export PATH=/usr/bin:/usr/sbin:/bin:$PATH
touch NEWS README AUTHORS ChangeLog stamp-h
aclocal
LIBTOOLIZE=glibtoolize
if test `which libtoolize`x != "x" ; 
  then LIBTOOLIZE=libtoolize 
fi
$LIBTOOLIZE  -f
autoheader -f
autoconf -f
automake --add-missing -c
echo "Ready to run configure!"
if [ $1"x" != "x" ]; then
  ./configure "$@"
fi

