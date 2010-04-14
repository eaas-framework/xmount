SUBDIRS = lib tools lzma443 win32 tests doc pyaff man

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = afflib.pc

include_HEADERS = 
pkginclude_HEADERS = lib/afflib.h lib/afflib_i.h lib/afflib_sha256.h lib/aftimer.h lib/utils.h  lib/threaded_hash.h

ACLOCAL_AMFLAGS = -I m4

RELEASE_USER = simsong@
RELEASE_HOST = www.afflib.org
RELEASE_DIR  = afflib.org/

RELEASE_LOC  = $(RELEASE_DIR)/downloads/
CHANGES_LOC  = $(RELEASE_DIR)/ChangeLog.txt
RELEASE_FN   = $(PACKAGE)_version.txt
RELEASE_PATH = $(RELEASE_LOC)/$(RELEASE_FN)
RELEASE_SSH  = $(RELEASE_USER)$(RELEASE_HOST):$(RELEASE_LOC)
RELEASE	     = $(PACKAGE)-$(VERSION)

AM_CFLAGS = -Wall
AM_CPPFLAGS = -Wall


release:
	make prerelease
	make distcheck
	make therelease

prerelease:
	@echo Building and uploading the release...
	@echo Making release $(RELEASE)
	@echo
	echo
	wget http://$(RELEASE_HOST)/downloads/$(RELEASE_FN)
	@echo Version `cat $(RELEASE_FN)` is on the server.
	python checkversion.py $(RELEASE_FN) $(RELEASE)
	/bin/rm $(RELEASE_FN)

therelease:
	scp $(RELEASE).tar.gz $(RELEASE_SSH)
	ssh $(RELEASE_HOST) 'echo $(RELEASE) > $(RELEASE_PATH)'
	ssh $(RELEASE_HOST) 'cd $(RELEASE_LOC);/bin/rm $(PACKAGE).tar.gz;ln -s $(RELEASE).tar.gz $(PACKAGE).tar.gz'
	@echo Release $(RELEASE) uploaded to server

getrelease:
	@echo testing release on your system
	wget http://$(PACKAGE).tar.gz
	tar xfz $(PACKAGE).tar.gz
	(cd afflib-*;./configure;make;make test_crypto)

DEBIAN_FILES = \
        debian/afflib.install \
        debian/afflib.dirs \
        debian/afflib.docs \
        debian/afflib.postinst.debhelper \
        debian/afflib.postrm.debhelper \
        debian/afflib-default.ex \
        debian/afflib-dev.dirs \
        debian/afflib-dev.docs \
        debian/afflib-dev.install \
        debian/afflib-tools.dirs \
        debian/afflib-tools.docs \
        debian/afflib-tools.install \
        debian/afflib.doc-base.EX \
        debian/changelog \
        debian/changelog.in \
        debian/compat \
        debian/control \
        debian/copyright \
        debian/cron.d.ex \
        debian/emacsen-install.ex \
        debian/emacsen-startup.ex \
        debian/emacsen-remove.ex \
        debian/init.d.ex \
        debian/manpage.1.ex \
        debian/manpage.sgml.ex \
        debian/manpage.xml.ex \
        debian/menu.ex \
        debian/rules \
        debian/postinst.ex \
        debian/postrm.ex \
        debian/preinst.ex \
        debian/prerm.ex \
        debian/shlibs.local.ex \
        debian/shlibs.local.ex.in \
        debian/watch.ex

DOCS = BUGLIST.txt README_Linux.txt README_Win32.txt \
	doc/crypto_doc.txt \
	doc/crypto_design.txt \
	doc/announce_1.7.txt \
	doc/announce_1.8.txt \
	doc/announce_2.2.txt

EXTRA_DIST = $(DOCS) \
	$(DEBIAN_FILES) \
	acx_pthread.m4 \
	afflib.spec.in \
	afflib.spec \
	afflib.pc.in \
	afflib.pubkey.asc \
	misc/expat-2.0.1.tar.gz \
	tests/encrypted.iso \
	tests/encrypted.aff

renew:
	touch NEWS README AUTHORS ChangeLog stamp-h
	aclocal
	libtoolize -f  
	autoheader -f
	autoconf -f
	automake --add-missing -c

renew-on-mac:
	touch NEWS README AUTHORS ChangeLog stamp-h
	aclocal
	glibtoolize -f  # perhaps this one works better for you
	aclocal
	autoheader -f
	autoconf -f
	automake --add-missing -c

#
# Note: don't forget to run autoreconf when significant changes are made
#
