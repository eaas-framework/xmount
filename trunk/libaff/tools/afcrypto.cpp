/*
 * afcrypto.cpp:
 *
 * command for dealing with encryption issues
 */

/*
 * Copyright (c) 2007, 2008
 *	Simson L. Garfinkel 
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
 * 3. [omitted]
 * 4. Neither the name of Simson Garfinkel, Basis Technology, or other
 *    contributors to this program may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIMSON GARFINKEL
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
#include "utils.h"

#include <stdio.h>
#include <algorithm>
#include <vector>

const char *progname = "afcrypto";
#define DEFAULT_PASSPHRASE_FILE ".affpassphrase"
int opt_debug = 0;
char *opt_unsealing_private_key_file= 0;

void change_passphrase(const char *fn,const char *old_passphrase,const char *new_passphrase)
{
    int fail = 0;

    AFFILE *af = af_open(fn,O_RDWR,0666);
    if(!af) af_err(1,fn);
    if(af_change_aes_passphrase(af,old_passphrase,new_passphrase)){
	warnx("%s: af_change_aes_passphrase failed",fn);
	fail = 1;
    }
    af_close(af);
    if(!fail) printf("%s: passphrase changed.\n",fn);
}

void get_and_change_passphrase(const char *fn)
{
    char old_passphrase[1024];
    char new_passphrase[1024];

    memset(old_passphrase,0,sizeof(old_passphrase));
    memset(new_passphrase,0,sizeof(new_passphrase));

    printf("Enter old passphrase: ");
    fgets(old_passphrase,sizeof(old_passphrase),stdin);
    char *cc = strchr(old_passphrase,'\n');if(cc) *cc='\000';

    /* See if this passphrase works*/

    AFFILE *af = af_open(fn,O_RDONLY,0666);
    if(!af) af_err(1,fn);
    if(af_use_aes_passphrase(af,old_passphrase)){
	errx(1,"passphrase incorrect");
    }
    af_close(af);

    printf("Enter new passphrase: ");
    fgets(new_passphrase,sizeof(new_passphrase),stdin);
    cc = strchr(new_passphrase,'\n');if(cc) *cc='\000';
    change_passphrase(fn,old_passphrase,new_passphrase);
}

void usage()
{
    printf("afcrypto version %s\n",PACKAGE_VERSION);
    printf("usage: afcrypto [options] filename.aff [filename2.aff ... ]\n");
    printf("   prints if each file is encrypted or not.\n");
    printf("options:\n");
    printf("    -e      --- encrypt the unencrypted non-signature segments\n");
    printf("    -r      --- change passphrase (take old and new from stdin)\n");
    printf("    -O old  --- specify old passphrase\n");
    printf("    -N new  --- specify new passphrase\n");
    printf("    -K mykey.key  -- specifies a private keyfile for unsealing (may not be repeated)\n");
    printf("    -C mycert.crt -- specifies a certificate file for sealing (may be repeated)\n");
    printf("    -S      --- add symmetric encryptiong (passphrase) to AFFILE encrypted with public key\n");
    printf("                    (requires a private key and a specified passphrase).\n");
    printf("    -A      --- add asymmetric encryption to a AFFILE encrypted with a passphrase\n");
    printf("                    (requires a certificate file spcified with the -C option\n");
    

    printf("\nPassword Cracking Options:\n");
    printf("    -p passphrase --- checks to see if passphrase is the passphrase of the file\n");
    printf("                exit code is 0 if it is, -1 if it is not\n");
    printf("    -k      --- attempt to crack passwords by reading a list of passwords from ~/.affpassphrase\n");
    printf("    -f file --- Crack passwords but read them from file.\n");

    printf("\nDebugging:\n");
    printf("    -V      --- Just print the version number and exit.\n");
    printf("    -d      --- debug; print out each key as it is tried\n");
    printf("Note: This program ignores the environment variables:\n");
    puts(AFFLIB_PASSPHRASE);
    puts(AFFLIB_PASSPHRASE_FILE);
    puts(AFFLIB_PASSPHRASE_FD);
    puts(AFFLIB_DECRYPTING_PRIVATE_KEYFILE);
    exit(0);
}

/* Try each of the passphrases in the file against the passphrase. If it is found, return it. */
char  *check_file(AFFILE *af,const char *passphrase_file)
{
    char *ret = 0;
    FILE *f = fopen(passphrase_file,"r");
    if(!f) return 0;
    int t=0;

    char buf[1024];
    memset(buf,0,sizeof(buf));
    while(fgets(buf,sizeof(buf)-1,f)){
	char *cc = index(buf,'\n');
	if(cc) *cc = 0;
	if(opt_debug){
	    if(opt_debug) printf("checking with '%s' ... ",buf);
	    fflush(stdout);
	}
	int r= af_use_aes_passphrase(af,buf);
	if(r==0){
	    if(opt_debug) printf("YES!\n");
	    ret = strdup(buf);
	    break;
	}
    }
    fclose(f);
    return ret;
}

