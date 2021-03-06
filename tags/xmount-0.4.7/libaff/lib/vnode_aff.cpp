/*
 * vnode_aff.cpp:
 * 
 * Functions for the manipulation of AFF files...
 */

#include "affconfig.h"
#include "afflib.h"
#include "afflib_i.h"
#include "vnode_aff.h"
#include "aff_db.h"

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#define xstr(s) str(s)
#define str(s) #s



static int      aff_write_ignore(AFFILE *af,size_t bytes);
static int	aff_write_seg(AFFILE *af,const char *name,unsigned long arg,
			      const u_char *value,size_t vallen); 
static int	aff_get_seg(AFFILE *af,const char *name,unsigned long *arg,
			    unsigned char *data,size_t *datalen);
static int	aff_get_next_seg(AFFILE *af,char *segname,size_t segname_len,
				 unsigned long *arg, unsigned char *data, size_t *datalen);

/** aff_segment_overhead:
 * @param segname - the name of a segment
 * @return The number of bytes in the AFF file that the segment takes up without the data.
 */

int aff_segment_overhead(const char *segname)
{
    return sizeof(struct af_segment_head)+sizeof(struct af_segment_tail)+(segname?strlen(segname):0);
}

static int aff_write_ignore2(AFFILE *af,size_t bytes)
{
    if(af_trace) fprintf(af_trace,"aff_write_ignore2(%p,%zd)\n",af,bytes);
    unsigned char *invalidate_data = (unsigned char *)calloc(bytes,1);
    aff_write_seg(af,AF_IGNORE,0,invalidate_data,bytes); // overwrite with NULLs
    free(invalidate_data);
    return 0;
}

static int aff_write_ignore(AFFILE *af,size_t bytes)
{
    int64_t startpos = ftello(af->aseg);	// remember start position
    int r = 0;

    if(af_trace) fprintf(af_trace,"aff_write_ignore(%p,%zd)\n",af,bytes);

    /* First write the ignore */
    r = aff_write_ignore2(af,bytes);

    /* If the next one is also an ignore,
     * then we should go back and make the ignore_size bigger.
     * We could do this recursively,
     * but it's probably not worth the added complexity.
     */
    char next[AF_MAX_NAME_LEN];
    size_t segsize2=0;
    int count=0;
    while(af_probe_next_seg(af,next,sizeof(next),0,0,&segsize2,1)==0 && next[0]==0 && segsize2>=0){
	count++;
	if(count>10) break;		// something is wrong; just get out.
	//printf("*** next %d segment at %qd len=%d will be deleted\n",count,ftello(af->aseg),segsize2);
	bytes += segsize2;
	fseeko(af->aseg,startpos,SEEK_SET);
	r = aff_write_ignore2(af,bytes);
	if(r!=0) return r;
    }

    /* See if the previous segment is also blank; if so, collapse them */
    fseeko(af->aseg,startpos,SEEK_SET);
    if(af_backspace(af)==0){
	uint64_t prev_segment_loc = ftello(af->aseg);	// remember where we are
	char   prev_segment_name[AF_MAX_NAME_LEN];
	size_t prev_segment_size=0;
	if(af_probe_next_seg(af,prev_segment_name,sizeof(prev_segment_name),0,0,&prev_segment_size,1)==0){
	    //printf("** prev segment name='%s' len=%d\n",prev_segment_name,prev_segment_size);
	    if(prev_segment_name[0]==0){
		bytes += prev_segment_size;
		fseeko(af->aseg,prev_segment_loc,SEEK_SET);
		r = aff_write_ignore2(af,bytes);
		fseeko(af->aseg,prev_segment_loc,SEEK_SET);
	    }
	}
    }

    return(r);
}


/* aff_write_seg:
 * put the given named segment at the current position in the file.
 * Return 0 for success, -1 for failure (probably disk full?)
 * This is the only place where a segment actually gets written
 */

