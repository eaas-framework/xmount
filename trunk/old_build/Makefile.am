##Process this file with automake to create Makefile.in
bin_PROGRAMS = xmount
man_MANS = xmount.1
xmount_SOURCES = md5.c xmount.c libaewf/aewf.c libaaff/aaff.c libdd/dd.c
xmount_CFLAGS = -fno-strict-aliasing -D_LARGEFILE64_SOURCE -D_GNU_SOURCE -std=c99 $(fuse_CFLAGS)
LIBS += $(fuse_LIBS)
# Statically link against local libaff and libewf
# Linux
#xmount_LDADD = libaff/lib/.libs/libafflib.a libewf/libewf/.libs/libewf.a /usr/lib/libssl.a /usr/lib/libcrypto.a -lstdc++ -lz
#xmount_CFLAGS += -Ilibewf/include
# OSX using osxfuse
#xmount_LDADD = libaff/lib/.libs/libafflib.a libewf/libewf/.libs/libewf.a /opt/local/lib/libcrypto.a /opt/local/lib/libssl.a -lstdc++ -lz -losxfuse
#xmount_CFLAGS += -Ilibewf/include -I/usr/local/include/osxfuse/ -D_FILE_OFFSET_BITS=64
