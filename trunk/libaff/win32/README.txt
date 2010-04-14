Information for compiling AFFLIB under Windows / Microsoft Visual C++

Introduction
============

This directory builds the following as a single library using
Microsoft Visual C++ that contains all of the following:

     * All of LIBAFF
     * LZMA compression system
     * ZLIB compression system
     * LIBEWF EnCase image reading system.

On Unix systems the ZLIB and LIBEWF libraries must be separately
installed. However, copies of these libraries are included as
subdirectories to the win32 library. These copies are included solely
to make things easier for Windows users; these copies are not used by
the Unix AFFLIB installation.

Windows programs that are linked with this library can read files in
any of the following formats:

    * RAW & Split raw
    * AFF, AFM, AFD
    * EnCase / Expert Witness


Compiling
=========

To compile this library, you need a copy of Microsoft Visual C++ 2005. 
(Libewf will not compile with any earlier version of Microsoft Visual C++.)


Installing VC++ 2500:
---------------------
You can download a FREE copy of Visual C++ 2005 Express Edition from
Microsoft: http://msdn.microsoft.com/vstudio/express/visualc/download/

1. Go to http://msdn.microsoft.com/vstudio/express/visualc/download/
   Note new location: http://msdn2.microsoft.com/en-us/vstudio/aa718376.aspx
   Note new location: http://msdn2.microsoft.com/en-us/vstudio/Aa700736.aspx

2. Download Visual Studio C++ 2005 Express Edition.  
   (You will get vcsetup.exe; save it on the desktop and run it.)

3. Select the following installation options:

   [*] Graphical IDE
   [*] Microsoft MSDN 2005 Express Edition

   You do not need to install Microsoft SQL Server 2005, but you may
   if you wish. 

4. Install in the default location, 
   C:\Program Files\Microsoft Visual Studio 8\

5. Follow the instructions:
   - Run Windows Update to install the latest service pack.
   - Register the product so that it doesn't fail in 30 days.
   - Get the registration key from Microsoft (after email answerback)
     and paste it into the Help panel.

6. Now install VC++ 2005 Service Pack #1

7. Now you must download and install the Microsoft Platform SDK so
   that you will have the header files for the Microsoft Crypto API.
   This can be confusing. I downloaded the Windows Server 2003 SP1
   Platform SDK Web Install. I got it from this URL:
   http://www.microsoft.com/msdownload/platformsdk/sdkupdate/

   (Be careful not to download the x64 platform SDK unless you are
   running on a 64-bit machine!)

   - Be sure that you DO NOT chose the configuration option to
     Register environment variables. 

   YOU CANNOT COMPILE AFFLIB UNLESS THE PLATFORM SDK IS INSTALLED.

   - If possible, install the platform SDK as 
     C:\Program Files\Microsoft Platform SDK\. 

     If you can't do this, you will need to modify afflib.mak to
     reflect the actual install location


Compiling AFFLIB:
-----------------
1. Unpack the afflib distribution into a directory such as c:\afflib

2. From the Windows Start menu, run the Visual Studio 2005 Command Prompt

3. Change into the win32 subdirectory, e.g. "chdir c:\afflib\win32"

4. Type "make.bat" to run the makefile.

3. The command file "make.bat" will compile AFFLIB, LIBEWF, ZLIB and LZMA, and create a single .lib
   file. Compilation options are specified in afflib.mak in this directory.  

4. Only one program is ported at this point, afcat.exe. You can compile it with:

   % make afcat.exe

5. To open a multi-file EnCase file, just specify the first .E01 file; the AFFLIB 
   implementation will automatically look for all of the other EnCase files.

6. DLLs are no longer required to run these programs:
    -  dependencies on OpenSSL have been removed 
    (sorry; currently no signing under windows)
    -  zlib is now compiled-in (statically-linked).

7. Right now you should really use this library for READING AFF & E01
   files, rather than WRITING them.  Writing should work, but it's not
   very well tested on Windows. S3 is currently not supported on Windows. 

8. If you want to change the compile switches, feel free. They're in afflib.mak




1. You will need to download and install OpenSSL, ZLIB, and expat
   a. no, I lied. zlib is in zlib123
   b. I've used the Shining Light Products OpenSSL v.0.9.8 for Win32. www.slproweb.com
      Install it in c:\OpenSSL

      It will install some things in \Windows\System\

2. 


#
# Local Variables:
# mode: flyspell
# mode: auto-fill
# LocalWords:  AFFLIB
# End:
#