int aff_write_seg(AFFILE *af, const char *segname,unsigned long arg,const u_char *data,size_t datalen)
{
    if(af_trace) fprintf(af_trace,"aff_write_seg(%p,%s,%lu,%p,len=%zu)\n",af,segname,arg,data,datalen);

    struct af_segment_head segh;
    struct af_segment_tail segt;

    if(af->debug){
      (*af->error_reporter)("aff_write_seg(" POINTER_FMT ",'%s',%lu,data=" POINTER_FMT ",datalen=%u)",
			    af,segname,arg,data,datalen);
    }

    assert(sizeof(segh)==16);
    assert(sizeof(segt)==8);

    /* If the last command was not a probe (so we know where we are), and
     * we are not at the end of the file, something is very wrong.
     */

    unsigned int segname_len = strlen(segname);

    strcpy(segh.magic,AF_SEGHEAD);
    segh.name_len = htonl(segname_len);
    segh.data_len = htonl(datalen);
    segh.flag      = htonl(arg);

    strcpy(segt.magic,AF_SEGTAIL);
    segt.segment_len = htonl(sizeof(segh)+segname_len + datalen + sizeof(segt));
    aff_toc_update(af,segname,ftello(af->aseg),datalen);

    
    if(af_trace) fprintf(af_trace,"aff_write_seg: putting segment %s (datalen=%zd) offset=%"PRId64"\n",
			 segname,datalen,ftello(af->aseg));

    if(fwrite(&segh,sizeof(segh),1,af->aseg)!=1) return -10;
    if(fwrite(segname,1,segname_len,af->aseg)!=segname_len) return -11;
    if(fwrite(data,1,datalen,af->aseg)!=datalen) return -12;
    if(fwrite(&segt,sizeof(segt),1,af->aseg)!=1) return -13;
    fflush(af->aseg);			// make sure it is on the disk
    return 0;
}


/****************************************************************
 *** low-level routines for reading 
 ****************************************************************/

/* aff_get_segment:
 * Get the named segment, using the toc cache.
 */

static int aff_get_seg(AFFILE *af,const char *name,
		       unsigned long *arg,unsigned char *data,size_t *datalen)
{
    if(af_trace) fprintf(af_trace,"aff_get_seg(%p,%s,arg=%p,data=%p,datalen=%p)\n",af,name,arg,data,datalen);

    char next[AF_MAX_NAME_LEN];

    /* If the segment is in the directory, then seek the file to that location.
     * Otherwise, we'll probe the next segment, and if it is not there,
     * we will rewind to the beginning and go to the end.
     */
    struct aff_toc_mem *adm = aff_toc(af,name);
    if(!adm) return -1;

    fseeko(af->aseg,adm->offset,SEEK_SET);
    int ret = aff_get_next_seg(af,next,sizeof(next),arg,data,datalen);
    assert(ret!=0 || strcmp(next,name)==0);	// hopefully this is what they asked for
    return ret;
}



/**
 * Get the next segment.
 * @param af          - The AFF file pointer
 * @param segname     - Array to hold the name of the segment.
 * @param segname_len - Available space in the segname array.
 * @param arg         - pointer to the arg
 * @param data        - pointer to the data
 * @param datalen_    - length of the data_ array. If *datalen_==0, set to the length of the data.
 * 
 * @return
 *    0 =  success.
 *  -1  = end of file. (AF_ERROR_EOF)
 *  -2  = *data is not large enough to hold the segment (AF_ERROR_DATASMALL)
 *  -3  = af file is corrupt; no tail (AF_ERROR_TAIL)
 */
