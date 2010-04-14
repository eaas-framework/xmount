/**
 ** AFF/libewf glue
 **
 ** (C) 2006 by Simson L. Garfinkel
 **
 **
 **/



#include "affconfig.h"
#include "afflib.h"
#include "afflib_i.h"

#ifdef USE_LIBEWF

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vnode_ewf.h"

#ifdef HAVE_CSTRING
#include <cstring>
#endif

/* We're gonna include libewf.h now, which causes problems, because libewf.h currently
 * includes an autoconf-generated affconfig.h file
 */

#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION

#ifdef HAVE_LIBEWF_H
#include "libewf.h"
#else
#error EWF support requires libewf, but HAVE_LIBEWF_H is not defined
#endif



/****************************************************************
 *** Service routines
 ****************************************************************/

#define EWF_HANDLE(af) ((libewf_handle_t *)af->vnodeprivate)

/****************************************************************
 *** AFFLIB Glue Follows
 ****************************************************************/


/* Return 1 if a file is a ewf file... */
static int ewf_identify_file(const char *filename,int exists)
{
    return libewf_check_file_signature(filename)==1 ? 1 : 0;
}

static int ewf_open(AFFILE *af)
{

    if(strchr(af->fname,'.')==0) return -1; // need a '.' in the filename

    /* See how many files there are to open */
    char **files = (char **)malloc(sizeof(char *));
    int nfiles = 1;
    files[0] = strdup(af->fname);

    char fname[MAXPATHLEN+1];
    strlcpy(fname,af->fname,sizeof(fname));
    char *ext = strrchr(fname,'.')+1;
    if(ext-fname > MAXPATHLEN-4){
	warn("ewf_open: %s: filename too long",af->fname);
	return -1;
    }

    /* Now open .E02 through .E99 and then .AAA through .ZZZ if they exist... */
    for(int i=2;i<=99;i++){
	sprintf(ext+1,"%02d",i);
	if(access(fname,R_OK)!=0) break;
	files = (char **)realloc(files,(nfiles+1) * sizeof(char *));
	files[nfiles] = strdup(fname);
	nfiles++;
    }
    for(int i=4*26*26;i<=26*26*26;i++){
        sprintf(ext, "%c%c%c", i/26/26%26+'A', i/26%26+'A', i%26+'A');
        if(access(fname,R_OK)!=0) break;  // file can't be read                                             
	files = (char **)realloc(files,(nfiles+1) * sizeof(char *));
	files[nfiles] = strdup(fname);
	nfiles++;
    }
    

    LIBEWF_HANDLE *handle = libewf_open( files, nfiles, LIBEWF_OPEN_READ );

    if(!handle){
	warn("Unable to open EWF image file");
	for(int i=0;i<nfiles;i++) free(files[i]);
	free(files);
	return -1;
    }
#if defined( HAVE_LIBEWF_GET_MEDIA_SIZE_ARGUMENT_VALUE ) || LIBEWF_VERSION>=20080501
    int r = libewf_get_media_size(handle,&af->image_size);
    if(r < 0){
	warn("EFW error: image size==0?");
	for(int i=0;i<nfiles;i++) free(files[i]);
	free(files);
	return -1;
    }
#else
     af->image_size = libewf_get_media_size(handle);
 
     if( af->image_size == 0 ){
 	warn("EFW error: image size==0?");
 	for(int i=0;i<nfiles;i++) free(files[i]);
 	free(files);
 	return -1;
     }
#endif

    af->vnodeprivate = (void *)handle;
#if defined( HAVE_LIBEWF_GET_CHUNK_SIZE_ARGUMENT_VALUE ) || LIBEWF_VERSION>=20080501
    libewf_get_chunk_size(handle,(size32_t *)&af->image_pagesize);
#else
     af->image_pagesize = libewf_get_chunk_size(handle);
#endif
    for(int i=0;i<nfiles;i++) free(files[i]);
    free(files);
    return 0;
}


static int ewf_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    LIBEWF_HANDLE *handle = EWF_HANDLE(af);
#if defined( HAVE_LIBEWF_GET_MEDIA_SIZE_ARGUMENT_VALUE ) || LIBEWF_VERSION>=20080501
    libewf_get_media_size(handle,(size64_t *)&vni->imagesize);
