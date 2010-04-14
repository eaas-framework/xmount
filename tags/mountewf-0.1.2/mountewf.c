/*******************************************************************************
 * mountewf (c) 2008 by Gillen Daniel <Daniel.GILLEN@police.etat.lu>           *
 *                                                                             *
 * mountewf is a small tool to "fuse mount" ewf images as dd or vdi files      *
 *                                                                             *
 * This program is free software: you can redistribute it andrandor modify        *
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation, either version 3 of the License, or           *
 * (at your option) any later version.                                         *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.        *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <libewf.h>
#include <semaphore.h>
#include <endian.h>
#include <stdint.h>

#include "mountewf.h"

#define MOUNTEWF_VERSION "0.1.1"
#define IMAGE_INFO_HEADER "The following values have been extracted from " \
                          "mounted ewf file:\n\n\0"
#define VDI_FILE_COMMENT "<<< This is an emulated vdi image based upon an " \
                         "ewf image >>>\0"
#define VDI_HEADER_COMMENT "This VDI was emulated using mountewf v" \
                           MOUNTEWF_VERSION"\0"

static char *pImagePathDD="/image.dd";
static char *pImagePathVDI="/image.vdi";
static char *pImagePath;
static const char *pImageInfoPath="/image.info";
static LIBEWF_HANDLE *hEwfFile;
static char *pInfoFile=NULL;
static VDIFILEHEADER *pVdiFileHeader;
static uint32_t VdiFileHeaderSize=0;
static char *pVdiBlockMap;
static int VdiBlockMapSize=0;
// Command line ptions
static short int DEBUG=0;
static MOUNTMETHODS MOUNTMETHOD=MOUNT_AS_DD;
// Semaphores
static sem_t SEM_EWF_READ;
static sem_t SEM_VDIHEADER_READ;
static sem_t SEM_VDIBLOCKMAP_READ;
static sem_t SEM_INFO_READ;

/*
 * FUSE getattr implementation
 */
static int mountewf_getattr(const char *path,
                            struct stat *stbuf)
{
  int res=0;

  memset(stbuf,0,sizeof(struct stat));
  if(strcmp(path,"/")==0) {
    stbuf->st_mode=S_IFDIR | 0755;
    stbuf->st_nlink=2;
  } else if(strcmp(path,pImagePath)==0) {
    stbuf->st_mode=S_IFREG | 0444;
    stbuf->st_nlink=1;
    if(MOUNTMETHOD==MOUNT_AS_DD) {
      sem_wait(&SEM_EWF_READ);
      if(libewf_get_media_size(hEwfFile,&(stbuf->st_size))!=1) {
        res=-ENOENT;
      }
      sem_post(&SEM_EWF_READ);
    } else if(MOUNTMETHOD==MOUNT_AS_VDI) {
      if(libewf_get_media_size(hEwfFile,&(stbuf->st_size))!=1) {
        res=-ENOENT;
      }
      stbuf->st_size+=sizeof(VDIFILEHEADER);
      stbuf->st_size+=VdiBlockMapSize;
    }
  } else if(strcmp(path,pImageInfoPath)==0) {
    stbuf->st_mode=S_IFREG | 0444;
    stbuf->st_nlink=1;
    sem_wait(&SEM_INFO_READ);
    if(pInfoFile!=NULL) stbuf->st_size=strlen(pInfoFile);
    else stbuf->st_size=0;
    sem_post(&SEM_INFO_READ);
  } else res=-ENOENT;

  return res;
}

/*
 * FUSE readdir implementation
 */
static int mountewf_readdir(const char *path,
                            void *buf,
                            fuse_fill_dir_t filler,
                            off_t offset,
                            struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;

  if(strcmp(path,"/")!=0) return -ENOENT;
  filler(buf,".",NULL,0);
  filler(buf, "..",NULL,0);
  filler(buf,pImagePath+1,NULL,0);
  filler(buf,pImageInfoPath+1,NULL,0);

  return 0;
}

/*
 * FUSE open implementation
 */
static int mountewf_open(const char *path,
                         struct fuse_file_info *fi)
{
  if(strcmp(path,pImagePath)!=0 &&
     strcmp(path,pImageInfoPath)!=0)
  {
    return -ENOENT;
  }
  if((fi->flags & 3)!=O_RDONLY) return -EACCES;
  return 0;
}