static int aff_get_next_seg(AFFILE *af,char *segname,size_t segname_len,unsigned long *arg,
			unsigned char *data,size_t *datalen_)
{
    if(af_trace) fprintf(af_trace,"aff_get_next_seg()\n");
    if(!af->aseg){
	snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv only works with aff files");
	return AF_ERROR_INVALID_ARG;
    }

    uint64_t start = ftello(af->aseg);
    size_t data_len;

    int r = af_probe_next_seg(af,segname,segname_len,arg,&data_len,0,0);
    if(r<0) return r;			// propigate error code
    if(data){				/* Read the data? */
	if(datalen_ == 0){
	    snprintf(af->error_str,sizeof(af->error_str),"af_get_next_seg: data provided but datalen is NULL");
	    return AF_ERROR_INVALID_ARG;
	}
	size_t read_size = data_len<=*datalen_ ? data_len : *datalen_;

	if(fread(data,1,read_size,af->aseg)!=read_size){
	    snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv: EOF on reading segment? File is corrupt.");
	    return AF_ERROR_SEGH;
	}
	if(data_len > *datalen_){
	    /* Read was incomplete;
	     * go back to the beginning of the segment and return
	     * the incomplete code.
	     */
	    fseeko(af->aseg,start,SEEK_SET);	// go back
	    errno = E2BIG;
	    return AF_ERROR_DATASMALL;
	}
    } else {
	fseeko(af->aseg,data_len,SEEK_CUR); // skip past the data
    }
    if(datalen_) *datalen_ = data_len;

    /* Now read the tail */
    struct af_segment_tail segt;
    memset(&segt,0,sizeof(segt));	// zero before reading
    if(fread(&segt,sizeof(segt),1,af->aseg)!=1){
	snprintf(af->error_str,sizeof(af->error_str),
		 "af_get_next_segv: end of file reading segment tail; AFF file is truncated (AF_ERROR_TAIL)");
	return AF_ERROR_TAIL;
    }
    /* Validate tail */
    unsigned long stl = ntohl(segt.segment_len);
    unsigned long calculated_segment_len =
	sizeof(struct af_segment_head)
	+ strlen(segname)
	+ data_len + sizeof(struct af_segment_tail);

    if(strcmp(segt.magic,AF_SEGTAIL)!=0){
	snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv: AF file is truncated (AF_ERROR_TAIL).");
	fseeko(af->aseg,start,SEEK_SET); // go back to last good position
	return AF_ERROR_TAIL;
    }
    if(stl != calculated_segment_len){
	snprintf(af->error_str,sizeof(af->error_str),"af_get_next_segv: AF file corrupt (%lu!=%lu)/!",
		 stl,calculated_segment_len);
	fseeko(af->aseg,start,SEEK_SET); // go back to last good position
	return AF_ERROR_TAIL;
    }
    return 0;
}


static int aff_rewind_seg(AFFILE *af)
{
    if(af_trace) fprintf(af_trace,"aff_rewind_seg()\n");
    fseeko(af->aseg,sizeof(struct af_head),SEEK_SET); // go to the beginning
    return 0;
}


/* Removes the last segment of an AFF file if it is blank.
 * @return 0 for success, -1 for error */
int af_truncate_blank(AFFILE *af)
{
    uint64_t last_loc = ftello(af->aseg);	// remember where we are
    if(af_backspace(af)==0){
	uint64_t backspace_loc = ftello(af->aseg);	// remember where we are
	char   next_segment_name[AF_MAX_NAME_LEN];
	if(af_probe_next_seg(af,next_segment_name,sizeof(next_segment_name),0,0,0,1)==0){
	    if(next_segment_name[0]==0){
		/* Remove it */
		fflush(af->aseg);
		if(ftruncate(fileno(af->aseg),backspace_loc)<0) return -1;
		return 0;
	    }
	}
    }
    fseeko(af->aseg,last_loc,SEEK_SET);	// return to where we were
    return -1;				// say that we couldn't do it. 
}




/****************************************************************
 *** Update functions
 ****************************************************************/

/*
 * af_update_seg:
 * Update the given named segment with the new value.
 */

