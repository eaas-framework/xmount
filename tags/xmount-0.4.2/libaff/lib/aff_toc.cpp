/*
 * afflib_dir.cpp:
 * 
 * Functions for the manipulation of the AFF directories
 */

#include "affconfig.h"
#include "afflib.h"
#include "afflib_i.h"

int	aff_toc_free(AFFILE *af)
{
    if(af->toc){
	for(int i=0;i<af->toc_count;i++){
	    if(af->toc[i].name) free(af->toc[i].name);
	}
	free(af->toc);
	af->toc = 0;
	af->toc_count = 0;
    }
    return 0;
}


void	aff_toc_print(AFFILE *af)
{
    printf("AF DIRECTORY:\n");
    for(int i=0;i<af->toc_count;i++){
	if(af->toc[i].name){
	    printf("%-32s @%" I64u" len: % "I64u" \n",af->toc[i].name, af->toc[i].offset,af->toc[i].segment_len);
	}
    }
}

static int toc_sort(const void *a_,const void *b_)
{
    const aff_toc_mem *a = (const aff_toc_mem *)a_;
    const aff_toc_mem *b = (const aff_toc_mem *)b_;
    if(a->offset < b->offset) return -1;
    if(a->offset > b->offset) return 1;
    return 0;
}

static int aff_toc_append(AFFILE *af,const char *segname,uint64_t offset,uint64_t datalen)
{
    af->toc = (aff_toc_mem *)realloc(af->toc,sizeof(*af->toc)*(af->toc_count+1));
    if(af->toc==0){
	(*af->error_reporter)("realloc() failed in aff_toc_append. toc_count=%d\n",af->toc_count);
	return -1;
    }
    af->toc[af->toc_count].offset = offset;
    af->toc[af->toc_count].name = strdup(segname); // make a copy of the string
    af->toc[af->toc_count].segment_len = aff_segment_overhead(segname)+datalen;
    af->toc_count++;
    return 0;
}

/* Find an empty slot in the TOC in which to put the TOC.
 * Otherwise add it to the end.
 */
void aff_toc_update(AFFILE *af,const char *segname,uint64_t offset,uint64_t datalen)
{
    for(int i=0;i<af->toc_count;i++){
	if(af->toc[i].name==0 || strcmp(af->toc[i].name,segname)==0){
	    if(af->toc[i].name==0){	// if name was empty, copy it over
		af->toc[i].name = strdup(segname);
	    }
	    af->toc[i].offset  = offset;
	    af->toc[i].segment_len = aff_segment_overhead(segname)+datalen;
	    return;
	}
    }
    aff_toc_append(af,segname,offset,datalen);    /* Need to append it to the directory */
}

/*
 * aff_toc_build:
 * Build the directory by reading the existing file.
 * Notice that we know that we can simply append.
 */
int	aff_toc_build(AFFILE *af)	// build the dir if we couldn't find it
{
    aff_toc_free(af);			// clear the old one
    af_rewind_seg(af);			// start at the beginning

    // note: was malloc(0), but that causes problems under Borland    
    af->toc = (aff_toc_mem *)malloc(sizeof(aff_toc_mem));
    while(1){
	char segname[AF_MAX_NAME_LEN];
	size_t segname_len=sizeof(segname);
	off_t pos = ftello(af->aseg);
	size_t datalen=0;

	int r = af_get_next_seg(af,segname,segname_len,0,0,&datalen);
	switch(r){
	case AF_ERROR_NO_ERROR:
	    if(aff_toc_append(af,segname,pos,datalen)){
		return -1; // malloc error?
	    }
	    break;
	case AF_ERROR_EOF:
	    return 0;			// end of file; no errors
	default: /* unknown error */
	    fseeko(af->aseg,pos,SEEK_SET); // go back
	    return r;			// send up the error code
	}
    }
    return AF_ERROR_NO_ERROR;
}

/*
 * return the named entry in the directory
 */

struct aff_toc_mem *aff_toc(AFFILE *af,const char *segname)
{
    for(int i=0;i<af->toc_count;i++){
	if(af->toc[i].name && strcmp(af->toc[i].name,segname)==0) return &af->toc[i];
    }
    return 0;
}

/* Delete something from the directory, but don't bother reallocating.*/
int aff_toc_del(AFFILE *af,const char *segname)
{
    for(int i=0;i<af->toc_count;i++){
	if(af->toc[i].name && strcmp(af->toc[i].name,segname)==0){
	    free(af->toc[i].name);
	    af->toc[i].name=0;
	    return 0;			//  should only be in TOC once
	}
    }
    return -1;
}