#else
    vni->imagesize = (int64_t) libewf_get_media_size(handle);
#endif

    vni->pagesize = 0;
#if defined( HAVE_LIBEWF_GET_CHUNK_SIZE_ARGUMENT_VALUE ) || LIBEWF_VERSION>=20080501
    libewf_get_chunk_size(handle,(size32_t *)&vni->pagesize);
#else
    vni->pagesize = libewf_get_chunk_size(handle);
#endif
    vni->supports_metadata = 1;
    vni->changable_pagesize = 0;
    vni->changable_sectorsize = 0;
    vni->supports_compression = 1;
    vni->has_pages = 1;			// debatable
    return 0;
}

static int ewf_read(AFFILE *af, unsigned char *buf, uint64_t pos,size_t count)
{
    LIBEWF_HANDLE *handle = EWF_HANDLE(af);
    return libewf_read_random(handle,buf,(uint64_t)count,pos);
}

static int ewf_write(AFFILE *af, unsigned char *buf, uint64_t pos,size_t count)
{
    LIBEWF_HANDLE *handle = EWF_HANDLE(af);
    return libewf_write_random(handle,buf,(uint64_t)count,pos);
}

static int ewf_close(AFFILE *af)
{
    LIBEWF_HANDLE *handle = EWF_HANDLE(af);
    if(libewf_close(handle)<0) return -1;
    return 0;
}


static int ewf_rewind_seg(AFFILE *af)
{
    af->cur_page = -1;			// starts at the metadata
    return 0;
}


/* return the length of a string up to a max */
static int strlenp(const unsigned char *data,int max)
{
    for(int i=0;i<max;i++){
	if(data[i]==0) return i;
    }
    return max;
}

static uint32_t ewf_bytes_per_sector(LIBEWF_HANDLE *handle)
{
    uint32_t bps = 0;
#if defined( HAVE_LIBEWF_GET_BYTES_PER_SECTOR_ARGUMENT_VALUE ) || LIBEWF_VERSION>=20080501
    libewf_get_bytes_per_sector(handle,&bps);
#else
    bps=libewf_get_bytes_per_sector(handle);
#endif
    return bps;
}

static int ewf_get_seg(AFFILE *af,const char *name, unsigned long *arg,
		       unsigned char *data,size_t *datalen)
{
    LIBEWF_HANDLE *handle = EWF_HANDLE(af);

    /* figure out chunksize */
    size32_t chunksize = 0;
#if defined( HAVE_LIBEWF_GET_CHUNK_SIZE_ARGUMENT_VALUE ) || LIBEWF_VERSION>=20080501
    libewf_get_chunk_size(handle,&chunksize);
#else
    chunksize=libewf_get_chunk_size(handle);
#endif

    /* Is the user asking for a page? */
    int64_t segnum = af_segname_page_number(name);
    if(segnum>=0){
	/* Get the segment number */
	if(data==0){
	    /* Need to make sure that the segment exists */
	    if(segnum*chunksize+chunksize > af->image_size ) return -1; // this segment does not exist
	    if(datalen) *datalen = chunksize;	// just return the chunk size
	    return 0;
	}
	size_t r = libewf_read_random(handle,data,*datalen,segnum * chunksize);
	return 0;				// should probably put in some error checking
    }

    /* See if it is a page name we understand */
    if(strcmp(name,AF_PAGESIZE)==0){
	if(arg) *arg = chunksize;
	return 0;
    }
    if(strcmp(name,AF_IMAGESIZE)==0){
	if(arg) *arg = 0;
	if(datalen==0 || *datalen==0) return 0;
	if(*datalen<8) return -2;
	
	struct aff_quad  q;
	q.low  = htonl((unsigned long)(af->image_size & 0xffffffff));
	q.high = htonl((unsigned long)(af->image_size >> 32));
	memcpy(data,&q,8);
	return 0;
    }
    if(strcmp(name,AF_SECTORSIZE)==0){
	if(arg) *arg=(unsigned long)ewf_bytes_per_sector(handle);
	if(datalen) *datalen = 0;
	return 0;
    }
    if(strcmp(name,AF_DEVICE_SECTORS)==0){
	/* Is this in flag or a quad word? */
	size64_t sz = 0;
	uint32_t bps = ewf_bytes_per_sector(handle);
	if(arg && bps>0) *arg = af->image_size / bps;
	if(datalen) *datalen = 0;
	return 0;
    }


    /* They are asking for a metdata segment. If we have wide character type
     * compiled in for libewf, just ignore it, because afflib doesn't do wide characters
     * at the moment...
     */

#if !defined(LIBEWF_WIDE_CHARACTER_TYPE) && defined(LIBEWF_VERSION) && (LIBEWF_VERSION >= 20080322)
    /* Can't guarentee character type in older versions of libewf */
    if(strcmp(name,AF_CASE_NUM)==0){
	if(data && datalen){
	    libewf_get_header_value_case_number(handle,(char *)data,*datalen); 
	    *datalen = strlenp(data,*datalen);
	    if(arg) *arg = 0;
	}
	return 0;
    }
    if(strcmp(name,AF_IMAGE_GID)==0){
	if(data && datalen){
	    libewf_get_guid(handle,data,*datalen);
	    if(arg) *arg = 0;
	}
	return 0;
    }
    if(strcmp(name,AF_ACQUISITION_NOTES)==0){
	if(data && datalen){
	    libewf_get_header_value_notes(handle,(char *)data,*datalen);
	    *datalen = strlenp(data,*datalen);
	}
	if(data==0 && datalen){
	    /* Caller wants to learn size of the notes */
	    unsigned char tmp[128];
	    memset(tmp,0,sizeof(tmp));
	    *datalen = sizeof(tmp);
	    libewf_get_header_value_notes(handle,(char *)tmp,*datalen);
	    *datalen = strlenp(tmp,*datalen);
	}
	if(arg) *arg = 0;
	return 0;
    }
#endif
    return -1;			// don't know this header
}