static int aff_update_seg(AFFILE *af, const char *name,
		    unsigned long arg,const u_char *value,unsigned int vallen)
{
    char   next_segment_name[AF_MAX_NAME_LEN];
    size_t next_segsize = 0;
    size_t next_datasize = 0;

    /* if we are updating with a different size,
     * remember the location and size of the AF_IGNORE segment that
     * has the smallest size that is >= strlen(name)+vallen
     */
    size_t size_needed = vallen+aff_segment_overhead(name);
    size_t size_closest = 0;
    uint64_t         loc_closest = 0;
    struct aff_toc_mem *adm = aff_toc(af,name);
       
    if(af_trace) fprintf(af_trace,"aff_update_seg(name=%s,arg=%lu,vallen=%u)\n",name,arg,vallen);


    if(adm){
	/* Segment is in the TOC; seek to it */
	fseeko(af->aseg,adm->offset,SEEK_SET);
    }
    else {
	/* Otherwise, go to the beginning of the file and try to find a suitable hole
	 * TK: This could be made significantly faster by just scanning the TOC for a hole.
	 */
	af_rewind_seg(af);			// start at the beginning
    }

    while(af_probe_next_seg(af,next_segment_name,sizeof(next_segment_name),0,&next_datasize,&next_segsize,1)==0){
	/* Remember this information */
	uint64_t next_segment_loc = ftello(af->aseg);
#ifdef DEBUG2
	fprintf(stderr,"  next_segment_name=%s next_datasize=%d next_segsize=%d next_segment_loc=%qd\n",
		next_segment_name, next_datasize, next_segsize,next_segment_loc);
#endif
	if(strcmp(next_segment_name,name)==0){	// found the segment
	    if(next_datasize == vallen){        // Does it exactly fit?
		int r = aff_write_seg(af,name,arg,value,vallen); // Yes, just write in place!
		return r;
	    }

	    //printf("** Segment '%s' doesn't fit at %qd; invalidating.\n",name,ftello(af->aseg));
	    aff_write_ignore(af,next_datasize+strlen(name));

	    /* If we are in random mode, jump back to the beginning of the file.
	     * This does a good job filling in the holes.
	     */
	    if(af->random_access){
		af_rewind_seg(af);
		continue;
	    }

	    /* Otherwise just go to the end. Experience has shown that sequential access
	     * tends not to generate holes.
	     */
	    fseeko(af->aseg,(uint64_t)0,SEEK_END);              // go to the end of the file
	    break;			// and exit this loop
	    
	}

	if((next_segment_name[0]==0) && (next_datasize>=size_needed)){
	    //printf("   >> %d byte blank\n",next_datasize);
	}

	/* If this is an AF_IGNORE, see if it is a close match */
	if((next_segment_name[0]==AF_IGNORE[0]) &&
	   (next_datasize>=size_needed) &&
	   ((next_datasize<size_closest || size_closest==0)) &&
	   ((next_datasize<1024 && size_needed<1024) || (next_datasize>=1024 && size_needed>=1024))){
	    size_closest = next_datasize;
	    loc_closest  = next_segment_loc;
	}
	fseeko(af->aseg,next_segsize,SEEK_CUR); // skip this segment
    }

    /* Ready to write */
    if(size_closest>0){
	/* Yes. Put it here and put a new AF_IGNORE in the space left-over
	 * TODO: If the following space is also an AF_IGNORE, then combine the two.
	 */
	//printf("*** Squeezing it in at %qd. name=%s. vallen=%d size_closest=%d\n",loc_closest,name,vallen,size_closest);

	fseeko(af->aseg,loc_closest,SEEK_SET); // move to the location
	aff_write_seg(af,name,arg,value,vallen); // write the new segment
	
	size_t newsize = size_closest - vallen - aff_segment_overhead(0) - strlen(name);
	aff_write_ignore(af,newsize); // write the smaller ignore
	return 0;
    }
    /* If we reach here we are positioned at the end of the file. */
    /* If the last segment is an ignore, truncate the file before writing */
    while(af_truncate_blank(af)==0){
	/* Keep truncating until there is nothing left */
    }
    //printf("*** appending '%s' bytes=%d to the end\n",name,vallen);
    fseeko(af->aseg,0L,SEEK_END);		// move back to the end of the file 
    return aff_write_seg(af,name,arg,value,vallen); // just write at the end
}



