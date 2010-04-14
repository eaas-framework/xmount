		       Using AFFLIB Under Microsoft Windows (Win32)

AFFLIB version 2.0 and above has been used successfully with Microsoft
Visual C++, Borland C++, and Cygwin.  This file provides step-by-step
instructions for compiling AFFLIB from sources on these platforms. You
can also download pre-compiled binaries and executables from the
AFFLIB website.


Microsoft VC++
==============
Full information for compiling and installing AFFLIB under VC++ is in the win32/README.txt file.


Cygwin
======
Cygwin is a Unix emulation system that allows standard Linux/Unix open
source software to be run on top of Windows through the use of a
special "cygwin" DLL. 

To use Cygwin, follow these step-by-step instructions:

1. Go to http://www.cygwin.com/. 

2. Click "Download Cygwin Now"; this will give you an executable file.

3. Run the Cygwin Net Release Setup Program. 

4. Select "Install from Internet"

5. Install into the C:\cygwin directory for All Users. Select
   "Unix/binary" as the default text file type.

6. Select a mirror site.

7. Click on the arrows next to "Devel" to change the word "Default" to
   "Install". This will cause the entire Cygwin development system to
   be installed.

8. Click "Next" and come back in an hour.

9. When you get the "Installation Complete" message, start the cygwin
   shell and type the following:

	 $ mkdir afflib
	 $ cd afflib
	 $ wget http://www.afflib.org/afflib.tar.gz
	 $ tar xfvz afflib.tar.gz
	 $ cd afflib*
	 $ ./configure
	 $ make
	 $ make install

  NOTE: EnCase support will NOT be compiled in unless you separately
  download and install LIBEWF.  libewf must be downloaded from 
  https://www.uitwisselplatform.nl/projects/libewf/
	 

10. Have fun!

Although we do not test every version of AFFLIB on Cygwin, every
effort is made to assure that every version of AFFLIB will compile on
Cygwin. The following versions have been explicitly tested and are
known to work:

      afflib-2.2.12
      afflib-2.3.0


Borland C++
===========
Although changes have been made to AFFLIB to support compliation under
Borland, we do not have instructions available at this time.


