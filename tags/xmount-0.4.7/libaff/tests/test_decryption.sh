#!/bin/sh
echo === VERIFY THAT WE CAN STILL DECRYPT encrypted.aff ===
unset AFFLIB_PASSPHRASE

if test "x$srcdir" = x; then
  srcdir=.
fi

if ( ../tools/affcompare file://:password@/$srcdir/encrypted.aff $srcdir/encrypted.iso ) ; then
  echo Verifies with correct decryption passphrase.
else
  echo Does not verify with correct decryption passphrase.
  exit 1
fi


echo This next affcompare should generate an error:
if ( ../tools/affcompare file://:wrongphrase@/$srcdir/encrypted.aff $srcdir/encrypted.iso ) ; then
  echo Does not verify with correct decryption passphrase.
  exit 1
else
  echo Cannot verify with the wrong passphrase!  
fi

echo ALL TESTS PASS
exit 0




