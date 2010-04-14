##Process this file with automake to create Makefile.in
SUBDIRS = libaff libewf
bin_PROGRAMS = xmount
xmount_SOURCES = md5.c xmount.c
xmount_CFLAGS = -D_FILE_OFFSET_BITS=64
# Link against the not installed static libaff and libewf
xmount_LDADD = libaff/lib/.libs/libafflib.a libewf/libewf/.libs/libewf.a -lstdc++