/*
 * FUSE read implementation
 */
static int mountewf_read(const char *path,
                         char *buf,
                         size_t size,
                         off_t offset,
                         struct fuse_file_info *fi)
{
  size_t len;
  size64_t len2;
  ssize_t ssize;
  (void) fi;

  if(DEBUG!=0) {
    printf("DEBUG: Reading %u bytes from offset %" PRId64 "\n",
           size,
           offset);
  }

  if(strcmp(path,pImagePath)==0) {
    if(MOUNTMETHOD==MOUNT_AS_DD) {
      // Read data from emulated dd image
      if(libewf_get_media_size(hEwfFile,&len2)==1) {
        if(offset<len2) {
          if(offset+size>len2) size=len2-offset;
          if(DEBUG==1)
            printf("DEBUG: Raw read %zd bytes from offset %" PRId64 "\n",
                   size,offset);
          sem_wait(&SEM_EWF_READ);
          if(libewf_seek_offset(hEwfFile,offset)!=-1) {
            ssize=libewf_read_buffer(hEwfFile,buf,size);
            if(ssize==-1) {
              if(DEBUG==1) printf("DEBUG: Couldn't read data from ewf file!\n");
              size=0;
            } else size=ssize;
          } else size=0;

          // BUG: Seems as I have spotted a libewf bug!
          // The following code segfaults
          //if((ssize=libewf_read_random(hEwfFile,buf,size,offset))==-1) {
          //  if(DEBUG==1) printf("DEBUG: Couldn't read data from ewf file!\n");
          //  size=0;
          //} else size=ssize;

          sem_post(&SEM_EWF_READ);
        } else {
          if(DEBUG!=0) printf("DEBUG: Attempt to read past dd EOF\n");
          size=0;
        }
      } else {
        printf("ERROR: libewf_get_media_size failed!\n");
        size=0;
      }
    } else if(MOUNTMETHOD==MOUNT_AS_VDI) {
      // Read data from emulated vdi file
      if(libewf_get_media_size(hEwfFile,&len2)==1) {
        if(offset<(sizeof(VDIFILEHEADER)+VdiBlockMapSize+len2)) {
          off_t tmp_off=offset;
          size_t tmp_size=size;
          size_t ReadBytes=0;

          if(offset<VdiFileHeaderSize) {
            // Read data from vdi header
            if(offset+size>VdiFileHeaderSize) {
              // Adjust offset and size for reading later from ewf
              offset=VdiFileHeaderSize;
              tmp_size=VdiFileHeaderSize-tmp_off;
            }
            sem_wait(&SEM_VDIHEADER_READ);
            memcpy(buf,((char*)pVdiFileHeader)+tmp_off,tmp_size);
            sem_post(&SEM_VDIHEADER_READ);
            if(DEBUG!=0) printf("DEBUG: Read %u bytes from vdi header "
                                "at offset %" PRId64 "\n",tmp_size,tmp_off);
            size-=tmp_size;
            buf+=tmp_size;
            ReadBytes+=tmp_size;
          }
          if(size>0 && offset<VdiFileHeaderSize+len2) {
            tmp_off=offset;
            tmp_size=size;
            if(offset+size>VdiFileHeaderSize+len2) {
              // Read past EOF! Adjust size to end at EOF
              tmp_size=((VdiFileHeaderSize+len2)-VdiFileHeaderSize)-
                       (tmp_off-VdiFileHeaderSize);
            }
            sem_wait(&SEM_EWF_READ);
            if(libewf_seek_offset(hEwfFile,tmp_off-VdiFileHeaderSize)!=-1) {
              if((ssize=libewf_read_buffer(hEwfFile,buf,tmp_size))==-1) {
                printf("ERROR: Couldn't read data from ewf file!\n");
                ReadBytes=0;
              } else ReadBytes+=ssize;
            } else {
              printf("ERROR: Couldn't seek in ewf file!\n");
              ReadBytes=0;
            }
            sem_post(&SEM_EWF_READ);
            size-=tmp_size;
printf("DEBUGDEBUG: ewfsize=%lld\n",len2);
            if(DEBUG!=0) printf("DEBUG: Read %u bytes from ewf data "
                                "at offset %" PRId64 "\n",tmp_size,tmp_off-VdiFileHeaderSize);
          }
          size=ReadBytes;
        } else {
          if(DEBUG!=0) printf("DEBUG: Attempt to read past vdi EOF\n");
          size=0;
        }
      } else {
        printf("ERROR: libewf_get_media_size failed!\n");
        size=0;
      }
    } else {
      printf("ERROR: Unknown emulation method selected!\n");
      size=0;
    }
  } else if(strcmp(path,pImageInfoPath)==0) {
    // Read data from info file
    len=strlen(pInfoFile);
    if(offset<len) {
      if(offset+size>len) size=len-offset;
      sem_wait(&SEM_INFO_READ);
      memcpy(buf,pInfoFile+offset,size);
      sem_post(&SEM_INFO_READ);
    } else {
      if(DEBUG!=0) printf("DEBUG: Attempt to read past info EOF\n");
      size=0;
    }
  } else return -ENOENT;

