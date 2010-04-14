#!/bin/sh
#
# test the passphrase tools

echo === MAKING THE TEST FILES ==
unset AFFLIB_PASSPHRASE
rm -f blank.iso blank.aff blanke.aff

PATH=$PATH:../tools:../../tools:.
test_make_random_iso.sh blank.iso

./afconvert -o blank.aff blank.iso
./afconvert -o file://:passphrase@/blanke.aff blank.iso

# Make sure afcrypto reports properly for with and with no encrypted segments
if (./afcrypto blank.aff | grep " 0 encrypted" > /dev/null ) ; then 
  echo blanke.aff properly created
else  
   echo ENCRYPTED SEGMENTS IN BLANKE.AFF --- STOP
   exit 1 
fi 

# Now test afcrypto
if (./afcrypto blanke.aff | grep -v " 0 encrypted" > /dev/null) ; then 
  echo blanke.aff properly created
else 
  echo NO ENCRYPTED SEGMENTS IN BLANKE.AFF --- STOP
  exit 1 
fi

echo "sleepy" > words
echo "dopey" >> words
echo "doc" >> words
echo "passphrase" >> words
echo "foobar" >> words
if [ "`./afcrypto -k -f words blanke.aff|grep correct|grep passphrase`"x == x ] ; then
  echo afcrypto did not find the right passphrase
  exit 1
else 
   echo afcrypto found the correct pasphrase 
fi

rm blank.iso blank.aff blanke.aff words

echo ALL TESTS PASS
exit 0
