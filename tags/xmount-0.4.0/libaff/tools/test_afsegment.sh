#!/bin/sh
# Test the afsegment command

echo === Putting a new metadata segment into blank.aff  ===
./afcopy /dev/null blank.aff
./afsegment -ssegname=testseg1 blank.aff
if [ x"testseg1" = x`./afsegment -p segname blank.aff` ] ; then 
  echo afsegment worked!
else
  echo afsegment does not work properly
  exit 1
fi