  if(DEBUG!=0) printf("DEBUG: Read %u bytes\n",size);

  return size;
}

/*
 * Init VDI header
 */
int init_vdi_header() {
  // See http://forums.virtualbox.org/viewtopic.php?t=8046 for a
  // "description" of the various header fields

  uint64_t ewf_data_size;
  off64_t offset;
  uint32_t i,BlockEntries;

  // Get ewf data size
  if(libewf_get_media_size(hEwfFile,&ewf_data_size)!=1) {
    printf("ERROR: libewf_get_media_size failed!\n");
    return 1;
  }

  #define BLOCK_SIZE (1024*1024)
  BlockEntries=ewf_data_size/BLOCK_SIZE;
  if((ewf_data_size%BLOCK_SIZE)!=0) BlockEntries++;
  VdiBlockMapSize=BlockEntries*sizeof(uint32_t);
  if (DEBUG) {
    printf("BlockMap: %d (%08X) entries, %d (08X) bytes!\n", BlockEntries, BlockEntries, VdiBlockMapSize, VdiBlockMapSize);
  }

  //VdiBlockMapSize=4*(ewf_data_size/1048576);
  //if(ewf_data_size%1048576!=0) VdiBlockMapSize+=4;

  // Allocate memmory for vdi header and block map
  VdiFileHeaderSize=sizeof(VDIFILEHEADER)+VdiBlockMapSize;
  if((pVdiFileHeader=malloc(VdiFileHeaderSize))==NULL) {
    printf("ERROR: Couldn't allocate memmory for VDIFILEHEADER!\n");
    return 1;
  }
  memset(pVdiFileHeader,0x00,VdiFileHeaderSize);
  pVdiBlockMap=((void*)pVdiFileHeader)+sizeof(VDIFILEHEADER);

  strncpy(pVdiFileHeader->szFileInfo,VDI_FILE_COMMENT,
          strlen(VDI_FILE_COMMENT)+1);
  pVdiFileHeader->u32Signature=0xBEDA107F; // 1:1 copy from hp
  pVdiFileHeader->u32Version=0x00010001; // Vers 1.1
  pVdiFileHeader->cbHeader=0x00000180;  // No idea what this is for! Testimage had same value
  pVdiFileHeader->u32Type=0x00000002; // Type 2 (fixed size)
  pVdiFileHeader->fFlags=0;
  strncpy(pVdiFileHeader->szComment,VDI_HEADER_COMMENT,
          strlen(VDI_HEADER_COMMENT)+1);
  pVdiFileHeader->offData=VdiFileHeaderSize;
  pVdiFileHeader->offBlocks=sizeof(VDIFILEHEADER);
  pVdiFileHeader->cCylinders=0; // Legacy info
  pVdiFileHeader->cHeads=0; // Legacy info
  pVdiFileHeader->cSectors=0; // Legacy info
  pVdiFileHeader->cbSector=512; // Legacy info
  pVdiFileHeader->u32Dummy=0;
  pVdiFileHeader->cbDisk=ewf_data_size;
  // TODO: Calculate blocksize depending on image size
  // Seems that VBox is always using 1MB as blocksize so no calc is needed
  pVdiFileHeader->cbBlock=0x00100000;
  pVdiFileHeader->cbBlockExtra=0;
  pVdiFileHeader->cBlocks=BlockEntries;
  pVdiFileHeader->cBlocksAllocated=BlockEntries;
  // Just generate some random UUIDS
  // VBox won't accept immages where create and modify UUIDS aren't set
  *((int*)(&(pVdiFileHeader->uuidCreate_l)))=rand();
  *((int*)(&(pVdiFileHeader->uuidCreate_l))+4)=rand();
  *((int*)(&(pVdiFileHeader->uuidCreate_h)))=rand();
  *((int*)(&(pVdiFileHeader->uuidCreate_h))+4)=rand();
  *((int*)(&(pVdiFileHeader->uuidModify_l)))=rand();
  *((int*)(&(pVdiFileHeader->uuidModify_l))+4)=rand();
  *((int*)(&(pVdiFileHeader->uuidModify_h)))=rand();
  *((int*)(&(pVdiFileHeader->uuidModify_h))+4)=rand();

  // Generate block map
  i=0;
  for(offset=0;offset<VdiBlockMapSize;offset+=4) {
    *((uint32_t*)(pVdiBlockMap+offset))=i;
    i++;
  }

  if(DEBUG!=0) printf("DEBUG: VDI header size = %u\n",VdiFileHeaderSize);

  return 0;
}

