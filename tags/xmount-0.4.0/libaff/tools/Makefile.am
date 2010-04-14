bin_PROGRAMS = afcat afcompare afconvert afcopy afcrypto affix afinfo afsegment \
	afstats afverify afxml affuse afrecover afsign

EXTRA_DIST = test_make_random_iso.sh test_crypto.sh test_signing.sh test_recovery.sh \
	test_passphrase.sh test_afsegment.sh

EXTRA_PROGRAMS = test_fix_perms 

TESTS = test_fix_perms test_signing.sh test_recovery.sh test_passphrase.sh test_afsegment.sh test_crypto.sh

# See http://www.gnu.org/software/libtool/manual.html		# Using-Automake

AM_LDFLAGS = -static		# staticly link our tools (easier debugging)

test_fix_perms_SOURCES = test_fix_perms.c

afcat_SOURCES = afcat.cpp 

afcrypto_SOURCES = afcrypto.cpp

afcompare_SOURCES = afcompare.cpp  unix4win32.h

afconvert_SOURCES = afconvert.cpp  unix4win32.h

afcopy_SOURCES = afcopy.cpp unix4win32.h aff_bom.h aff_bom.cpp

affix_SOURCES = affix.cpp unix4win32.h

affuse_SOURCES = affuse.c

afinfo_SOURCES = afinfo.cpp unix4win32.h

afrecover_SOURCES = afrecover.cpp unix4win32.h

afsegment_SOURCES = afsegment.cpp

afsign_SOURCES = afsign.cpp aff_bom.h aff_bom.cpp

afstats_SOURCES = afstats.cpp

afverify_SOURCES = afverify.cpp aff_bom.h aff_bom.cpp

afxml_SOURCES = afxml.cpp unix4win32.h


INCLUDES = -I@top_srcdir@/lib/
LDADD = @top_builddir@/lib/libafflib.la 

affuse_CFLAGS = @FUSE_CFLAGS@
affuse_LDADD = @top_builddir@/lib/libafflib.la @FUSE_LIBS@ 
affuse_LINK = $(CXXLINK)

CLEANFILES = deskclerk.pem test_password.sh blank.iso archives.pem password_signed.aff signed.aff \
	random.iso agent.pem r3.iso random*.aff blank.aff blanke.aff words r3.iso evidence1.aff test_fix_perms