/**
 * Encrypts the non-signature segments that are not encrypted.
 * There is no reason to encrypt the signature segments.
 *
 * @param af - the AFFILE to open
 */

int af_encrypt_unencrypted_unsigned_segments(AFFILE *af)
{
    af_set_option(af,AF_OPTION_AUTO_DECRYPT,0);	// do not automatically decrypt
    aff::seglist sl(af);
    for(aff::seglist::const_iterator si = sl.begin();si!=sl.end();si++){
	if(si->name == AF_AFFKEY) continue; // don't encrypt the affkey!
	if(strstr(si->name.c_str(),"affkey_evp")) continue;
	if(!af_is_encrypted_segment(si->name.c_str()) &&
	   !af_is_signature_segment(si->name.c_str())){
	    /* Get the segment and put it, which will force the encryption to take place */
	    if(opt_debug) printf("  encrypting segment %s\n",si->name.c_str());
	    u_char *buf = (u_char *)malloc(si->len);
	    if(!buf) warn("Cannot encrypt segment '%s' --- too large (%d bytes) --- malloc failed",
			  si->name.c_str(),si->len);
	    else {
		unsigned long arg;
		size_t datalen = si->len;
		if(af_get_seg(af,si->name.c_str(),&arg,buf,&datalen)){
		    warn("Could not read segment '%s'",si->name.c_str());
		}
		else{
		    /* make sure that what we read is what we thought we were going to read */
		    assert(si->len==datalen);
		    assert(si->arg==arg);
		    if(af_update_seg(af,si->name.c_str(),arg,buf,datalen)){
			warn("Could not encrypt segment '%s'",si->name.c_str());
		    } else {
		    }
		}
		free(buf);
	    }
	} else {
	    if(opt_debug) printf("  already encrypted or signed: %s\n",si->name.c_str());
	}
    }
    af_set_option(af,AF_OPTION_AUTO_DECRYPT,1);	// go back to automatically decrypting
    return 0;
}