/*
 * Parse command line options
 */
int parse_cmdline(const int argc,
                  char **argv,
                  int *pNargc,
                  char ***pppNargv,
                  int *pFilenameCount,
                  char ***pppFilenames,
                  char **ppMountpoint) {
  int i=1;
  int files=0,opts=0;

  // add argv[0] to pppNargv
  opts++;
  (*pppNargv)=(char**)realloc((*pppNargv),opts*sizeof(char*));
  if((*pppNargv)==NULL) {
    printf("ERROR: Couldn't allocate memmory for argv[0]!\n");
    return 1;
  }
  (*pppNargv)[opts-1]=(char*)malloc((strlen(argv[0])+1)*sizeof(char));
  if((*pppNargv)[opts-1]==NULL) {
    printf("ERROR: Couldn't allocate memmory for argv[0]!\n");
    return 1;
  }
  strncpy((*pppNargv)[opts-1],argv[0],strlen(argv[0])+1);

  // Parse options
  while(i<argc && *argv[i]=='-') {
    if(strlen(argv[i])>1 && *(argv[i]+1)!='-') {
      opts++;
      (*pppNargv)=(char**)realloc((*pppNargv),opts*sizeof(char*));
      if((*pppNargv)==NULL) {
        printf("ERROR: Couldn't allocate memmory for fuse options!\n");
        return 1;
      }
      (*pppNargv)[opts-1]=(char*)malloc((strlen(argv[i])+1)*sizeof(char));
      if((*pppNargv)[opts-1]==NULL) {
        printf("ERROR: Couldn't allocate memmory for fuse options!\n");
        return 1;
      }
      strncpy((*pppNargv)[opts-1],argv[i],strlen(argv[i])+1);
      // React too on fuse's debug flag (-d)
      if(strcmp(argv[i],"-d")==0) DEBUG=1;
    } else {
      // Options beginning with -- are mountewf specific
      if(strcmp(argv[i],"--vdi")==0) {
       MOUNTMETHOD=MOUNT_AS_VDI;
      } else {
        if(DEBUG!=0) printf("DEBUG: Unknown command line option \"%s\"\n",
                            argv[i]);
      }
    }
    i++;
  }

  // Parse EWF filenames
  while(i<(argc-1)) {
    files++;
    (*pppFilenames)=(char**)realloc((*pppFilenames),files*sizeof(char*));
    if((*pppFilenames)==NULL) {
      printf("ERROR: Couldn't allocate memmory for ewf filename(s)!\n");
      return 1;
    }
    (*pppFilenames)[files-1]=(char*)malloc((strlen(argv[i])+1)*sizeof(char));
    if((*pppFilenames)[files-1]==NULL) {
      printf("ERROR: Couldn't allocate memmory for ewf filename(s)!\n");
      return 1;
    }
    strncpy((*pppFilenames)[files-1],argv[i],strlen(argv[i])+1);
    i++;
  }
  *pFilenameCount=files;

  // Extract mountpoint
  if(argc>1) {
    (*ppMountpoint)=(char*)malloc((strlen(argv[argc-1])+1)*sizeof(char));
    if((*ppMountpoint)==NULL) {
      printf("ERROR: Couldn't allocate memmory for mountpoint string!\n");
      return 1;
    }
    strncpy(*ppMountpoint,argv[argc-1],strlen(argv[argc-1])+1);
    opts++;
    (*pppNargv)=(char**)realloc((*pppNargv),opts*sizeof(char*));
   if((*pppNargv)==NULL) {
      printf("ERROR: Couldn't allocate memmory for mountpoint string!\n");
      return 1;
    }
    (*pppNargv)[opts-1]=(char*)malloc((strlen(argv[i])+1)*sizeof(char));
    if((*pppNargv)[opts-1]==NULL) {
      printf("ERROR: Couldn't allocate memmory for mountpoint string!\n");
      return 1;
    }
    strncpy((*pppNargv)[opts-1],*ppMountpoint,strlen((*ppMountpoint))+1);
  }

  *pNargc=opts;

  return 0;
}

