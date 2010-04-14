##Process this file with automake to create Makefile.in
bin_PROGRAMS = xmount
xmount_SOURCES = md5.c xmount.c
xmount_CFLAGS = -fno-strict-aliasing -D_LARGEFILE64_SOURCE $(fuse_CFLAGS)
LIBS += $(fuse_LIBS)