static const char *emap[] = {
    AF_PAGESIZE,
    AF_IMAGESIZE,
    AF_SECTORSIZE,
    AF_DEVICE_SECTORS,
    AF_CASE_NUM,
    AF_IMAGE_GID,
    AF_ACQUISITION_NOTES,
    0
};


static int ewf_get_next_seg(AFFILE *af,char *segname,size_t segname_len,unsigned long *arg,
			unsigned char *data,size_t *datalen)
{
    /* Figure out what the next segment would be, then get it */
    /* Metadata first */
    if(af->cur_page<0){
	/* Find out how many mapped segments there are */
	int mapped=0;
	for(mapped=0;emap[mapped];mapped++){
	}
	if(-af->cur_page >= mapped ){
	    af->cur_page = 0;
	    goto get_next_data_seg;
	}
	int which = 0 - af->cur_page;	// which one to get
	af->cur_page--;			// go to the next one
	if(segname_len < strlen(emap[which])) return -2; // not enough room for segname
	strlcpy(segname,emap[which],segname_len);	// give caller the name of the mapped segment.
	return ewf_get_seg(af,segname,arg,data,datalen);
    }

 get_next_data_seg:
    if(af->cur_page * af->image_pagesize >= af->image_size) return -1; // end of list
    /* Make the segment name */
    char pagename[AF_MAX_NAME_LEN];		//
    memset(pagename,0,sizeof(pagename));
    snprintf(pagename,sizeof(pagename),AF_PAGE,af->cur_page++);
    
    int r = 0;
    /* Get the segment, if it is wanted */
    if(data) r = ewf_get_seg(af,pagename,arg,data,datalen);
    
    /* If r==0 and there is room for copying in the segment name, return it */
    if(r==0){
	if(strlen(pagename)+1 < segname_len){
	    strlcpy(segname,pagename,segname_len);
	    return 0;
	}
	/* segname wasn't big enough */
	return -2;
    }
    return r;			// some other error
}

struct af_vnode vnode_ewf = {
    AF_IDENTIFY_EWF,
    AF_VNODE_TYPE_PRIMITIVE|AF_VNODE_NO_SIGNING|AF_VNODE_NO_SEALING,
    "LIBEWF",
    ewf_identify_file,
    ewf_open,
    ewf_close,
    ewf_vstat,
    ewf_get_seg,			// get seg
    ewf_get_next_seg,			// get_next_seg
    ewf_rewind_seg,			// rewind_seg
    0,					// update_seg
    0,					// del_seg
    ewf_read,				// read
    ewf_write				// write
};

#endif
