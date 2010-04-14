#!/bin/sh
# 
# test the signing tools

/bin/rm -f agent.pem analyst.pem archives.pem evidence.aff evidence2.aff evidence3.aff
unset AFFLIB_PASSPHRASE

echo === MAKING THE TEST FILES ===

PATH=$PATH:../tools:../../tools:.
test_make_random_iso.sh rawevidence.iso

echo ==== AFSIGN TEST ===
echo Making X.509 keys

openssl req -x509 -newkey rsa:1024 -keyout agent.pem -out agent.pem -nodes -subj "/C=US/ST=California/L=Remote/O=Country Govt./OU=Sherif Dept/CN=Mr. Agent/emailAddress=agent@investiations.com"

 openssl req -x509 -newkey rsa:1024 -keyout analyst.pem -out analyst.pem -nodes -subj "/C=US/ST=California/L=Remote/O=State Police/OU=Forensics/CN=Ms. Analyst/emailAddress=analyst@investiations.com"
openssl req -x509 -newkey rsa:1024 -keyout archives.pem -out archives.pem -nodes -subj "/C=US/ST=CA/L=Remote/O=Archives/OU=Electronic/CN=Dr. Librarian/emailAddress=drbits@investiations.com"

echo Making an AFF file to sign
rm -f evidence.aff evidence?.aff
./afconvert -o evidence.aff rawevidence.iso 
echo Initial AFF file
if ! ./afinfo -a evidence.aff ; then exit 1 ; fi

echo Signing AFF file...
if ! ./afsign -k agent.pem evidence.aff ; then echo afsign failed ; exit 1 ; fi 
if ! ./afverify evidence.aff ; then echo afverify failed ; exit 1 ; fi ; 

echo Signature test 1 passed

echo Testing chain-of-custody signatures
echo Copying original raw file to evidence1.aff

if ! ./afcopy -z -k agent.pem rawevidence.iso evidence1.aff ; then exit 1; fi
if ! ./afinfo -a evidence1.aff ; then exit 1 ; fi
if ! ./afcompare rawevidence.iso evidence1.aff ; then exit 1 ; fi
if ! ./afverify evidence1.aff ; then exit 1 ; fi

echo
echo Making the second generation copy
echo "This copy was made by the analyst" | ./afcopy -z -k analyst.pem -n evidence1.aff evidence2.aff
if ! ./afinfo -a evidence2.aff ; then exit 1 ; fi
if ! ./afcompare rawevidence.iso evidence2.aff ; then exit 1 ; fi
if ! ./afverify evidence2.aff ; then exit 1 ; fi
echo
echo Making the third generation copy
echo "This copy was made by the archives" | ./afcopy -z -k archives.pem -n evidence2.aff evidence3.aff
if ! ./afinfo -a evidence3.aff ; then exit 1 ; fi
if ! ./afcompare rawevidence.iso evidence3.aff ; then exit 1 ; fi
if ! ./afverify evidence3.aff ; then exit 1 ; fi


echo All tests passed successfully
echo Erasing temporary files.
rm -f agent.pem archives.pem analyst.pem evidence.aff evidence.afm rawevidence.iso cevidence.iso evidence2.aff evidence3.aff evidence.aff
exit 0