/*
 * Create virtual image.info file
 */
int init_image_info() {
  char buf[200];
  int ret;

#define M_CHECK_ALLOC { \
  if(pInfoFile==NULL) { \
    printf("ERROR: Couldn't allocate memmory!\n"); \
    return 1; \
  } \
}

  pInfoFile=(char*)realloc(pInfoFile,
                           (strlen(IMAGE_INFO_HEADER)+1)*sizeof(char));
  M_CHECK_ALLOC
  strncpy(pInfoFile,IMAGE_INFO_HEADER,strlen(IMAGE_INFO_HEADER)+1);

#define M_SAVE_VALUE(DESC) { \
  if(ret==1) {             \
    pInfoFile=(char*)realloc(pInfoFile, \
      (strlen(pInfoFile)+strlen(buf)+strlen(DESC)+2)*sizeof(char));\
    strncpy((pInfoFile+strlen(pInfoFile)),DESC,strlen(DESC)+1); \
    strncpy((pInfoFile+strlen(pInfoFile)),buf,strlen(buf)+1); \
    strncpy((pInfoFile+strlen(pInfoFile)),"\n\0",2); \
  } else if(ret==-1) return 1; \
}

  // Extract various infos from ewf file and add them to the virtual image.info
  // file content.
  ret=libewf_get_header_value_case_number(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Case number: \0")
  ret=libewf_get_header_value_description(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Description: \0")
  ret=libewf_get_header_value_examiner_name(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Examiner: \0")
  ret=libewf_get_header_value_evidence_number(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Evidence number: \0")
  ret=libewf_get_header_value_notes(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Notes: \0")
  ret=libewf_get_header_value_acquiry_date(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Acquiry date: \0")
  ret=libewf_get_header_value_system_date(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("System date: \0")
  ret=libewf_get_header_value_acquiry_operating_system(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Acquiry os: \0")
  ret=libewf_get_header_value_acquiry_software_version(hEwfFile,buf,sizeof(buf));
  M_SAVE_VALUE("Acquiry sw version: \0")

#undef M_CHECK_ALLOC
#undef M_SAVE_VALUE

  return 0;
}

/*
 * Struct containing implemented FUSE functions
 */
static struct fuse_operations mountewf_oper = {
  .getattr=mountewf_getattr,
  .readdir=mountewf_readdir,
  .open=mountewf_open,
  .read=mountewf_read,
};

/*
 * Main
 */
int main(int argc, char *argv[])
{
  char **ppEwfFilenames=NULL;
  int EwfFilenameCount=0;
  int nargc=0;
  char **ppNargv=NULL;
  char *pMountpoint=NULL;
  int ret=1;
  int i=0;

  // Parse and check command line options
  ret=parse_cmdline(argc,
                    argv,
                    &nargc,
                    &ppNargv,
                    &EwfFilenameCount,
                    &ppEwfFilenames,
                    &pMountpoint);
  if(ret!=0) {
    printf("ERROR: Couldn't parse command line parameters!\n");
    return 1;
  }
  // Basic checks
  if(nargc<2 || EwfFilenameCount==0 || pMountpoint==NULL) {
    printf("ERROR: Not enough command line parameters!\n");
    printf("Usage:\n");
    printf("  %s [[fuse options] [--vdi]] " \
           "<ewf-image.E?""?> <mountpoint>\n",argv[0]);
    return 1;
  }
  // Check for valid ewf files
  for(i=0;i<EwfFilenameCount;i++) {
    if(libewf_check_file_signature(ppEwfFilenames[i])!=1) {
      printf("ERROR: File \"%s\" isn't a valid ewf file!\n",
             ppEwfFilenames[i]);
      return 1;
    }
  }
  // TODO: Check if mountpoint is a valid dir

  if(DEBUG!=0) {
    if(EwfFilenameCount==1) {
      printf("* Extracting infos from ewf file \"%s\"...\n",
             ppEwfFilenames[0]);
    } else {
      printf("* Extracting infos from ewf glob \"%s .. %s\"...\n",
             ppEwfFilenames[0],
            ppEwfFilenames[EwfFilenameCount-1]);
    }
  }

  // Init random generator
  srand(time(NULL));

  // Open ewf file
  hEwfFile=libewf_open(ppEwfFilenames,
                       EwfFilenameCount,
                       libewf_get_flags_read());
  if(hEwfFile==NULL) {
    printf("ERROR: Couldn't open ewf file!\n");
    return 1;
  }

  // Parse EWF header
  if(libewf_parse_header_values(hEwfFile,0)!=1) {
    printf("ERROR: Couldn't parse ewf header values!\n");
    return 1;
  }

  // Gather infos for info file
  if(init_image_info()!=0) {
    printf("ERROR: Couldn't gather infos for image.info file!\n");
    return 1;
  }

  if(MOUNTMETHOD==MOUNT_AS_VDI) {
    // When mounting as VDI, we need to construct a vdi header
    if(init_vdi_header()!=0) {
      printf("ERROR: Couldn't initialize vdi header!\n");
      return 1;
    }
  }

  // Set emulated image name
  if(MOUNTMETHOD==MOUNT_AS_DD) pImagePath=pImagePathDD;
  else pImagePath=pImagePathVDI;

  // Init semaphores
  sem_init(&SEM_EWF_READ,0,1);
  sem_init(&SEM_VDIHEADER_READ,0,1);
  sem_init(&SEM_VDIBLOCKMAP_READ,0,1);
  sem_init(&SEM_INFO_READ,0,1);

  // Call fuse_main to do the fuse magic
  ret=fuse_main(nargc,ppNargv,&mountewf_oper,NULL);

  // TODO: Perhaps wait for unposted sem's
  sem_destroy(&SEM_INFO_READ);
  sem_destroy(&SEM_VDIBLOCKMAP_READ);
  sem_destroy(&SEM_VDIHEADER_READ);
  sem_destroy(&SEM_EWF_READ);

  libewf_close(hEwfFile);

  // Free allocated memmory
  if(MOUNTMETHOD==MOUNT_AS_VDI) {
    // Free constructed VDI headers
    free(pVdiFileHeader);
    //free(pVdiBlockMap);
  }
  for(i=0;i<EwfFilenameCount;i++) free(ppEwfFilenames[i]);
  free(ppEwfFilenames);
  for(i=0;i<nargc;i++) free(ppNargv[i]);
  free(ppNargv);

  return ret;
}

/*
  ----- Change history -----
  20090131: 0.1.0 released
            * Some minor things have still to be done.
            * Mounting ewf as dd: Seems to work. Diff didn't complain about
              changes betwenn original dd and emulated dd.
            * Mounting ewf as vdi: Seems to work too. VBox accepts the emulated
              vdi as valid vdi file and I was able to mount the containing fs
              under Debian. INFO: Debian freezed when not using mount -r !!
  20090203: 0.1.1 released
            * Fixed severe bug in BlockMap size calculation (Didn't check for
              odd input while converting from bytes to megabytes).
            * Improved vdi header allocation.
  20090210: 0.1.2 released
            * Fixed compilation problem (Typo in image_init_info() function).
            * Fixed some problems with the debian scripts to be able to build
              packages.
            * Added random generator initialisation (Makes it possible to use
              more than one image in VBox at a time).
*/