/* Delete the first occurance of the named segment.
 * Special case code: See if the segment being deleted
 * is the last segment. If it is, truncate the file...
 * This handles the case of AF_DIRECTORY and possibly other cases
 * as well...
 */

static int aff_del_seg(AFFILE *af,const char *segname)
{
    if(af_trace) fprintf(af_trace,"aff_del_seg(%p,%s)\n",af,segname);

    if(aff_toc_del(af,segname)){	// if del fails
	return 0;			// it's not present.
    }

    /* Find out if the last segment is the one we are deleting;
     * If so, we can just truncate the file.
     */
    char last_segname[AF_MAX_NAME_LEN];
    int64_t last_pos;
    af_last_seg(af,last_segname,sizeof(last_segname),&last_pos);
    if(strcmp(segname,last_segname)==0){
	fflush(af->aseg);		// flush any ouput
	if(ftruncate(fileno(af->aseg),last_pos)) return -1; // make the file shorter
	return 0;
    }

    size_t datasize=0,segsize=0;
    if(aff_find_seg(af,segname,0,&datasize,&segsize)!=0){
	return -1;			// nothing to delete?
    }
    /* Now wipe it out */
    size_t ignore_size = datasize+strlen(segname);
    aff_write_ignore(af,ignore_size);

    return 0;
}



#ifdef HAVE_OPENSSL_RAND_H
#include <openssl/rand.h>
#endif

/* aff_create:
 * af is an empty file that is being set up.
 */
static int aff_create(AFFILE *af)
{
    fwrite(AF_HEADER,1,8,af->aseg);  // writes the header 
    aff_toc_build(af);	             // build the toc (will be pretty small)
    af_make_badflag(af);	     // writes the flag for bad blocks
    
    const char *version = xstr(PACKAGE_VERSION);
    aff_update_seg(af,AF_AFFLIB_VERSION,0,(const u_char *)version,strlen(version));
    
#ifdef HAVE_GETPROGNAME
    const char *progname = getprogname();
    if(aff_update_seg(af,AF_CREATOR,0,(const u_char *)progname,strlen(progname))) return -1;
#endif
    if(aff_update_seg(af,AF_AFF_FILE_TYPE,0,(const u_char *)"AFF",3)) return -1;

    return 0;
}


/****************************************************************
 *** VNODE implementation functions
 ****************************************************************/

/* Return 1 if a file is an AFF file */
static int aff_identify_file(const char *filename,int exists)
{
    if(af_is_filestream(filename)==0) return 0; // not a file stream
    if(strncmp(filename,"file://",7)==0){
	/* Move file pointer past file:// then find a '/' and take the next character  */
	filename += 7;
	while(*filename && *filename!='/'){
	    filename++;
	}
	/* At this point if *filename==0 then we never found the end of the URL.
	 * return 0, since it's not an AFF file.
	 */
	if(*filename==0) return 0;

	/* So *filename must == '/' */
	assert(*filename == '/');
	filename++;
    }

    if(exists && access(filename,R_OK)!=0) return 0;	// needs to exist and it doesn't
    int fd = open(filename,O_RDONLY | O_BINARY);
    if(fd<0){
	/* File doesn't exist. Is this an AFF name? */
	if(af_ext_is(filename,"aff")) return 1;
	return 0;
    }
	
    if(fd>0){
	int len = strlen(AF_HEADER)+1;	
	char buf[64];
	int r = read(fd,buf,len);
	close(fd);
	if(r==len){			// if I could read the header
	    if(strcmp(buf,AF_HEADER)==0) return 1; // must be an AFF file
	    return 0;			// not an AFF file
	}
	/* If it is a zero-length file and the file extension ends AFF,
	 * then let it be an AFF file...
	 */
	if(r==0 && af_ext_is(filename,"aff")) return 1;
	return 0;			// must not be an aff file
    }
    return 0;
}



