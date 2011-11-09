#!/bin/sh
# Test the afsegment command

export PATH=$srcdir:../tools:../../tools:.:$PATH
BLANK_BASE=`mktemp -t blankXXXXX`
BLANK_AFF=$BLANK_BASE.aff
unset AFFLIB_PASSPHRASE

echo === Putting a new metadata segment into blank.aff  ===

/bin/rm -f $BLANK_AFF
affcopy /dev/null $BLANK_AFF
affsegment -ssegname=testseg1 $BLANK_AFF
if [ x"testseg1" = x`affsegment -p segname $BLANK_AFF` ] ; then 
  echo affsegment worked!
else
  echo affsegment does not work properly
  exit 1
fi
/bin/rm -f $BLANK_AFF

