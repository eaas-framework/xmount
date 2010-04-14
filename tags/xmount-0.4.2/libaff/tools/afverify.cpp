/*
 * afverify.cpp:
 *
 * Verify the digital signature on a signed file
 */

/*
 * Copyright (c) 2007
 *	Simson L. Garfinkel and Basis Technology, Inc. 
 *      All rights reserved.
 *
 * This code is derrived from software contributed by
 * Simson L. Garfinkel
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Simson L. Garfinkel
 *    and Basis Technology Corp.
 * 4. Neither the name of Simson Garfinkel, Basis Technology, or other
 *    contributors to this program may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIMSON GARFINKEL, BASIS TECHNOLOGY,
 * AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL SIMSON GARFINKEL, BAIS TECHNOLOGy,
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.  
 */

#include "affconfig.h"
#include "afflib.h"
#include "afflib_i.h"
#include "afflib_sha256.h"

#include "utils.h"
#include "base64.h"

#include "aff_bom.h"

#include <stdio.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include <openssl/pem.h>
#include <openssl/x509.h>


using namespace std;
using namespace aff;

const char *progname = "afcrypto";
int opt_change = 0;
int opt_verbose = 0;
int opt_all = 0;

void usage()
{
    printf("afverify version %s\n",PACKAGE_VERSION);
    printf("usage: afverify [options] filename.aff\n");
    printf("Verifies the digital signatures on a file\n");
    printf("options:\n");
    printf("    -a      --- print all segments\n");
    printf("    -V      --- Just print the version number and exit.\n");
    printf("    -v      --- verbose\n");
    exit(0);
}

void print_x509_info(X509 *cert)
{
    printf("SIGNING CERTIFICATE :\n");
    printf("   Subject: "); X509_NAME_print_ex_fp(stdout,X509_get_subject_name(cert),0,XN_FLAG_SEP_CPLUS_SPC);
    printf("\n");
    printf("   Issuer: "); X509_NAME_print_ex_fp(stdout,X509_get_issuer_name(cert),0,XN_FLAG_SEP_CPLUS_SPC);
    printf("\n");
    ASN1_INTEGER *sn = X509_get_serialNumber(cert);
    if(sn){
	long num = ASN1_INTEGER_get(sn);
	if(num>0) printf("   Certificate serial number: %d\n",num);
    }
    printf("\n");
}

#ifdef USE_AFFSIGS
#include "expat.h"
void startElement(void *userData, const char *name, const char **atts);
void endElement(void *userData, const char *name);
void cHandler(void *userData,const XML_Char *s,int len);

class segmenthash {
public:
    segmenthash():total_validated(0),total_invalid(0),sigmode(0),in_cert(false),
		 in_seghash(false),get_cdata(false),arg(0),seglen(0),
		 get_cdata_segment(0),af(0),cert(0),pubkey(0) {
	
	parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, ::startElement, ::endElement);
	XML_SetCharacterDataHandler(parser,cHandler);
    };
    int parse(const char *buf,int len) { return XML_Parse(parser, buf, len, 1);}
    XML_Parser parser;
    int total_validated;
    int total_invalid;
    int sigmode;
    bool in_cert;
    bool in_seghash;
    bool get_cdata;
    string segname;
    string alg;
    string cdata;
    int arg;
    int seglen;
    const char *get_cdata_segment;	// just get this segment
    AFFILE *af;				// if set, we are parsing crypto
    X509 *cert;				// public key used to sign
    EVP_PKEY *pubkey;
    void clear(){
	segname="";
	cdata="";
	sigmode=0;
	alg="";
	seglen=0;
    }
    ~segmenthash(){
	if(cert) X509_free(cert);
	if(parser) XML_ParserFree(parser);
    }
    void startElement(const char *name,const char **atts);
    void endElement(const char *name);
};

int count=0;
void startElement(void *userData, const char *name, const char **atts)
{
    segmenthash *sh = (segmenthash *)userData;
    sh->startElement(name,atts);
}

void segmenthash::startElement(const char *name,const char **atts)
{
    clear();
    if(strcmp(name,AF_XML_SEGMENT_HASH)==0){
	for(int i=0;atts[i];i+=2){
	    const char *name = atts[i];
	    const char *value = atts[i+1];
	    if(!strcmp(name,"segname")) segname = value;
	    if(!strcmp(name,"sigmode")) sigmode = atoi(value);
	    if(!strcmp(name,"alg")) alg = value;
	    if(!strcmp(name,"seglen")) seglen = atoi(value);
	}
	in_seghash = true;
	get_cdata = true;
	return;
    }
    if(strcmp(name,"signingcertificate")==0){
	in_cert = true;
	get_cdata = true;
	return;
    }
    if(get_cdata_segment && strcmp(name,get_cdata_segment)==0){
	get_cdata = true;
	return;
    }
}

void cHandler(void *userData,const XML_Char *s,int len)
{
    segmenthash *sh = (segmenthash *)userData;
    if(sh->get_cdata==false) return;	// don't want cdata
    sh->cdata.append(s,len);
}

