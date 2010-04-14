                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Order is largely irrevellant, although it must start with AC_INIT and end with AC_OUTPUT
# See http://autotoolset.sourceforge.net/tutorial.html
# and http://www.openismus.com/documents/linux/automake/automake.shtml


AC_INIT([AFFLIB],[3.3.6],[bugs@afflib.org])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE


# Where we get installed
AC_PREFIX_PROGRAM

# Programs that we will be using
AC_PROG_CC
AC_PROG_INSTALL

# Turn off shared libraries during beta-testing, since they
# make the build process take too long.
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

# We are now threading
m4_include([acx_pthread.m4])
m4_include([acinclude.m4])

ACX_PTHREAD()


# AC_PROG_RANLIB not needed if you are using AC_PROG_LIBTOOL
# AC_PROG_RANLIB
AC_CONFIG_FILES([Makefile lib/Makefile tools/Makefile lzma443/Makefile win32/Makefile 
			  tests/Makefile doc/Makefile pyaff/Makefile man/Makefile])
dnl Have configure make distribution specific files
AC_CONFIG_FILES([debian/changelog])
AC_CONFIG_FILES([debian/shlibs.local.ex])
AC_CONFIG_FILES([afflib.spec])
dnl Have configure write its configuration
AM_CONFIG_HEADER([affconfig.h])

# Bring additional directories where things might be found into our
# search path. I don't know why autoconf doesn't do this by default
for spfx in /usr/local /opt/local /sw ; do
    echo checking ${spfx}/include
    if test -d ${spfx}/include; then
        CPPFLAGS="-I${spfx}/include $CPPFLAGS"
        LDFLAGS="-L${spfx}/lib $LDFLAGS"
    fi
done
        

AC_DEFINE([IN_AFFLIB],1,[Defined if we are inside AFFLIB])

# Special features that I use
AC_FUNC_ALLOCA
AC_FUNC_FSEEKO
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_SYS_LARGEFILE


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM			
AC_STRUCT_ST_BLOCKS
AC_STRUCT_ST_RDEV


# Specific headers that I plan to use
AC_CHECK_HEADERS([arpa/inet.h assert.h ctype.h dmalloc.h err.h errno.h fcntl.h getopt.h inttypes.h linux/fs.h malloc.h ncurses/term.h netinet/in.h signal.h stdio.h stdlib.h string.h sys/cdefs.h sys/disk.h sys/file.h sys/ioctl.h sys/ioctl.h sys/param.h sys/param.h sys/socket.h sys/signal.h sys/stat.h sys/time.h sys/types.h sys/vfs.h sysexits.h term.h time.h unistd.h zlib.h])

AC_CHECK_MEMBER([struct sockaddr_in.sin_len],
		[ AC_DEFINE(HAVE_SOCKADDR_SIN_LEN, 1, [Do we have sockaddr.sin_len?]) ],
		[],
		[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
])


# Specific functions that we want to know about
AC_CHECK_FUNCS([getprogname strlcpy strlcat err_set_exit srandom srandomdev flock])
AC_CHECK_FUNCS([fstatfs valloc isdigit isalnum isalphanum isatty popen])
AC_CHECK_FUNCS([ftruncate memset mkdir putenv regcomp strcasecmp strchr strdup strerror strrchr])
AC_CHECK_FUNCS([err errx warn warnx])

# Special features that can be enabled or disabled
AC_ARG_WITH([noopt], AC_HELP_STRING([--with-noopt],[Drop -O C flags]))

# C++ headers
AC_PROG_CXX
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([cstring])
AC_LANG_POP([C++])



