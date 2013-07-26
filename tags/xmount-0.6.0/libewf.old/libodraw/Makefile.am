AM_LFLAGS = -Cf
AM_YFLAGS = -d -v

if HAVE_LOCAL_LIBODRAW
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/libcstring \
	-I$(top_srcdir)/liberror \
	-I$(top_srcdir)/libnotify \
	@LIBUNA_CPPFLAGS@ \
	@LIBBFIO_CPPFLAGS@ \
	@MEMWATCH_CPPFLAGS@

BUILT_SOURCES = libodraw_cue_parser.h

noinst_LTLIBRARIES = libodraw.la

libodraw_la_SOURCES = \
	libodraw_array_type.c libodraw_array_type.h \
	libodraw_codepage.h \
	libodraw_cue_parser.y \
	libodraw_cue_scanner.l \
	libodraw_data_file.c libodraw_data_file.h \
	libodraw_data_file_descriptor.c libodraw_data_file_descriptor.h \
	libodraw_debug.c libodraw_debug.h \
	libodraw_definitions.h \
	libodraw_error.c libodraw_error.h \
	libodraw_extern.h \
	libodraw_handle.c libodraw_handle.h \
	libodraw_io_handle.c libodraw_io_handle.h \
	libodraw_metadata.c libodraw_metadata.h \
	libodraw_libbfio.h \
	libodraw_libuna.h \
	libodraw_notify.c libodraw_notify.h \
	libodraw_sector_range.c libodraw_sector_range.h \
	libodraw_support.c libodraw_support.h \
	libodraw_track_value.c libodraw_track_value.h \
	libodraw_types.h

libodraw_la_LIBADD = \
	../libcstring/libcstring.la \
	../liberror/liberror.la \
	../libnotify/libnotify.la \
	@LIBUNA_LIBADD@ \
	@LIBBFIO_LIBADD@
endif

EXTRA_DIST = \
	libodraw_cue_parser.c libodraw_cue_parser.h \
	libodraw_cue_scanner.c

MAINTAINERCLEANFILES = \
	Makefile.in

distclean: clean
	/bin/rm -f libodraw_cue_parser.c
	/bin/rm -f libodraw_cue_parser.h
	/bin/rm -f libodraw_cue_parser.output
	/bin/rm -f libodraw_cue_scanner.c
	/bin/rm -f Makefile

splint:
	@echo "Running splint on libodraw ..." 
	-splint -preproc -redef $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(libodraw_la_SOURCES) 