static int aff_open(AFFILE *af)
{
    if(af_is_filestream(af->fname)==0) return -1; // not a file stream

    /* Open the raw file */
    int fd = open(af->fname,af->openflags | O_BINARY,af->openmode);
    if(fd<0){				// couldn't open
	return -1;			
    }

    /* Lock the file if writing */
#ifdef HAVE_FLOCK
    if(af->openflags & O_RDWR){
	int lockmode = LOCK_SH;		// default
	if((af->openflags & O_ACCMODE)==O_RDWR) lockmode = LOCK_EX; // there can be only one
	if(flock(fd,lockmode)){
	    warn("Cannot exclusively lock %s:",af->fname);
	}
    }
#endif

    /* Set defaults */

    af->compression_type     = AF_COMPRESSION_ALG_ZLIB; 
    af->compression_level    = Z_DEFAULT_COMPRESSION;

    /* Open the FILE  for the AFFILE */
    char strflag[8];
    strcpy(strflag,"rb");		// we have to be able to read
    if(af->openflags & O_RDWR) 	strcpy(strflag,"w+b"); 

    af->aseg = fdopen(fd,strflag);
    if(!af->aseg){
      (*af->error_reporter)("fdopen(%d,%s)",fd,strflag);
      return -1;
    }

    /* Get file size */
    struct stat sb;
    if(fstat(fd,&sb)){
	(*af->error_reporter)("aff_open: fstat(%s): ",af->fname);	// this should not happen
	return -1;
    }

    /* If file is empty, then put out an AFF header, badflag, and AFF version */
    if(sb.st_size==0){
	return aff_create(af);
    }
    
    /* We are opening an existing file. Verify once more than it is an AFF file
     * and skip past the header...
     */

    char buf[8];
    if(fread(buf,sizeof(buf),1,af->aseg)!=1){
	/* Hm. End of file. That shouldn't happen here. */
	(*af->error_reporter)("aff_open: couldn't read AFF header on existing file?");
	return -1;			// should not happen
    }

    if(strcmp(buf,AF_HEADER)!=0){
	buf[7] = 0;
	(*af->error_reporter)("aff_open: %s is not an AFF file (header=%s)\n",
			      af->fname,buf);
	return -1;
    }

    /* File has been validated */
    if(aff_toc_build(af)) return -1;	// build the TOC
    return 0;				// everything must be okay.
}


/*
 * aff_close:
 * If the imagesize changed, write out a new value.
 */
static int aff_close(AFFILE *af)
{
    aff_toc_free(af);
    fclose(af->aseg);
    return 0;
}


static int aff_vstat(AFFILE *af,struct af_vnode_info *vni)
{
    memset(vni,0,sizeof(*vni));		// clear it
    vni->imagesize = af->image_size;	// we can just return this
    vni->pagesize = af->image_pagesize;
    vni->supports_compression = 1;
    vni->has_pages            = 1;
    vni->supports_metadata    = 1;
    vni->cannot_decrypt       = af_cannot_decrypt(af) ? 1 : 0;

    /* Check for an encrypted page */
    if(af->toc){
	for(int i=0;i<af->toc_count;i++){
	    if(af->toc[i].name){
		bool is_page = false;
		vni->segment_count_total++;
		if(af_segname_page_number(af->toc[i].name)>=0){
		    vni->page_count_total++;
		    is_page = true;
		}
		if(af_is_encrypted_segment(af->toc[i].name)){
		    vni->segment_count_encrypted++;
		    if(is_page) vni->page_count_encrypted++;
		}
		if(af_is_signature_segment(af->toc[i].name)){
		    vni->segment_count_signed++;
		}
	    }
	}
    }
    return 0;
}


struct af_vnode vnode_aff = {
    AF_IDENTIFY_AFF,
    AF_VNODE_TYPE_PRIMITIVE|AF_VNODE_TYPE_RELIABLE,
    "AFF",
    aff_identify_file,
    aff_open,
    aff_close,
    aff_vstat,
    aff_get_seg,
    aff_get_next_seg,
    aff_rewind_seg,
    aff_update_seg,
    aff_del_seg,
    0,				// read; keep 0
    0				// write
};