void endElement(void *userData, const char *name)
{
    segmenthash *sh = (segmenthash *)userData;
    sh->endElement(name);
}


void segmenthash::endElement(const char *name)
{
    if(get_cdata_segment && strcmp(name,get_cdata_segment)==0){
	get_cdata = false;
	XML_StopParser(parser,0);
	return;
    }
    if(in_seghash && af){
	if(segname.size()==0) return;	// don't have a segment name
	/* Try to validate this one */
	size_t  hashbuf_len = cdata.size() + 2;
	u_char *hashbuf = (u_char *)malloc(hashbuf_len);
	hashbuf_len = b64_pton_slg((char *)cdata.c_str(),cdata.size(),hashbuf,hashbuf_len);
	if(alg=="sha256"){
	    /* TODO: Don't re-validate something that's already validated */
	    int r = af_hash_verify_seg2(af,segname.c_str(),hashbuf,hashbuf_len,sigmode);
	    if(r==AF_HASH_VERIFIES){
		total_validated++;
	    }
	    else total_invalid++;
	}
	free(hashbuf);
	in_seghash = false;
    }
    if(in_cert && af){
	BIO *cert_bio = BIO_new_mem_buf((char *)cdata.c_str(),cdata.size());
	PEM_read_bio_X509(cert_bio,&cert,0,0);
	BIO_free(cert_bio);
	pubkey = X509_get_pubkey(cert);
	in_cert = false;
    }
    cdata = "";				// erase it
}

string get_xml_field(const char *buf,const char *field) 
{
    segmenthash sh;
    sh.get_cdata_segment = field;
    sh.parse(buf,strlen(buf));
    return sh.cdata;
}

/* verify the chain signature; return 0 if successful, -1 if failed */
int  verify_bom_signature(AFFILE *af,const char *buf)
{
    const char *cce = "</" AF_XML_AFFBOM ">\n";
    char *chain_end = strstr(buf,cce);
    if(!chain_end){
	warn("end of chain XML can't be found\n");
	return -1;		// can't find it
    }
    char *sig_start = chain_end + strlen(cce);

    BIO *seg = BIO_new_mem_buf((void *)buf,strlen(buf));
    BIO_seek(seg,0);
    X509 *cert = 0;
    PEM_read_bio_X509(seg,&cert,0,0);	// get the contained x509 cert
    BIO_free(seg);

    /* Now get the binary signature */
    u_char sigbuf[1024];
    int sigbuf_len = b64_pton_slg(sig_start,strlen(sig_start),sigbuf,sizeof(sigbuf));
    if(sigbuf_len<80){
	warn("BOM is not signed");
	return -1;
    }

    /* Try to verify it */
    EVP_MD_CTX md;
    EVP_VerifyInit(&md,EVP_sha256());
    EVP_VerifyUpdate(&md,buf,sig_start-buf);
    int r = EVP_VerifyFinal(&md,sigbuf,sigbuf_len,X509_get_pubkey(cert));
    if(r!=1){ 
	printf("BAD SIGNATURE ON BOM\n");
	return -1;
    }
    
    print_x509_info(cert);
    printf("Date: %s\n",get_xml_field(buf,"date").c_str());
    printf("Notes: \n%s\n",get_xml_field(buf,"notes").c_str());
    
    sig_start[0] = 0; /* terminate the XML to remove the signature */
    segmenthash sh;

    sh.af = af;
    if (!sh.parse(buf, strlen(buf))){
	fprintf(stderr,
		"%s at line %d\n",
		XML_ErrorString(XML_GetErrorCode(sh.parser)),
		XML_GetCurrentLineNumber(sh.parser));
    }
    return 0;
}
#endif

