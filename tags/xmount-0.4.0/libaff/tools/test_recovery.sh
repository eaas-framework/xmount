#!/bin/sh
# 
# test the signing tools

/bin/rm -f recovery.key recovery.bak recovery.iso recovery.afm

echo ==== AFRECOVERY TEST ===
echo Make an X509 key


SUBJECT="/CN=Mr. Recovery/emailAddress=recovery@investiations.com"
openssl req -x509 -newkey rsa:1024 -keyout recovery.pem -out recovery.pem -nodes -subj "$SUBJECT"

PATH=$PATH:../tools:../../tools:.
test_make_random_iso.sh recovery.iso

cp recovery.iso recovery.bak
echo SIGNING RECOVERY.ISO 
if ! ./afsign -k recovery.pem recovery.iso ; then exit 1 ; fi
ls -l recovery.iso recovery.afm
echo VERIFYING SIGNATURE
if ! ./afverify recovery.afm ; then exit 1 ; fi
echo CORRUPTING FILE recovery.iso
dd if=/dev/random of=recovery.iso count=1 skip=1 conv=notrunc
echo ATTEMPTING RECOVERY
if ! ./afrecover recovery.afm ; then exit 1 ; fi
if ! ./afverify recovery.afm ; then exit 1 ; fi
echo MAKING SURE THAT THE MD5 HAS NOT CHANGED
if ! cmp recovery.bak recovery.iso ; then echo file changed ; exit 1 ; fi
echo ALL TESTS PASS
/bin/rm -f recovery.key recovery.bak recovery.iso recovery.afm recovery.pem
