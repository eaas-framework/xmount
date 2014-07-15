#!/bin/sh

# The easiest way to build AFFLIB for Win32 is to ues MinGW as either a cross-compiler
# or as a native compiler. In either case you will need to install OpenSSL and optionally
# libregex.

# This script will compile AFFLIB for Win32 on either Mac or Linux.

# To install OpenSSL on On Linux, first download OpenSSL 1.0.0 and then:
# ./Configure  mingw --prefix=/usr/i586-mingw32msvc/
# sudo make install


if test -r /opt/local/bin/i386-mingw32-gcc ; then
  echo Compiling for mingw on a Mac installed with MacPorts
  export CC=/opt/local/bin/i386-mingw32-gcc
  export CXX=/opt/local/bin/i386-mingw32-g++
  export RANLIB=/opt/local/bin/i386-mingw32-ranlib
  export AR=/opt/local/bin/i386-mingw32-ar
  export PREFIX=/opt/local/i386-mingw32/
  export MINGWFLAGS="-mwin32 -mconsole -march=pentium4 "
  export CFLAGS="$MINGWFLAGS"
  export CXXFLAGS="$MINGWFLAGS"
  autoreconf -f
  ./configure CC=$CC CXX=$CXX RANLIB=$RANLIB --target=i586-mingw32msvc --host=i586 \
      --prefix=$PREFIX
  make CC=$CC CXX=$CXX RANLIB=$RANLIB CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS"
fi
if test -r /usr/i586-mingw32msvc ; then
  echo Compiling for mingw on Linux
  export PREFIX=/usr/i586-mingw32msvc
  export MINGW32PATH=/usr/i586-mingw32msvc
  export CC=/usr/bin/i586-mingw32msvc-gcc
  export CXX=/usr/bin/i586-mingw32msvc-g++
  export AR=${MINGW32PATH}/bin/ar
  export RANLIB=${MINGW32PATH}/bin/ranlib
  export STRIP=${MINGW32PATH}/bin/strip
  export MINGWFLAGS="-mwin32 -mconsole -march=i586 "
  export CFLAGS="$MINGWFLAGS"
  export CXXFLAGS="$MINGWFLAGS"
  autoreconf -f
  ./configure CC=$CC CXX=$CXX RANLIB=$RANLIB --target=i586-mingw32msvc --host=i586 \
      --enable-winapi=yes --prefix=$PREFIX
  make CC=$CC CXX=$CXX RANLIB=$RANLIB CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS"
fi

# Make a release of the executables

zip afflib_windows.zip tools/*.exe

