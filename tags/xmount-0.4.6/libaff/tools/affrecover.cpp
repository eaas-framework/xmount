/*
 * afrecover.cpp
 *
 * Recover broken pages of an AFF file using the party bits
 */

/*
 * Copyright (c) 2005
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
#include "utils.h"

#include <ctype.h>

#include <zlib.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <assert.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_TERM_H
#include <term.h>
#endif

#ifdef HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#ifdef WIN32
#include "unix4win32.h"
#include <malloc.h>
#endif

using namespace std;
using namespace aff;

const char *progname = "affix";


int opt_b = 0;


void usage()
{
    printf("usage: %s filename\n",progname);
    exit(0);
}



int recover(const char *fname)
{
    AFFILE *af = af_open(fname,O_RDWR,0);
    if(!af) af_err(1,fname);

    /* Get the parity page */
    size_t pagesize = af_page_size(af);
    u_char *pagebuf    = (unsigned char *)calloc(pagesize,1);
    u_char *parity_buf = (unsigned char *)calloc(pagesize,1);
    u_char *my_parity_buf = (unsigned char *)calloc(pagesize,1);
    

    if(af_get_seg(af,AF_PARITY0,0,parity_buf,&pagesize)){
	err(1,"Cannot read %s segment; cannot continue",AF_PARITY0);
    }

    /* Now, for every page:
     * 1. Read the page & the signature
     * 2. If the signature is good, add it into the parity buffer.
     *    - If not, put it on the list of bad segments.
     */
    seglist segments(af);
    seglist bad_sigs;
    seglist good_sigs;
    for(seglist::const_iterator seg = segments.begin();
	seg != segments.end();
	seg++){

	if (seg->pagenumber()<0) continue; // only look for pages
	switch(af_sig_verify_seg(af,seg->name.c_str())){
	case AF_ERROR_SIG_NO_CERT:
	    errx(1,"%s: no public key in AFF file\n",af_filename(af));
	case AF_ERROR_SIG_READ_ERROR:
	    errx(1,"no signature for segment '%s' --- recovery cannot continue",seg->name.c_str());
	case AF_ERROR_SIG_BAD:
	    printf("%s has a bad signature\n",af_filename(af));
	    bad_sigs.push_back(*seg);
	    break;
	case AF_SIG_GOOD:
	    good_sigs.push_back(*seg);
	    /* While the page is in the cache, make our parity buf */
	    pagesize = af_page_size(af);
	    if(af_get_page(af,seg->pagenumber(),pagebuf,&pagesize)){
		err(1,"cannot read %s\n",seg->name.c_str());
	    }
	    for(u_int i=0;i<pagesize;i++){
		my_parity_buf[i] ^= pagebuf[i];
	    }
	    break;
	default:
	    break;
	}
    }
    u_char *new_pagebuf    = (unsigned char *)calloc(pagesize,1);

    if(bad_sigs.size()>1) errx(1,"This program can only repair 1 bad page at the moment.");
    if(bad_sigs.size()==0) errx(1,"There are no bad pages for this program to repair.");
    printf("Attempting to repair %s\n",bad_sigs[0].name.c_str());
	    
    /* Calculate the page buf */
    for(u_int i=0;i<pagesize;i++){
	new_pagebuf[i] = parity_buf[i] ^ my_parity_buf[i];
    }

    /* Write the page back */
    if(af_update_page(af,bad_sigs[0].pagenumber(),new_pagebuf,pagesize)){
	err(1,"Cannot put page back");
    }

    /* Now verify the signature */
    int r = af_sig_verify_seg(af,bad_sigs[0].name.c_str());
    if(r==AF_SIG_GOOD){
	printf("Page %s successfully repaired\n",bad_sigs[0].name.c_str());
    }
    else{
	printf("Page %s could not be repaired; signature error code=%d\n",bad_sigs[0].name.c_str(),r);
	exit(1);
    }
    af_close(af);
    return 0;
}



int main(int argc,char **argv)
{
    setvbuf(stdout,0,_IONBF,0);		// turn off buffering
    int ch;
    while ((ch = getopt(argc, argv, "bh?v")) != -1) {
	switch (ch) {
	case 'h':
	case '?':
	default:
	    usage();
	    break;
	case 'v':
	    printf("%s version %s\n",progname,PACKAGE_VERSION);
	    exit(0);
	    
	}
    }
    argc -= optind;
    argv += optind;

    if(argc<1){
	usage();
    }

    recover(argv[0]);
    exit(0);
}