################################################################
## LIBEWF support
dnl Function to test if a libewf_get_media_size takes value as an argument
AC_DEFUN([AFF_CHECK_LIBEWF_GET_MEDIA_SIZE],
        [AC_CHECK_FUNC(
         [libewf_get_media_size],
         [AC_DEFINE(
          [HAVE_LIBEWF_GET_MEDIA_SIZE],
          [1],
          [Define to 1 if you have the libewf_get_media_size function.])
	 AC_LANG_PUSH(C)
	 AC_MSG_CHECKING(
	  [if libewf_get_media_size takes size as an argument.])
	 AC_LINK_IFELSE(
		[AC_LANG_PROGRAM(
		 [[#include <libewf.h>]],
		 [[libewf_get_media_size(NULL,NULL); ]] )],
		[AC_MSG_RESULT(
		 [yes])
		AC_DEFINE(
		 [HAVE_LIBEWF_GET_MEDIA_SIZE_ARGUMENT_VALUE],
		 [1],
		 [Define to 1 if libewf_get_media_size takes value as an argument.])],
		[AC_MSG_RESULT(
		 [no])] )
	 AC_LANG_POP(C) ])
])

dnl Function to test if a libewf_get_chunk_size takes value as an argument
AC_DEFUN([AFF_CHECK_LIBEWF_GET_CHUNK_SIZE],
        [AC_CHECK_FUNC(
         [libewf_get_chunk_size],
         [AC_DEFINE(
          [HAVE_LIBEWF_GET_CHUNK_SIZE],
          [1],
          [Define to 1 if you have the libewf_get_chunk_size function.])
	 AC_LANG_PUSH(C)
	 AC_MSG_CHECKING(
	  [if libewf_get_chunk_size takes size as an argument.])
	 AC_LINK_IFELSE(
		[AC_LANG_PROGRAM(
		 [[#include <libewf.h>]],
		 [[libewf_get_chunk_size(NULL,NULL); ]] )],
		[AC_MSG_RESULT(
		 [yes])
		AC_DEFINE(
		 [HAVE_LIBEWF_GET_CHUNK_SIZE_ARGUMENT_VALUE],
		 [1],
		 [Define to 1 if libewf_get_chunk_size takes value as an argument.])],
		[AC_MSG_RESULT(
		 [no])] )
	 AC_LANG_POP(C) ])
])

dnl Function to test if a libewf_get_bytes_per_sector takes value as an argument
AC_DEFUN([AFF_CHECK_LIBEWF_GET_BYTES_PER_SECTOR],
        [AC_CHECK_FUNC(
         [libewf_get_bytes_per_sector],
         [AC_DEFINE(
          [HAVE_LIBEWF_GET_BYTES_PER_SECTOR],
          [1],
          [Define to 1 if you have the libewf_get_bytes_per_sector function.])
	 AC_LANG_PUSH(C)
	 AC_MSG_CHECKING(
	  [if libewf_get_bytes_per_sector takes size as an argument.])
	 AC_LINK_IFELSE(
		[AC_LANG_PROGRAM(
		 [[#include <libewf.h>]],
		 [[libewf_get_bytes_per_sector(NULL,NULL); ]] )],
		[AC_MSG_RESULT(
		 [yes])
		AC_DEFINE(
		 [HAVE_LIBEWF_GET_BYTES_PER_SECTOR_ARGUMENT_VALUE],
		 [1],
		 [Define to 1 if libewf_get_bytes_per_sector takes value as an argument.])],
		[AC_MSG_RESULT(
		 [no])] )
	 AC_LANG_POP(C) ])
])

AC_ARG_ENABLE([libewf],
	AC_HELP_STRING([--enable-libewf=yes], [Use libewf for reading EnCase files(default yes)]),
        [enable_libewf=$enableval], [enable_libewf=yes])
if test "${enable_libewf}" = "yes" ; then
  AC_CHECK_LIB([ewf],[libewf_get_version],,[enable_libewf=no])
  AC_CHECK_HEADERS([libewf.h],,[enable_libewf=no])
fi
if test "${enable_libewf}" = "yes"; then
      AFF_CHECK_LIBEWF_GET_MEDIA_SIZE
      AFF_CHECK_LIBEWF_GET_CHUNK_SIZE
      AFF_CHECK_LIBEWF_GET_BYTES_PER_SECTOR
      AC_DEFINE([USE_LIBEWF],1,[Use libewf to read EnCase files])
fi

################################################################
# For AFF tools
AC_CHECK_FUNCS(tputs tgoto tgetstr tgetnum gotorc beep endwin setupterm printw)
AC_CHECK_HEADERS([readline/readline.h])
AC_CHECK_HEADERS([curses.h termcap.h])
AC_CHECK_LIB([readline],[readline],, AC_MSG_RESULT([readline not installed]))
AC_CHECK_LIB([ncurses],[initscr],, AC_MSG_RESULT([ncurses not installed]))
AC_CHECK_LIB([z],[uncompress],, AC_MSG_ERROR([zlib not installed; cannot continue.]))
AC_CHECK_LIB([rt],[aio_error64])

################################################################
## Expat
## Required for S3 and Digital Signatures
##
AC_ARG_WITH(expat,
  AS_HELP_STRING([--with-expat=PATH], [where libexpat is compiled (if it isn't installed); required for S3 and Digital Signatures]),
  [LDFLAGS="-L${with_expat} $LDFLAGS" ;
   CPPFLAGS="-I${with_expat}/lib $CPPFLAGS"])

have_expat=yes
AC_CHECK_HEADER([expat.h])
AC_CHECK_LIB([expat],[XML_ParserCreate],,[have_expat="no ";AC_MSG_WARN([expat not found; S3 and Digital Signatures not enabled])])


################################################################
## Amazon S3
## S3 requires curl and expat; otherwise we don't need them
AC_ARG_ENABLE(s3,
	AC_HELP_STRING([--enable-s3=yes],
          [Support for Amazon's S3 service. Requires CURL and Expat.]),
          [enable_s3=$enableval], [enable_s3=no])

if test "x${enable_s3}" = "xyes" ; then
  AC_MSG_NOTICE([S3 support requested. Looking for curl and expat...])
  AC_PATH_PROG(CURL_CONFIG,curl-config)
  AC_ARG_WITH(curl,
            AC_HELP_STRING([--with-curl=PATH], [where libcurl is installed; required for S3]),
            [CURL_CONFIG="${with_curl}/bin/curl-config"])
  if test -f "${CURL_CONFIG}"; then
      LDFLAGS="`${CURL_CONFIG} --libs` $LDFLAGS"
      CPPFLAGS="`${CURL_CONFIG} --cflags` $CPPFLAGS"
  else
      AC_MSG_WARN([curl-config not found, guessing at libcurl build settings])
  fi
  AC_CHECK_HEADER([curl/curl.h],,
    AC_MSG_WARN([curl/curl.h not found; Disabling S3 Support.])
    enable_s3=no)
  AC_CHECK_LIB([curl],[curl_global_init],,
    AC_MSG_WARN([Curl library corrupt; Disabling S3 Support.])
    enable_s3=no)
fi

S3_BIN=
if test "x${enable_s3}" = "xyes" ; then
  AC_DEFINE([USE_S3],1,[Enable support for Amazon S3])
  S3_BIN='s3$(EXEEXT)'
fi
AC_SUBST(S3_BIN)
AM_CONDITIONAL([MAYBE_S3],[test "x${enable_s3}" = "xyes"])   # used by automake

################################################################
### QEMU 
################################################################

AC_ARG_ENABLE([qemu])

echo "disable_qemu: " ${disable_qemu}
echo "enable_qemu: " ${enable_qemu}

if test "x${enable_qemu}" = "xno" ; then
   enable_qemu="no"
   AC_DEFINE([DISABLE_QEMU],1,[User has disabled QEMU support])
else
   AC_DEFINE([USE_QEMU],1,[Use QEMU image drivers])
   enable_qemu="yes"
fi
AM_CONDITIONAL([MAYBE_QEMU],[test "x${enable_qemu}" = "xyes"])  # used by automake


################################################################
### PyAFF 
################################################################
# python checks 
# (requires autoconf 1.5+ and the macros in acinclude.m4)
AC_ARG_ENABLE([python],
	AC_HELP_STRING([--enable-python=no], [Build python bindings (pyaff)]),
        [enable_python=$enableval], [enable_python=no])
if test "${enable_python}" = "yes" ; then
  AM_PATH_PYTHON([2.5])
  AC_PYTHON_DEVEL()
  AC_DEFINE([HAVE_PYTHON],1,[Build Python bindings])
fi

AM_CONDITIONAL(HAVE_PYTHON, test "$enable_python" = yes)

##
################################################################
################################################################
## Crypto (must follow S3)
###


AC_CHECK_LIB([ssl],[ssl3_new],,
	AC_MSG_ERROR([OpenSSL developer library 'libssl-dev' or 'openssl-devel' not installed; cannot continue.]))
AC_CHECK_LIB([crypto],[MD5_Update],,
	AC_MSG_ERROR([OpenSSL developer library 'libcrypto' not installed; cannot continue.]))	

AC_CHECK_HEADERS([openssl/aes.h openssl/bio.h openssl/evp.h openssl/hmac.h openssl/md5.h openssl/rand.h openssl/rsa.h openssl/sha.h openssl/pem.h])

AC_CHECK_FUNCS([MD5 SHA1 AES_encrypt RAND_pseudo_bytes des_read_pw_string EVP_read_pw_string])
AC_CHECK_LIB([md],[MD5])		# if libmd is available, get it

### SHA256 insanity. 
### Some sha.h include files have sha256 defined and some do not
### Some systems put it in a different location.
### the whole thing is really disturbing
AC_CHECK_HEADERS([sha256.h])		# on some system
AC_CHECK_HEADERS([openssl/fips_sha.h])	# on other systems
AC_CHECK_FUNCS([SHA256_Init])           # try to figure out what it's called
AC_CHECK_FUNCS([EVP_sha256],[evp_sha256=yes],[evp_sha256="no "])
AC_CHECK_FUNCS([PEM_read_bio_RSA_PUBKEY])

################################################################
## FUSE: Filesystem in Userspace
AC_ARG_ENABLE(fuse,
	AC_HELP_STRING([--enable-fuse=yes],
	       [Support for FUSE, Filesystem in Userspace. (default yes)]),
               [enable_fuse=$enableval], [enable_fuse=yes])
if test "x${enable_fuse}" = "xyes" ; then
  AC_MSG_NOTICE([FUSE requested])
  CPPFLAGS="-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 $CPPFLAGS"
  if test `uname -s` = Darwin ; then
    AC_MSG_NOTICE([FUSE IS NOT SUPPORTED ON MACOS])
    enable_fuse=no
  fi
  AC_CHECK_HEADER([fuse.h],,
    AC_MSG_NOTICE([fuse.h not found; Disabling FUSE support.])
    enable_fuse=no)
  AC_SUBST(FUSE_LIBS)
  AC_SUBST(FUSE_CFLAGS)
fi
AFFUSE_BIN=
if test "${enable_fuse}" = "yes"; then
  AC_DEFINE([USE_FUSE],1,[Use FUSE to mount AFF images])
  AFFUSE_BIN='affuse$(EXEEXT)'
  FUSE_LIBS=-lfuse
fi
AC_SUBST(AFFUSE_BIN)
AM_PROG_CC_C_O			dnl for affuse

############## drop optimization flags if requeted ################

if test x"${AFF_NOOPT}" != "x" ; then
   with_noopt="yes";
fi

if test "${with_noopt}" = "yes" ; then
  CFLAGS=`echo "$CFLAGS" | sed s/-O[[0-9]]//`             # note the double quoting! 
  CXXFLAGS=`echo "$CXXFLAGS" | sed s/-O[[0-9]]//`
fi

################ drop threading if requested ################
AC_ARG_ENABLE(threading,
  [  --disable-threading    builds library without threading
   --enable-threading   use threading if available])
if test "x$disable_threading" != xno; then
  PTHREAD_CFLAGS=""
  PTHREAD_CXXFLAGS=""
  PTHREAD_LDFLAGS=""
  AC_DEFINE(DISABLE_PTHREAD,1,[User has disabled PTHREADING])
fi
AM_CONDITIONAL([THREADING], [test "x$enable_threading" != xno])

###### Tell user what flags we have #######
# see http://pre.plt-scheme.org/plt/src/mzscheme/configure.ac
#
AC_MSG_NOTICE([])
AC_MSG_NOTICE([*****************************************])
AC_MSG_NOTICE([AFFLIB ${PACKAGE_VERSION} configuration])
AC_MSG_NOTICE([Amazon S3 Support:  ${enable_s3}])
AC_MSG_NOTICE([LZMA Compression:   yes])
AC_MSG_NOTICE([LIBEWF:             ${enable_libewf}])
AC_MSG_NOTICE([PYTHON Bindings:    ${enable_python}])
AC_MSG_NOTICE([QEMU Image Drivers: ${enable_qemu}])
AC_MSG_NOTICE([FUSE:               ${enable_fuse}])
AC_MSG_NOTICE([LIBEXPAT:           ${have_expat} (needed for AFF signatures)])
AC_MSG_NOTICE([EVP_SHA256:         ${evp_sha256} (in OpenSSL -- also needed for AFF signatures)])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([CPPFLAGS:   ${CPPFLAGS}])
AC_MSG_NOTICE([CXXFLAGS:   ${CXXFLAGS}])
AC_MSG_NOTICE([CFLAGS:     ${CFLAGS}])
AC_MSG_NOTICE([LIBS:       ${LIBS}])
AC_MSG_NOTICE([LDFLAGS:    ${LDFLAGS}])
AC_MSG_NOTICE([*****************************************])
AC_MSG_NOTICE([])
AC_OUTPUT([afflib.pc])


