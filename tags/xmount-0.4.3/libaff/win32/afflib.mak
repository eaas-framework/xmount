#
# Windows makefile for AFFLIB & libewf
#

# What to make:
TARGETS = afcompare.exe afconvert.exe afcopy.exe affix.exe afinfo.exe afstats.exe afxml.exe

# These are things you may need to change:
#
# SDK_DIR is where the Windows Platform SDK is installed on your computer
#
SDK_DIR = "C:\Program Files\Microsoft Platform SDK"

# COMPILER_MODE specifies how you want the libaries compiled:

COMPILE_MODE = /MT

LIBEWF  = libewf-20070512
LIBEWFDIR = $(LIBEWF)\libewf


all:   $(TARGETS)

################################################################

INCS =	/I.\
	/Izlib-1.2.3\ \
	/I..\lib \
	/I..\lzma443\C \
	/I..\lzma443\C\7zip\Compress\LZMA_Alone \
	/I$(LIBEWF)\include \
	/I$(SDK_DIR)/Include

EWF_DEFS = /DHAVE_CONFIG_WINDOWS_H /DHAVE_LIBCRYPTO /DHAVE_OPENSSL_EVP_H /DHAVE_WINDOWS_API
DEFS = /DWIN32 /DWIN32_NT /DUSE_LIBEWF /DMSC /D_CRT_SECURE_NO_DEPRECATE $(EWF_DEFS)

CC=cl

# removed: /Gm - enable minimal rebuild; generated internal compiler error

OTHER_FLAGS = /c /nologo /EHsc /RTC1 /RTCs /W2 $(COMPILER_MODE)

CPPFLAGS=$(INCS) $(DEFS) $(OTHER_FLAGS) /Fp"afflib.pch" /Fo$*.obj
CFLAGS=$(INCS) $(DEFS) $(OTHER_FLAGS) /Fp"afflib.pch" /Fo$*.obj

# Here are some useful flags:
# -CODE GENERATION-
# /W4       - warning level 4
# /Gm       - enable minimal rebuild
# /ZI       - enable full Edit and Continue info (conflicts with /OPT:ICF)
# /RTC1     - Enable fast checks
# /RTCs     - Stack Frame runtime checking
#
#
# -PREPROCESSOR-
# /I        - specifies include directory
# /D        - define a switch
#
# -OUTPUT FILES-
# /Fp       - Precompiled headers
#
# -MISCELLANEOUS-
# /nologo   - Disable logo
# /c        - compile only, don't link
#
# -LINKING-
# /MT       - Multithreaded, static link
# /MD       - Multithreaded, Dynamic Link
# /MTd      - Multithreaded, static w/ debugging
# /MDd      - Multithreaded, Dynamic w/ debugging
# Note: "the single-threaded CRT (formerly /ML or /MLd options)
#        are no longer available. Instead, use the multithreaded CRT."
#        http://msdn2.microsoft.com/en-us/library/abx4dbyh(VS.80).aspx

LZMA_OBJS =  \
	..\lzma443\C\7zip\Compress\LZMA_Alone\LzmaBench.obj \
	..\lzma443\C\7zip\Compress\LZMA_Alone\LzmaRam.obj \
	..\lzma443\C\7zip\Compress\LZMA_Alone\LzmaRamDecode.obj \
	..\lzma443\C\7zip\Compress\LZMA_C\LzmaDecode.obj \
	..\lzma443\C\7zip\Compress\Branch\BranchX86.obj \
	..\lzma443\C\7zip\Compress\LZMA\LZMADecoder.obj \
	..\lzma443\C\7zip\Compress\LZMA\LZMAEncoder.obj \
	..\lzma443\C\7zip\Compress\LZ\LZInWindow.obj \
	..\lzma443\C\7zip\Compress\LZ\LZOutWindow.obj \
	..\lzma443\C\7zip\Compress\RangeCoder\RangeCoderBit.obj \
	..\lzma443\C\7zip\Common\InBuffer.obj \
	..\lzma443\C\7zip\Common\OutBuffer.obj \
	..\lzma443\C\7zip\Common\StreamUtils.obj \
	..\lzma443\C\Common\Alloc.obj \
	..\lzma443\C\Common\CommandLineParser.obj \
	..\lzma443\C\Common\CRC.obj \
	..\lzma443\C\Common\String.obj \
	..\lzma443\C\Common\StringConvert.obj \
	..\lzma443\C\Common\StringToInt.obj \
	..\lzma443\C\Common\Vector.obj 

AFF_OBJS = ..\lib\aff_db.obj \
	..\lib\aff_toc.obj \
	..\lib\afflib.obj \
	..\lib\afflib_os.obj \
	..\lib\afflib_pages.obj \
	..\lib\afflib_stream.obj \
	..\lib\afflib_util.obj \
	..\lib\crypto.obj \
	..\lib\base64.obj \
	..\lib\lzma_glue.obj \
	..\lib\s3_glue.obj \
	..\lib\vnode_aff.obj \
	..\lib\vnode_afd.obj \
	..\lib\vnode_afm.obj \
	..\lib\vnode_ewf.obj \
	..\lib\vnode_raw.obj \
	..\lib\vnode_s3.obj \
	..\lib\vnode_split_raw.obj 