int main(int argc,char **argv)
{
    int bflag, ch;
    const char *old_passphrase=0;
    const char *new_passphrase=0;
    const char *check_passphrase = 0;
    char *passphrase_file = 0;
    const char *progname = argv[0];
    int opt_encrypt = 0;
    int opt_add_passphrase_to_public_key = 0;
    int opt_add_public_key_to_passphrase = 0;
    
    int mode = O_RDONLY;		// mode for opening AFF file
    const char **certificates = (const char **)malloc(0);
    int num_certificates = 0;

    /* Don't use auto-supplied passphrases */
    unsetenv(AFFLIB_PASSPHRASE);
    unsetenv(AFFLIB_PASSPHRASE_FILE);
    unsetenv(AFFLIB_PASSPHRASE_FD);
    unsetenv(AFFLIB_DECRYPTING_PRIVATE_KEYFILE);

    bflag = 0;
    int opt_change = 0;
    const char *home = getenv("HOME");
    while ((ch = getopt(argc, argv, "reC:SAO:N:p:f:kdh?VK:")) != -1) {
	switch (ch) {

	    /* These options make the mode read-write */
	case 'r': opt_change = 1; mode = O_RDWR; break;
	case 'e': opt_encrypt = 1; mode = O_RDWR; break;
	case 'S': opt_add_passphrase_to_public_key = 1; mode = O_RDWR; break;
	case 'A': opt_add_public_key_to_passphrase = 1; mode = O_RDWR; break;
	    /* These just set up variables */
	case 'C': 
	    certificates = (const char **)realloc(certificates,sizeof(int *)*(num_certificates+1));
	    certificates[num_certificates] = optarg;
	    num_certificates++;
	    break;
	case 'K': opt_unsealing_private_key_file = optarg;break;
	case 'O': old_passphrase = optarg;break;
	case 'N': new_passphrase = optarg;break;
	case 'p': check_passphrase = optarg;break;
	case 'f': passphrase_file = optarg;break;
	case 'k': 
	    if(!home) home = "/";
	    passphrase_file = (char *)malloc(strlen(home)+strlen(DEFAULT_PASSPHRASE_FILE)+2);
	    strcpy(passphrase_file,home);
	    strcat(passphrase_file,"/");
	    strcat(passphrase_file,DEFAULT_PASSPHRASE_FILE);
	    break;
	case 'd': opt_debug = 1;break;
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

    if(argc<1){
	usage();
    }

    if(num_certificates>0 && (opt_encrypt==0 && opt_add_public_key_to_passphrase==0)){
	errx(1,"Encryption certificates specified by -e option not set. "
	     "What do you want me to do with these certificates? ");
    }
	    

    if((check_passphrase || passphrase_file) && opt_encrypt){
	err(1,"Sorry, can't both encrypt and password crack. Pick one.\n");
    }

    if(opt_encrypt && (new_passphrase==0 && num_certificates==0)){
	err(1,"Currently -e requires that the passphrase be specified on the command line\nor that one or more encryption certificates be provided\n");
    }

    while(argc--){
	const char *fname = *argv++;

	if(opt_change){
	    if(old_passphrase && new_passphrase) change_passphrase(fname,old_passphrase,new_passphrase);
	    else get_and_change_passphrase(fname);
	}


	/* Get the information */
	AFFILE *af = af_open(fname,mode,0);
	if(!af) af_err(1,"af_open(%s)",fname);
	if(af_identify(af)!=AF_IDENTIFY_AFF && af_identify(af)!=AF_IDENTIFY_AFD){
	    errx(1,"Cannot encrypt %s: %s only supports AFF and AFD files.",af_filename(af),progname);
	}

	af_vnode_info vni;
	if(opt_encrypt && new_passphrase){
	    int r = af_establish_aes_passphrase(af,new_passphrase);
	    switch(r){
	    case AF_ERROR_NO_AES: errx(1,"AFFLIB compiled without AES; cannot continue");
	    case AF_ERROR_NO_SHA256: errx(1,"AFFLIB compiled without SHA256; cannot continue");
	    default: err(1,"%s: cannot establish passphrase (error %d)",fname,r);
	    case 0: 
	    case AF_ERROR_AFFKEY_EXISTS:
		/* no matter if we established it or if a phrase already exists, try to use it now */
		/* File already has a passphrase; see if this is it. */
		break;
	    }
	    r = af_use_aes_passphrase(af,new_passphrase);
	    switch(r){
	    case 0: break;		// everything okay
	    case AF_ERROR_WRONG_PASSPHRASE: errx(1,"%s: wrong passphrase",fname);
	    default: errx(1,"%s: passphrase already established (error %d)",fname,r);
	    }
        }

	if (opt_add_public_key_to_passphrase){ 
	  if(!num_certificates) errx(1,"You must specify a certificate with the -C option");
	  if(!check_passphrase) errx(1,"You must specify a passphrase with the -p option");
	  printf("Attepmting to add public key to AFFILE...\n");
	  if(af->crypto->sealing_key_set) return AF_ERROR_KEY_SET;		// already enabled
	  unsigned char affkey[32];
	  int r = af_get_aes_key_from_passphrase(af,check_passphrase,affkey);
	  if(r) errx(1, "%s: cannot get aes key.  Failed to add Public Key", fname);
	  af_seal_affkey_using_certificates(af, certificates, num_certificates, affkey);
	  printf("...Public key added successfully.\n");
	}

	if(opt_encrypt && num_certificates){
	    if(af_set_seal_certificates(af,certificates,num_certificates)){
		errx(1,"%s: can't set encryption certificate%s",fname,num_certificates==1 ? "" : "s");
	    }
	}
	if(opt_encrypt){
	    if(af_encrypt_unencrypted_unsigned_segments(af)){
		errx(1,"%s: can't encrypt unsigned, unencrypted segments",fname);
	    }
	}

	if(opt_add_passphrase_to_public_key) {
	    if(!new_passphrase) errx(1,"You must specify a new passphrase with the -N option");
	    printf("Attempting to add passphrase...\n");
	    u_char affkey[32];
	    if(af_get_affkey_using_keyfile(af, opt_unsealing_private_key_file,affkey)){
		errx(1,"%s: cannot unseal AFFKEY",fname);
	    }
	    if(af_save_aes_key_with_passphrase(af,new_passphrase,affkey)){
		af_err(1,"%s: could not set the passphrase",fname);
	    }
	    printf("... new passphrase established.\n");
	}


	if(af_vstat(af,&vni)) err(1,"%s: af_vstat failed: ",fname);
	const char *the_passphrase = 0;	// the correct passphrase


	/* were we supposed to try a check_passphrase? */
	if(check_passphrase){
	    if(af_use_aes_passphrase(af,check_passphrase)==0){
		the_passphrase = check_passphrase;
	    }
	    af_use_aes_passphrase(af,0); // clear the passphrase
	}

	/* Is a passphrase file provided? */
	if(!the_passphrase && passphrase_file){
	    the_passphrase = check_file(af,passphrase_file);
	    if(the_passphrase){
		af_use_aes_passphrase(af,0); // clear the passphrase
	    }
	}
	
	printf("%s: %5d segments; %5d signed; %5d encrypted; ",
	       fname,vni.segment_count_total,vni.segment_count_signed,vni.segment_count_encrypted);

	if(the_passphrase) printf("passphrase correct (\"%s\")",the_passphrase);
	putchar('\n');
	af_close(af);
    }
    return(0);
}