int process(const char *fn)
{
    AFFILE *af = af_open(fn,O_RDONLY,0666);
    if(!af) af_err(1,fn);

    /* Get the public key */
    unsigned char certbuf[65536];
    size_t certbuf_len = sizeof(certbuf);
    if(af_get_seg(af,AF_SIGN256_CERT,0,certbuf,&certbuf_len)){
	/* See if it is present, but encrypted */
	if(af_get_seg(af,AF_SIGN256_CERT AF_AES256_SUFFIX,0,0,0)==0){
	    errx(1,"%s: signed file is encrypted; present decryption key to verify signature",fn);
	}
	errx(1,"%s: no signing certificate present. Cannot continue.",fn);
    }

    seglist segments(af);
    seglist no_sigs;
    seglist bad_sigs;
    seglist good_sigs;
    seglist unknown_errors;

    for(seglist::const_iterator seg = segments.begin();
	seg != segments.end();
	seg++){

	if(parse_chain(seg->name)>=0) continue; // chain of custody segments don't need signatures
	
	const char *segname = seg->name.c_str();
	int i =af_sig_verify_seg(af,segname);
	if(opt_verbose){
	    printf("af_sig_verify_seg(af,%s)=%d\n",segname,i);
	}
	switch(i){
	case AF_ERROR_SIG_NO_CERT:
	    err(1,"%s: no public key in AFF file\n",af_filename(af));
	case AF_ERROR_SIG_BAD:
	    bad_sigs.push_back(*seg);
	    break;
	case AF_ERROR_SIG_READ_ERROR:
	    no_sigs.push_back(*seg);
	    break;
	case AF_SIG_GOOD:
	    good_sigs.push_back(*seg);
	    break;
	case AF_ERROR_SIG_SIG_SEG:
	    break;			// can't verify the sig on a sig seg
	case AF_ERROR_SIG_NOT_COMPILED:
	    errx(1,"AFFLIB was compiled without signature support. Cannot continue.\n");
	default:
	    unknown_errors.push_back(*seg);
	    break;
	}
    }
    const char *prn = "";
    /* Tell us something about the certificate */
    BIO *cert_bio = BIO_new_mem_buf(certbuf,certbuf_len);
    X509 *cert = 0;
    PEM_read_bio_X509(cert_bio,&cert,0,0);
    if(!cert) errx(1,"Cannot decode certificate");
    printf("\n");
    printf("Filename: %s\n",fn);
    printf("# Segments signed and Verified:       %d\n",good_sigs.size());
    printf("# Segments unsigned:                  %d\n",no_sigs.size());
    printf("# Segments with corrupted signatures: %d\n",bad_sigs.size());
    printf("\n");
    print_x509_info(cert);

    int compromised = 0;
    for(seglist::const_iterator seg = good_sigs.begin(); seg != good_sigs.end() && opt_all;
	seg++){
	if(*seg==good_sigs.front()) printf("%sSegments with valid signatures:\n",prn);
	printf("\t%s\n",seg->name.c_str());
	prn = "\n";
    }
    for(seglist::const_iterator seg = no_sigs.begin();
	seg != no_sigs.end();
	seg++){
	if(*seg==no_sigs.front()) printf("%sUnsigned segments:\n",prn);
	printf("\t%s\n",seg->name.c_str());
	prn = "\n";
	compromised++;
    }
    for(seglist::const_iterator seg = bad_sigs.begin();
	seg != bad_sigs.end();
	seg++){
	if(*seg==bad_sigs.front()) printf("%sBad signature segments:\n",prn);
	printf("\t%s\n",seg->name.c_str());
	prn = "\n";
	compromised++;
    }
    for(seglist::const_iterator seg = unknown_errors.begin();
	seg != unknown_errors.end();
	seg++){
	if(*seg==unknown_errors.front()) printf("%sUnknown error segments:\n",prn);
	printf("\t%s\n",seg->name.c_str());
	prn = "\n";
	compromised++;
    }

    int highest = highest_chain(segments);
    printf("\nNumber of custody chains: %d\n",highest+1);
    for(int i=0;i<=highest;i++){
	/* Now print each one */
	printf("---------------------\n");
	printf("Signed Bill of Material #%d:\n\n",i+1);

	/* Get the segment and verify */
	size_t chainbuf_len = 0;
	char segname[AF_MAX_NAME_LEN];
	snprintf(segname,sizeof(segname),AF_BOM_SEG,i);
	if(af_get_seg(af,segname,0,0,&chainbuf_len)){
	    printf("*** BOM MISSING ***\n");
	    compromised++;
	}
	char *chainbuf = (char *)malloc(chainbuf_len+1);
	if(af_get_seg(af,segname,0,(u_char *)chainbuf,&chainbuf_len)){
	    printf("*** CANNOT READ BOM ***\n");
	    compromised++;
	}
		
	chainbuf[chainbuf_len]=0;	// terminate
#ifdef USE_AFFSIGS
	if(verify_bom_signature(af,chainbuf)){
	    printf("*** BOM SIGNATURE INVALID ***\n");
	    compromised++;
	}
#else
	printf("BOM signature cannot be verified beause libxpat is not available.\n");
#endif
    }
    printf("---------------------\n");
    af_close(af);
#ifdef USE_AFFSIGS
    if(compromised){
	printf("\nEVIDENCE FILE DOES NOT VERIFY.\nERRORS DETECTED: %d\n EVIDENTUARY VALUE MAY BE COMPROMISED.\n",compromised);
	return -1;
    }
    printf("\nEVIDENCE FILE VERIFIES.\n");
    return 0;
#endif
    printf("\n");
    return -1;
}


int main(int argc,char **argv)
{
    int bflag, ch;

    bflag = 0;
    while ((ch = getopt(argc, argv, "ach?vV")) != -1) {
	switch (ch) {
	case 'a': opt_all = 1;break;
	case 'c': opt_change = 1; break;
	case 'v': opt_verbose++;break;
	case 'h':
	case '?':
	default:
	    usage();
	    break;
	case 'V':
	    printf("%s version %s\n",progname,PACKAGE_VERSION);
	    exit(0);
	}
    }
    argc -= optind;
    argv += optind;

    if(argc!=1){
	usage();
    }

    return process(argv[0]);
}
