		       Using AFF Tool Under Microsoft Windows (Win32)


There are two ways to use AFFLIB with Windows: you can download the
pre-compiled executables, or you can compile your own.  The advantage
of the pre-compiled executables is that they work. The advantage of
compiling the executables yourself is that you can modify them.

Downloading and Installing
==========================
You can download the current version of AFF Tools from:

    http://afflib.org/downloads/afflib_windows.zip

The ZIP file contains:
    * pre-compiled executables for AFF Tools
    * lib32eay.dll, the OpenSSL DLL (cryptography support for AFFLIB)
    * bulk_extractor jar and bat file. (Use the bat file to run the jar file)

Install these tools by:

1. Unzip the archive into the c:\afflib directory.
2. Add c:\afflib to your system PATH directory by:
   a. Opening the System control panel.
   b. Clicking the "Environment Variables" button.
   c. Adding "c:\afflib;" to the beginning of the PATH environment variable.


Working with the tools
======================

If you are working with an encrypted disk image, set the environment
variable AFFLIB_PASSPHRASE to be the passphrase that should be used
for decryption.

   % set AFFLIB_PASSPHRASE="this_is_my_passphrase"

Displaying the metadata with a disk image:

   % afinfo.exe filename.aff	  

To convert an AFF file into a RAW file, use:

   % affconvert.exe -e raw filename.aff


To reliably copy an AFF file from one location to another:

   % afcopy.exe  file1.aff  d:\dest\path\file2.aff


To compare two AFF files:

   % afcompare file1.aff file2.aff


To fix a corrupted AFF file:

  % affix badfile.aff


To print statistics about a file:

  % afstats.exe filename.aff



Diskprint
=================
An exciting feature in AFF 3.5 is the ability to rapidly calculate and
verify the "print" of a disk image. A print is constructed by
computing the SHA-256 of the beginning, end, and several randomly
chosen parts of the disk image.

To calculate the diskprint and store it in a file:

   % afdiskprint myfile.iso > myfile.xml

To verify a diskprint

   % afdiskprint -x myfile.xml myfile.iso



Verifying the AFFLIB Digital Signature
===============================
Some organizations require that dgital signatures be verified on programs that are downloaded.

Some AFF distributions are now signed with the AFFLIB privat key. You
can verify the distribution by downloading a copy of the public key
from the AFFLIB website or the GPG key server. 

The public key can be downloaded from the website:

    http://afflib.org/pubkey.asc

You can also download the key directly from the GPG keyserver with
this command:

  $ gpg --keyserver subkeys.pgp.net --recv-keys 805B3DB0
  gpg: requesting key 805B3DB0 from hkp server subkeys.pgp.net
  gpg: /home/simsong/.gnupg/trustdb.gpg: trustdb created
  gpg: key 805B3DB0: public key "AFFLIB Distribution (Simson L. Garfinkel)" imported
  gpg: Total number processed: 1
  gpg:               imported: 1
  $