EWF_OBJS = \
	 $(LIBEWFDIR)\ewf_compress.obj \
	 $(LIBEWFDIR)\ewf_crc.obj \
	 $(LIBEWFDIR)\ewf_data.obj \
	 $(LIBEWFDIR)\ewf_error2.obj \
	 $(LIBEWFDIR)\ewf_file_header.obj \
	 $(LIBEWFDIR)\ewf_hash.obj \
	 $(LIBEWFDIR)\ewf_ltree.obj \
	 $(LIBEWFDIR)\ewf_section.obj \
	 $(LIBEWFDIR)\ewf_string.obj \
	 $(LIBEWFDIR)\ewf_table.obj \
	 $(LIBEWFDIR)\ewf_volume.obj \
	 $(LIBEWFDIR)\ewf_volume_smart.obj \
	 $(LIBEWFDIR)\libewf_chunk_cache.obj \
	 $(LIBEWFDIR)\libewf_common.obj \
	 $(LIBEWFDIR)\libewf_debug.obj \
	 $(LIBEWFDIR)\libewf_digest_context.obj \
	 $(LIBEWFDIR)\libewf_endian.obj \
	 $(LIBEWFDIR)\libewf_file.obj \
	 $(LIBEWFDIR)\libewf_hash_values.obj \
	 $(LIBEWFDIR)\libewf_header_values.obj \
	 $(LIBEWFDIR)\libewf_internal_handle.obj \
	 $(LIBEWFDIR)\libewf_notify.obj \
	 $(LIBEWFDIR)\libewf_offset_table.obj \
	 $(LIBEWFDIR)\libewf_read.obj \
	 $(LIBEWFDIR)\libewf_section.obj \
	 $(LIBEWFDIR)\libewf_section_list.obj \
	 $(LIBEWFDIR)\libewf_segment_table.obj \
	 $(LIBEWFDIR)\libewf_string.obj \
	 $(LIBEWFDIR)\libewf_write.obj 

ZLIB_OBJS = zlib-1.2.3\adler32.obj \
	zlib-1.2.3\compress.obj \
	zlib-1.2.3\crc32.obj \
	zlib-1.2.3\deflate.obj \
	zlib-1.2.3\gzio.obj \
	zlib-1.2.3\infback.obj \
	zlib-1.2.3\inffast.obj \
	zlib-1.2.3\inflate.obj \
	zlib-1.2.3\inftrees.obj \
	zlib-1.2.3\trees.obj \
	zlib-1.2.3\uncompr.obj \
	zlib-1.2.3\zutil.obj

#
# WIN32_OBJS are extra objects we need on windows because
# they aren't present
#
WIN32_OBJS = getopt.obj \
	   openssl\md5.obj \
	   openssl\sha.obj \


# LIB_OBJS are all of the objects that we'll put in the library

LIB_OBJS = $(EWF_OBJS)	$(AFF_OBJS) $(LZMA_OBJS)  $(WIN32_OBJS) $(ZLIB_OBJS)

afflib.lib: $(LIB_OBJS)
	lib -out:afflib.lib $(LIB_OBJS)

# WIN32_LIBS are the libraries that we link with on win32
# ws2_32.lib = Winsock 2
# advapi32.lib = CryptoAPI support DLL (LIBEWF uses crypto api)

WIN32LIBS = ws2_32.lib advapi32.lib 

clean:
	del afflib.lib $(LIB_OBJS) $(TARGETS)

LINK_OPTS = /libpath:$(SDK_DIR)/Lib /nodefaultlib:libc $(WIN32LIBS)

aftest.exe: ..\lib\aftest.obj afflib.lib
	link -out:aftest.exe ..\lib\aftest.obj afflib.lib $(LINK_OPTS)	    

afcat.exe: ..\tools\afcat.obj afflib.lib
	link -out:afcat.exe ..\tools\afcat.obj afflib.lib $(LINK_OPTS)

afcopy.exe: ..\tools\afcopy.obj ..\tools\utils.obj afflib.lib 
	link -out:afcopy.exe ..\tools\afcopy.obj ..\tools\utils.obj afflib.lib $(LINK_OPTS)

afcompare.exe: ..\tools\afcompare.obj ..\tools\utils.obj afflib.lib 
	link -out:afcompare.exe ..\tools\afcompare.obj ..\tools\utils.obj afflib.lib $(LINK_OPTS)

afconvert.exe: ..\tools\afconvert.obj afflib.lib
	link -out:afconvert.exe ..\tools\afconvert.obj afflib.lib $(LINK_OPTS)

affix.exe: ..\tools\affix.obj afflib.lib
	link -out:affix.exe ..\tools\affix.obj afflib.lib $(LINK_OPTS)

afinfo.exe: ..\tools\afinfo.obj ..\tools\quads.obj afflib.lib
	link -out:afinfo.exe ..\tools\afinfo.obj ..\tools\quads.obj afflib.lib $(LINK_OPTS)

afsegment.exe: ..\tools\afsegment.obj afflib.lib
	link -out:afsegment.exe ..\tools\afsegment.obj afflib.lib $(LINK_OPTS)

afstats.exe: ..\tools\afstats.obj ..\tools\quads.obj afflib.lib
	link -out:afstats.exe ..\tools\afstats.obj ..\tools\quads.obj afflib.lib $(LINK_OPTS)

afxml.exe: ..\tools\afxml.obj ..\tools\quads.obj afflib.lib
	link -out:afxml.exe ..\tools\afxml.obj  ..\tools\quads.obj afflib.lib $(LINK_OPTS)

