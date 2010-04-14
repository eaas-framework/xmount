/*******************************************************************************
* xmount (c) 2009 by Gillen Daniel <gillen.dan@pinguin.lu>                     *
*                                                                              *
* xmount is a small tool to "fuse mount" various image formats as dd or vdi    *
* files and enable virtual write access.                                       *
*                                                                              *
* This program is free software: you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the Free   *
* Software Foundation, either version 3 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* This program is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* this program. If not, see <http://www.gnu.org/licenses/>.                    *
*******************************************************************************/

// TODO: Should be defined later by configure script in function to which libs
// it finds and what input image types are supported
#undef XMOUNT_SUPPORTS_AFF

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <semaphore.h>
#include <endian.h>
#include <stdint.h>
#ifdef HAVE_LIBEWF
  #include <libewf.h>
#endif
#ifdef XMOUNT_SUPPORTS_AFF
  //#include <libaff.h>
#endif
#include "xmount.h"

// Some constant values
#define IMAGE_INFO_HEADER "The following values have been extracted from " \
                          "the mounted image file:\n\n"
#define VDI_FILE_COMMENT "<<< This is a virtual VDI image >>>"
#define VDI_HEADER_COMMENT "This VDI was emulated using xmount v" \
                           PACKAGE_VERSION

// Struct that contains various runtime configuration options
static TXMountConfData XMountConfData;
// Handles for input image types
static FILE *hDdFile=NULL;
#ifdef HAVE_LIBEWF
  static LIBEWF_HANDLE *hEwfFile=NULL;
#endif
#ifdef XMOUNT_SUPPORTS_AFF
  //static AFILE *hAffFile=NULL;
#endif
// Pointer to virtual info file
static char *pVirtualImageInfoFile=NULL;
// Vars needed for VMDK emulation
static char *pVirtualVmdkFile=NULL;
// Vars needed for VDI emulation
static TVdiFileHeader *pVdiFileHeader=NULL;
static uint32_t VdiFileHeaderSize=0;
static char *pVdiBlockMap=NULL;
static uint32_t VdiBlockMapSize=0;
// Vars needed for virtual write access
static FILE *hCacheFile=NULL;
static pTCacheFileHeader pCacheFileHeader=NULL;
static pTCacheFileBlockIndex pCacheFileBlockIndex=NULL;
// Semaphores to control concurrent read & write access
static sem_t sem_img_read;
static sem_t sem_vdiheader_read;
static sem_t sem_info_read;
static sem_t sem_cachefile_rw;

/*
 * PrintUsage:
 *   Print usage instructions (cmdline options etc..)
 *
 * Params:
 *   pProgramName: Program name (argv[0])
 *
 * Returns:
 *   n/a
 */
static void PrintUsage(char *pProgramName) {
  printf("\nUsage:\n");
  printf("  %s [[fopts] [mopts]] <ifile> [<ifile> [...]] <mntp>\n\n",pProgramName);
  printf("Options:\n");
  printf("  fopts:\n");
  printf("    -d : Enable FUSE's debug mode. (Enables also xmount's debug mode)\n");
  printf("    -s : Run single threaded.\n");
  printf("  mopts:\n");
  printf("    --rw <cache_file> : Enable virtual write support and set cache file to use.\n");
  printf("    --in <type> : Specify input image type. Type can be \"dd\"");
#ifdef HAVE_LIBEWF
  printf(", \"ewf\"");
#endif
#ifdef XMOUNT_SUPPORTS_AFF
  printf(", \"aff\"");
#endif
  printf(".\n");
  printf("    --out <type> : Specify output image type. Type can be \"dd\" or \"vdi\".\n");
  printf("    Input and output image type defaults to \"dd\" if not specified.\n");
  printf("  ifile:\n");
  printf("    Input image file. If you use EWF files, you have to specify all image\n");
  printf("    segments!\n");
  printf("  mntp:\n");
  printf("    Mount point where virtual files should be located.\n");
}

/*
 * LogMessage:
 *   Print error and debug messages to stdout
 *
 * Params:
 *  pMessageType: "ERROR", "DEBUG" or "WARNING"
 *  pCallingFunction: Name of calling function
 *  line: Line number of call
 *  pMessage: Message string
 *  ...: Variable params with values to include in message string
 *
 * Returns:
 *   n/a
 */
static void LogMessage(char *pMessageType,
                       char *pCallingFunction,
                       int line,
                       char *pMessage,
                       ...)
{
  va_list VaList;

  // Print message "header"
  printf("%s: %s.%u : ",pMessageType,pCallingFunction,line);
  // Print message with variable parameters
  va_start(VaList,pMessage);
  vprintf(pMessage,VaList);
  va_end(VaList);
}

/*
 * ExtractVirtFileNames:
 *   Extract virtual file name from input image name
 *
 * Params:
 *   pOrigName: Name of input image (Can include a path)
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int ExtractVirtFileNames(char *pOrigName) {
  char *tmp;

  // Truncate any leading path
  tmp=strrchr(pOrigName,'/');
  if(tmp!=NULL) pOrigName=tmp+1;

  tmp=strrchr(pOrigName,'.');
  if(tmp==NULL) {
    // Input image filename has no extension
    // Alloc mem for leading '/', filename and extension (.dd or .vdi)
    XMountConfData.pVirtualImagePath=
      (char*)malloc((strlen(pOrigName)+6)*sizeof(char));
    XMountConfData.pVirtualImageInfoPath=
      (char*)malloc((strlen(pOrigName)+7)*sizeof(char));
  } else {
    // Extract base filename from input image filename
    // Alloc mem for leading '/', filename and extension (.dd or .vdi)
    XMountConfData.pVirtualImagePath=
      (char*)malloc(((strlen(pOrigName)-strlen(tmp))+6)*sizeof(char));
    XMountConfData.pVirtualImageInfoPath=
      (char*)malloc(((strlen(pOrigName)-strlen(tmp))+7)*sizeof(char));
  }
  if(XMountConfData.pVirtualImagePath==NULL ||
     XMountConfData.pVirtualImageInfoPath==NULL)
  {
    LOG_ERROR("Couldn't alloc memmory!\n")
    return FALSE;
  }
  // Set leading '/'
  (XMountConfData.pVirtualImagePath)[0]='/';
  (XMountConfData.pVirtualImageInfoPath)[0]='/';
  // Copy filename
  if(tmp==NULL) {
    strcpy(&(XMountConfData.pVirtualImagePath[1]),
            pOrigName);
    strcpy(&(XMountConfData.pVirtualImageInfoPath[1]),
            pOrigName);
    // Add file extensions
    switch(XMountConfData.VirtImageType) {
      case TVirtImageType_DD:
        strcpy(&(XMountConfData.pVirtualImagePath[strlen(pOrigName)+1]),
               ".dd");
        break;
      case TVirtImageType_VDI:
        strcpy(&(XMountConfData.pVirtualImagePath[strlen(pOrigName)+1]),
               ".vdi");
        break;
      default:
        LOG_ERROR("Unknown virtual image type!\n")
        return FALSE;
    }
    strcpy(&(XMountConfData.pVirtualImageInfoPath[strlen(pOrigName)+1]),
               ".info");
  } else {
    strncpy(&(XMountConfData.pVirtualImagePath[1]),
            pOrigName,
            strlen(pOrigName)-strlen(tmp));
    strncpy(&(XMountConfData.pVirtualImageInfoPath[1]),
            pOrigName,
            strlen(pOrigName)-strlen(tmp));
    // Add file extensions
    switch(XMountConfData.VirtImageType) {
      case TVirtImageType_DD:
        strcpy(&(XMountConfData.pVirtualImagePath[(strlen(pOrigName)-strlen(tmp))+1]),
               ".dd");
        break;
      case TVirtImageType_VDI:
        strcpy(&(XMountConfData.pVirtualImagePath[(strlen(pOrigName)-strlen(tmp))+1]),
               ".vdi");
        break;
      default:
        LOG_ERROR("Unknown virtual image type!\n")
        return FALSE;
    }
    strcpy(&(XMountConfData.pVirtualImageInfoPath[(strlen(pOrigName)-strlen(tmp))+1]),
           ".info");
  }
  LOG_DEBUG("Set virtual image name to \"%s\"\n",
            XMountConfData.pVirtualImagePath)
  LOG_DEBUG("Set virtual image info name to \"%s\"\n",
            XMountConfData.pVirtualImageInfoPath)
  return TRUE;
}

/*
 * GetOrigImageSize:
 *   Get size of original image
 *
 * Params:
 *   size: Pointer to an uint64_t to which the size will be written to
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int GetOrigImageSize(uint64_t *size) {
  // Make sure to return correct values when dealing with only 32bit file sizes
  *size=0;

  // When size was already queryed, use old value rather than regetting value
  // from disk
  //if(XMountConfData.OrigImageSize!=0) return XMountConfData.OrigImageSize;

  // Now get size of original image
  switch(XMountConfData.OrigImageType) {
    case TOrigImageType_DD:
      // Original image is a DD file. Seek to end to get size.
      sem_wait(&sem_img_read);
      if(fseeko(hDdFile,0,SEEK_END)!=0) {
        LOG_ERROR("Couldn't seek to end of image file!\n")
        sem_post(&sem_img_read);
        return FALSE;
      }
      *size=ftello(hDdFile);
      sem_post(&sem_img_read);
      break;
#ifdef HAVE_LIBEWF
    case TOrigImageType_EWF:
      // Original image is an EWF file. Just query media size.
      sem_wait(&sem_img_read);
      if(libewf_get_media_size(hEwfFile,size)!=1) {
        LOG_ERROR("Couldn't get ewf media size!\n")
        sem_post(&sem_img_read);
        return FALSE;
      }
      sem_post(&sem_img_read);
      break;
#endif
#ifdef XMOUNT_SUPPORTS_AFF
    case TOrigImageType_AFF:
      // Original image is an AFF file.
      // TODO: Implement AFF image type handling
      LOG_ERROR("AFF image type handling not implemented!\n")
      return FALSE;
      break;
#endif
    default:
      LOG_ERROR("Unsupported image type!\n")
      return FALSE;
  }
  // Save size so we have not to reget it from disk next time
  XMountConfData.OrigImageSize=*size;
  return TRUE;
}

/*
 * GetVirtImageSize:
 *   Get size of the emulated image
 *
 * Params:
 *   size: Pointer to an uint64_t to which the size will be written to
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int GetVirtImageSize(uint64_t *size) {
  //if(XMountConfData.VirtImageSize!=0) return XMountConfData.VirtImageSize;

  switch(XMountConfData.VirtImageType) {
    case TVirtImageType_DD:
      // Virtual image is a DD file. Just return the size of the original image
      if(!GetOrigImageSize(size)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      break;
    case TVirtImageType_VDI:
      // Virtual image is a VDI file. Get size of original image and add size
      // of VDI header etc.
      if(!GetOrigImageSize(size)) {
        LOG_ERROR("Couldn't get size of input image!\n")
        return FALSE;
      }
      (*size)+=(sizeof(TVdiFileHeader)+VdiBlockMapSize);
      break;
    default:
      LOG_ERROR("Unsupported image type!\n")
      return FALSE;
  }
  XMountConfData.VirtImageSize=*size;
  return TRUE;
}

/*
 * GetOrigImageData:
 *   Read data from original image
 *
 * Params:
 *   buf: Pointer to buffer to write read data to (Must be preallocated!)
 *   offset: Offset at which data should be read
 *   size: Size of data which should be read (Size of buffer)
 *
 * Returns:
 *   Number of read bytes on success or "-1" on error
 */
static int GetOrigImageData(char *buf, off_t offset, size_t size) {
  size_t ToRead=0;
  uint64_t ImageSize=0;

  // Make sure we aren't reading past EOF of image file
  if(!GetOrigImageSize(&ImageSize)) {
    LOG_ERROR("Couldn't get image size!\n")
    return -1;
  }
  if(offset>=ImageSize) {
    // Offset is beyond image size
    LOG_DEBUG("Offset is beyond image size.\n")
    return 0;
  }
  if(offset+size>ImageSize) {
    // Attempt to read data past EOF of image file
    ToRead=ImageSize-offset;
    LOG_DEBUG("Attempt to read data past EOF. Corrected size from %zd"
              " to %zd.\n",size,ToRead)
  } else ToRead=size;

  // Now read data from image file
  switch(XMountConfData.OrigImageType) {
    case TOrigImageType_DD:
      // Original image is a DD file. Seek to offset and read ToRead bytes.
      sem_wait(&sem_img_read);
      if(fseeko(hDdFile,offset,SEEK_SET)!=0) {
        LOG_ERROR("Couldn't seek to offset %" PRIu64 "!\n",offset)
        sem_post(&sem_img_read);
        return -1;
      }
      if(fread(buf,ToRead,1,hDdFile)!=1) {
        LOG_ERROR("Couldn't read %zd bytes from offset %" PRIu64
                  "!\n",ToRead,offset)
        sem_post(&sem_img_read);
        return -1;
      }
      sem_post(&sem_img_read);
      LOG_DEBUG("Read %zd bytes at offset %" PRIu64 " from DD file\n",
                ToRead,offset)
      break;
#ifdef HAVE_LIBEWF
    case TOrigImageType_EWF:
      // Original image is an EWF file. Seek to offset and read ToRead bytes.
      sem_wait(&sem_img_read);
      if(libewf_seek_offset(hEwfFile,offset)!=-1) {
        if(libewf_read_buffer(hEwfFile,buf,ToRead)!=ToRead) {
          LOG_ERROR("Couldn't read %zd bytes from offset %" PRIu64
                    "!\n",ToRead,offset)
          sem_post(&sem_img_read);
          return -1;
        }
      } else {
        LOG_ERROR("Couldn't seek to offset %" PRIu64 "!\n",offset)
        sem_post(&sem_img_read);
        return -1;
      }
      sem_post(&sem_img_read);
      LOG_DEBUG("Read %zd bytes at offset %" PRIu64 " from EWF file\n",
                ToRead,offset)
      break;
#endif
#ifdef XMOUNT_SUPPORTS_AFF
    case TOrigImageType_AFF:
      // Original image is an AFF file.
      // TODO: Implement AFF image type handling
      LOG_ERROR("AFF image type handling not implemented!\n")
      return -1;
      break;
#endif
    default:
      LOG_ERROR("Unsupported image type!\n")
      return -1;
  }
  return ToRead;
}

/*
 * GetVirtImageData:
 *   Read data from virtual image
 *
 * Params:
 *   buf: Pointer to buffer to write read data to (Must be preallocated!)
 *   offset: Offset at which data should be read
 *   size: Size of data which should be read (Size of buffer)
 *
 * Returns:
 *   Number of read bytes on success or "-1" on error
 */
static int GetVirtImageData(char *buf, off_t offset, size_t size) {
  uint32_t CurBlock=0;
  uint64_t VirtImageSize;
  size_t ToRead=0;
  size_t CurToRead=0;
  int64_t ret=0;
  off_t FileOff=offset;
  off_t BlockOff=0;

  // Get virtual image size
  if(!GetVirtImageSize(&VirtImageSize)) {
    LOG_ERROR("Couldn't get virtual image size!\n")
    return -1;
  }

  if(offset>=VirtImageSize) {
    LOG_ERROR("Attempt to read beyond virtual image EOF!\n")
    return 0;
  }

  if(offset+size>VirtImageSize) {
    LOG_DEBUG("Attempt to read pas EOF of virtual image file\n")
    size=VirtImageSize-offset;
  }

  ToRead=size;

  // Read virtual image type specific data
  switch(XMountConfData.VirtImageType) {
    case TVirtImageType_VDI:
      if(FileOff<VdiFileHeaderSize) {
        if(FileOff+ToRead>VdiFileHeaderSize) CurToRead=VdiFileHeaderSize-FileOff;
        else CurToRead=ToRead;
        if(XMountConfData.Writable==1 &&
           pCacheFileHeader->VdiFileHeaderCached==1)
        {
          // VDI header was already cached
          sem_wait(&sem_cachefile_rw);
          if(fseeko(hCacheFile,
                    pCacheFileHeader->pVdiFileHeader+FileOff,
                    SEEK_SET)!=0)
          {
            LOG_ERROR("Couldn't seek to cached VDI header at offset %"
                      PRIu64 "\n",pCacheFileHeader->pVdiFileHeader+FileOff)
            sem_post(&sem_cachefile_rw);
            return 0;
          }
          if(fread(buf,CurToRead,1,hCacheFile)!=1) {
            LOG_ERROR("Couldn't read %zu bytes from cache file at offset %"
                      PRIu64 "\n",CurToRead,
                      pCacheFileHeader->pVdiFileHeader+FileOff)
            sem_post(&sem_cachefile_rw);
            return 0;
          }
          sem_post(&sem_cachefile_rw);
          LOG_DEBUG("Read %zd bytes from cached VDI header at offset %"
                    PRIu64 " at cache file offset %" PRIu64 "\n",
                    CurToRead,FileOff,
                    pCacheFileHeader->pVdiFileHeader+FileOff)
        } else {
          // VDI header isn't cached
          sem_wait(&sem_vdiheader_read);
          memcpy(buf,((char*)pVdiFileHeader)+FileOff,CurToRead);
          sem_post(&sem_vdiheader_read);
          LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                    " from virtual VDI header\n",CurToRead,
                    FileOff)
        }
        if(ToRead==CurToRead) return ToRead;
        else {
          // Adjust values to read from original image
          ToRead-=CurToRead;
          buf+=CurToRead;
          FileOff=0;
        }
      } else FileOff-=VdiFileHeaderSize;
      break;
  }

  // Calculate block to read data from
  CurBlock=FileOff/CACHE_BLOCK_SIZE;
  BlockOff=FileOff%CACHE_BLOCK_SIZE;
  
  // Read data from original data
  while(ToRead!=0) {
    // Calculate how many bytes we have to read from this block
    if(BlockOff+ToRead>CACHE_BLOCK_SIZE) {
      CurToRead=CACHE_BLOCK_SIZE-BlockOff;
    } else CurToRead=ToRead;
    if(XMountConfData.Writable &&
       pCacheFileBlockIndex[CurBlock].Assigned==1)
    {
      // Write support enabled and need to read altered data from cachefile
      sem_wait(&sem_cachefile_rw);
      if(fseeko(hCacheFile,
                pCacheFileBlockIndex[CurBlock].off_data+BlockOff,
                SEEK_SET)!=0)
      {
        LOG_ERROR("Couldn't seek to offset %" PRIu64
                  " in cache file\n")
        sem_post(&sem_cachefile_rw);
        return -1;
      }
      if(fread(buf,CurToRead,1,hCacheFile)!=1) {
        LOG_ERROR("Couldn't read data from cache file!\n")
        sem_post(&sem_cachefile_rw);
        return -1;
      }
      sem_post(&sem_cachefile_rw);
      LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                " from cache file\n",CurToRead,FileOff)
    } else {
      // No write support or data not cached
      if(GetOrigImageData(buf,
                          FileOff,
                          CurToRead)!=CurToRead)
      {
        LOG_ERROR("Couldn't read data from input image!\n")
        return -1;
      }
      LOG_DEBUG("Read %zd bytes at offset %" PRIu64
                " from virtual DD file\n",CurToRead,
                FileOff)
    }
    CurBlock++;
    BlockOff=0;
    buf+=CurToRead;
    ToRead-=CurToRead;
    FileOff+=CurToRead;
  }
  return size;
}

/*
 * SetVdiFileHeaderData:
 *   Write data to virtual VDI file header
 *
 * Params:
 *   buf: Buffer containing data to write
 *   offset: Offset of changes
 *   size: Amount of bytes to write
 *
 * Returns:
 *   Number of written bytes on success or "-1" on error
 */
static int SetVdiFileHeaderData(char *buf,off_t offset,size_t size) {
  if(offset+size>VdiFileHeaderSize) size=VdiFileHeaderSize-offset;
  LOG_DEBUG("Need to cache %zu bytes at offset %" PRIu64
            " from VDI header\n",size,offset)
  sem_wait(&sem_cachefile_rw);
  sem_wait(&sem_vdiheader_read);
  if(pCacheFileHeader->VdiFileHeaderCached==1) {
    // Header was already cached
    if(fseeko(hCacheFile,
              pCacheFileHeader->pVdiFileHeader+offset,
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to cached VDI header at address %"
                PRIu64 "\n",pCacheFileHeader->pVdiFileHeader+offset)
      sem_post(&sem_vdiheader_read);
      sem_post(&sem_cachefile_rw);
      return -1;
    }
    if(fwrite(buf,size,1,hCacheFile)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                PRIu64 "\n",size,
                pCacheFileHeader->pVdiFileHeader+offset)
      sem_post(&sem_vdiheader_read);
      sem_post(&sem_cachefile_rw);
      return -1;
    }
    LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64 " to cache file\n",
              size,pCacheFileHeader->pVdiFileHeader+offset)
  } else {
    // Header wasn't already cached.
    if(fseeko(hCacheFile,
              0,
              SEEK_END)!=0)
    {
      LOG_ERROR("Couldn't seek to end of cache file!")
      sem_post(&sem_vdiheader_read);
      sem_post(&sem_cachefile_rw);
      return -1;
    }
    pCacheFileHeader->pVdiFileHeader=ftello(hCacheFile);
    LOG_DEBUG("Caching whole VDI header\n")
    if(offset>0) {
      // Changes do not begin at offset 0, need to prepend with data from
      // VDI header
      if(fwrite((char*)pVdiFileHeader,offset,1,hCacheFile)!=1) {
        LOG_ERROR("Error while writing %" PRIu64 " bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  offset,
                  pCacheFileHeader->pVdiFileHeader);
        sem_post(&sem_vdiheader_read);
        sem_post(&sem_cachefile_rw);
        return -1;
      }
      LOG_DEBUG("Prepended changed data with %" PRIu64
                " bytes at cache file offset %" PRIu64 "\n",
                offset,pCacheFileHeader->pVdiFileHeader)
    }
    // Cache changed data
    if(fwrite(buf,size,1,hCacheFile)!=1) {
      LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                PRIu64 "\n",size,
                pCacheFileHeader->pVdiFileHeader+offset)
      sem_post(&sem_vdiheader_read);
      sem_post(&sem_cachefile_rw);
      return -1;
    }
    LOG_DEBUG("Wrote %zu bytes of changed data to cache file offset %"
              PRIu64 "\n",size,
              pCacheFileHeader->pVdiFileHeader+offset)
    if(offset+size!=VdiFileHeaderSize) {
      // Need to append data from VDI header to cache whole data struct
      if(fwrite(((char*)pVdiFileHeader)+offset+size,
                VdiFileHeaderSize-(offset+size),
                1,
                hCacheFile)!=1)
      {
        LOG_ERROR("Couldn't write %zu bytes to cache file at offset %"
                  PRIu64 "\n",VdiFileHeaderSize-(offset+size),
                  (uint64_t)(pCacheFileHeader->pVdiFileHeader+offset+size))
        sem_post(&sem_vdiheader_read);
        sem_post(&sem_cachefile_rw);
        return -1;
      }
      LOG_DEBUG("Appended %" PRIu32
                " bytes to changed data at cache file offset %"
                PRIu64 "\n",VdiFileHeaderSize-(offset+size),
                pCacheFileHeader->pVdiFileHeader+offset+size)
    }
    // Mark header as cached and update header in cache file
    pCacheFileHeader->VdiFileHeaderCached=1;
    if(fseeko(hCacheFile,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to offset 0 of cache file!\n")
      sem_post(&sem_vdiheader_read);
      sem_post(&sem_cachefile_rw);
      return -1;
    }
    if(fwrite((char*)pCacheFileHeader,sizeof(TCacheFileHeader),1,hCacheFile)!=1) {
      LOG_ERROR("Couldn't write changed cache file header!\n")
      sem_post(&sem_vdiheader_read);
      sem_post(&sem_cachefile_rw);
      return -1;
    }
  }
  // All important data has been written, now flush all buffers to make
  // sure data is written to cache file
  fflush(hCacheFile);
  ioctl(fileno(hCacheFile),BLKFLSBUF,0);
  sem_post(&sem_vdiheader_read);
  sem_post(&sem_cachefile_rw);
  return size;
}

/*
 * SetVirtImageData:
 *   Write data to virtual image
 *
 * Params:
 *   buf: Buffer containing data to write
 *   offset: Offset to start writing at
 *   size: Size of data to be written
 *
 * Returns:
 *   Number of written bytes on success or "-1" on error
 */
static int SetVirtImageData(const char *buf, off_t offset, size_t size) {
  uint64_t CurBlock=0;
  uint64_t VirtImageSize;
  uint64_t OrigImageSize;
  size_t ToWrite=0;
  size_t CurToWrite=0;
  off_t FileOff=offset;
  off_t BlockOff=0;
  char *WriteBuf=(char*)buf;
  char *buf2;
  ssize_t ret;

  // Get virtual image size
  if(!GetVirtImageSize(&VirtImageSize)) {
    LOG_ERROR("Couldn't get virtual image size!\n")
    return -1;
  }

  if(offset>=VirtImageSize) {
    LOG_ERROR("Attempt to write beyond EOF of virtual image file!\n")
    return -1;
  }

  if(offset+size>VirtImageSize) {
    LOG_DEBUG("Attempt to write past EOF of virtual image file\n")
    size=VirtImageSize-offset;
  }

  ToWrite=size;

  // Cache virtual image type specific data
  if(XMountConfData.VirtImageType==TVirtImageType_VDI) {
    if(FileOff<VdiFileHeaderSize) {
      ret=SetVdiFileHeaderData(WriteBuf,FileOff,ToWrite);
      if(ret==-1) {
        LOG_ERROR("Couldn't write data to virtual VDI file header!\n")
        return -1;
      }
      if(ret==ToWrite) return ToWrite;
      else {
        ToWrite-=ret;
        WriteBuf+=ret;
        FileOff=0;
      }
    } else FileOff-=VdiFileHeaderSize;
  }

  // Get original image size
  if(!GetOrigImageSize(&OrigImageSize)) {
    LOG_ERROR("Couldn't get original image size!\n")
    return -1;
  }

  // Calculate block to write data to
  CurBlock=FileOff/CACHE_BLOCK_SIZE;
  BlockOff=FileOff%CACHE_BLOCK_SIZE;
  
  while(ToWrite!=0) {
    // Calculate how many bytes we have to write to this block
    if(BlockOff+ToWrite>CACHE_BLOCK_SIZE) {
      CurToWrite=CACHE_BLOCK_SIZE-BlockOff;
    } else CurToWrite=ToWrite;
    if(pCacheFileBlockIndex[CurBlock].Assigned==1) {
      // Block was already cached
      sem_wait(&sem_cachefile_rw);
      // Seek to data offset in cache file
      if(fseeko(hCacheFile,
             pCacheFileBlockIndex[CurBlock].off_data+BlockOff,
             SEEK_SET)!=0)
      {
        LOG_ERROR("Couldn't seek to cached block at address %" PRIu64 "\n",
                  pCacheFileBlockIndex[CurBlock].off_data+BlockOff)
        return -1;
      }
      if(fwrite(WriteBuf,CurToWrite,1,hCacheFile)!=1) {
        LOG_ERROR("Error while writing %zu bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  CurToWrite,
                  pCacheFileBlockIndex[CurBlock].off_data+BlockOff);
        sem_post(&sem_cachefile_rw);
        return -1;
      }
      sem_post(&sem_cachefile_rw);
      LOG_DEBUG("Wrote %zd bytes at offset %" PRIu64
                " to cache file\n",CurToWrite,
                pCacheFileBlockIndex[CurBlock].off_data+BlockOff)
    } else {
      // Uncached block. Need to cache entire new block
      // Seek to end of cache file to append new cache block
      sem_wait(&sem_cachefile_rw);
      fseeko(hCacheFile,0,SEEK_END);
      pCacheFileBlockIndex[CurBlock].off_data=ftello(hCacheFile);
      if(BlockOff!=0) {
        // Changed data does not begin at block boundry. Need to prepend
        // with data from virtual image file
        buf2=(char*)malloc(BlockOff*sizeof(char));
        if(buf2==NULL) {
          LOG_ERROR("Couldn't allocate memmory!\n")
          sem_post(&sem_cachefile_rw);
          return -1;
        }
        if(GetOrigImageData(buf2,FileOff-BlockOff,BlockOff)!=BlockOff) {
          LOG_ERROR("Couldn't read data from original image file!\n")
          sem_post(&sem_cachefile_rw);
          return -1;
        }
        if(fwrite(buf2,BlockOff,1,hCacheFile)!=1) {
          LOG_ERROR("Couldn't writing %" PRIu64 " bytes "
                    "to cache file at offset %" PRIu64 "!\n",
                    BlockOff,
                    pCacheFileBlockIndex[CurBlock].off_data);
          sem_post(&sem_cachefile_rw);
          return -1;
        }
        LOG_DEBUG("Prepended changed data with %" PRIu64
                  " bytes from virtual image file at offset %" PRIu64
                  "\n",BlockOff,FileOff-BlockOff)
        free(buf2);
      }
      if(fwrite(WriteBuf,CurToWrite,1,hCacheFile)!=1) {
        LOG_ERROR("Error while writing %zd bytes "
                  "to cache file at offset %" PRIu64 "!\n",
                  CurToWrite,
                  pCacheFileBlockIndex[CurBlock].off_data+BlockOff);
        sem_post(&sem_cachefile_rw);
        return -1;
      }
      if(BlockOff+CurToWrite!=CACHE_BLOCK_SIZE) {
        // Changed data does not end at block boundry. Need to append
        // with data from virtual image file
        buf2=(char*)malloc((CACHE_BLOCK_SIZE-
                           (BlockOff+CurToWrite))*sizeof(char));
        memset(buf2,0,CACHE_BLOCK_SIZE-(BlockOff+CurToWrite));
        if(buf2==NULL) {
          LOG_ERROR("Couldn't allocate memmory!\n")
          sem_post(&sem_cachefile_rw);
          return -1;
        }
        if((FileOff-BlockOff)+CACHE_BLOCK_SIZE>OrigImageSize) {
          // Original image is smaller than full cache block
          if(GetOrigImageData(buf2,
               FileOff+CurToWrite,
               OrigImageSize-(FileOff+CurToWrite))!=
             OrigImageSize-(FileOff+CurToWrite))
          {
            LOG_ERROR("Couldn't read data from virtual image file!\n")
            sem_post(&sem_cachefile_rw);
            return -1;
          }
        } else {
          if(GetOrigImageData(buf2,
               FileOff+CurToWrite,
               CACHE_BLOCK_SIZE-(BlockOff+CurToWrite))!=
             CACHE_BLOCK_SIZE-(BlockOff+CurToWrite))
          {
            LOG_ERROR("Couldn't read data from virtual image file!\n")
            sem_post(&sem_cachefile_rw);
            return -1;
          }
        }
        if(fwrite(buf2,
                  CACHE_BLOCK_SIZE-(BlockOff+CurToWrite),
                  1,
                  hCacheFile)!=1)
        {
          LOG_ERROR("Error while writing %zd bytes "
                    "to cache file at offset %" PRIu64 "!\n",
                    CACHE_BLOCK_SIZE-(BlockOff+CurToWrite),
                    pCacheFileBlockIndex[CurBlock].off_data+BlockOff+CurToWrite);
          sem_post(&sem_cachefile_rw);
          return -1;
        }
        free(buf2);
      }
      // All important data for this cache block has been written,
      // flush all buffers and mark cache block as assigned
      fflush(hCacheFile);
      ioctl(fileno(hCacheFile),BLKFLSBUF,0);
      pCacheFileBlockIndex[CurBlock].Assigned=1;
      // Update cache block index entry in cache file
      fseeko(hCacheFile,
             sizeof(TCacheFileHeader)+(CurBlock*sizeof(TCacheFileBlockIndex)),
             SEEK_SET);
      if(fwrite(&(pCacheFileBlockIndex[CurBlock]),
                sizeof(TCacheFileBlockIndex),
                1,
                hCacheFile)!=1)
      {
        LOG_ERROR("Couldn't update cache file block index!\n");
        sem_post(&sem_cachefile_rw);
        return -1;
      }
      LOG_DEBUG("Updated cache file block index: Number=%" PRIu64
                ", Data offset=%" PRIu64 "\n",CurBlock,
                pCacheFileBlockIndex[CurBlock].off_data);
    }
    // Flush buffers
    fflush(hCacheFile);
    ioctl(fileno(hCacheFile),BLKFLSBUF,0);
    sem_post(&sem_cachefile_rw);
    BlockOff=0;
    CurBlock++;
    WriteBuf+=CurToWrite;
    ToWrite-=CurToWrite;
    FileOff+=CurToWrite;
  }
  return size;
}

/*
 * GetVirtFileAttr:
 *   FUSE getattr implementation
 *
 * Params:
 *   path: Path of file to get attributes from
 *   stbuf: Pointer to stat structure to save attributes to
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int GetVirtFileAttr(const char *path, struct stat *stbuf) {
  memset(stbuf,0,sizeof(struct stat));
  if(strcmp(path,"/")==0) {
    // Attributes of mountpoint
    stbuf->st_mode=S_IFDIR | 0777;
    stbuf->st_nlink=2;
  } else if(strcmp(path,XMountConfData.pVirtualImagePath)==0) {
    // Attributes of virtual image
    if(!XMountConfData.Writable) stbuf->st_mode=S_IFREG | 0444;
    else stbuf->st_mode=S_IFREG | 0666;
    stbuf->st_nlink=1;
    // Get virtual image file size
    if(!GetVirtImageSize(&(stbuf->st_size))) {
      LOG_ERROR("Couldn't get image size!\n");
      return -ENOENT;
    }
  } else if(strcmp(path,XMountConfData.pVirtualImageInfoPath)==0) {
    // Attributes of virtual image info file
    stbuf->st_mode=S_IFREG | 0444;
    stbuf->st_nlink=1;
    // Get virtual image info file size
    sem_wait(&sem_info_read);
    if(pVirtualImageInfoFile!=NULL) {
      stbuf->st_size=strlen(pVirtualImageInfoFile);
    } else stbuf->st_size=0;
    sem_post(&sem_info_read);
  } else return -ENOENT;
  return 0;
}

/*
 * GetVirtFiles:
 *   FUSE readdir implementation
 *
 * Params:
 *   path: Path from where files should be listed
 *   buf: Buffer to write file entrys to
 *   filler: Function to write file entrys to buffer
 *   offset: ??? but not used
 *   fi: ??? but not used
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int GetVirtFiles(const char *path,
                        void *buf,
                        fuse_fill_dir_t filler,
                        off_t offset,
                        struct fuse_file_info *fi)
{
  (void)offset;
  (void)fi;

  // We have only files in root
  if(strcmp(path,"/")!=0) return -ENOENT;
  // Add std . and .. entrys
  filler(buf,".",NULL,0);
  filler(buf, "..",NULL,0);
  // Add our virtual files (p+1 to ignore starting "/")
  filler(buf,XMountConfData.pVirtualImagePath+1,NULL,0);
  filler(buf,XMountConfData.pVirtualImageInfoPath+1,NULL,0);
  return 0;
}

/*
 * OpenVirtFile:
 *   FUSE open implementation
 *
 * Params:
 *   path: Path to file to open
 *   fi: ??? but not used
 *
 * Returns:
 *   "0" on success, negated error code on error
 */
static int OpenVirtFile(const char *path, struct fuse_file_info *fi) {
  if(strcmp(path,XMountConfData.pVirtualImagePath)!=0 &&
     strcmp(path,XMountConfData.pVirtualImageInfoPath)!=0)
  {
    // Attempt to open a non existant file
    LOG_DEBUG("Attempt to open non existant file \"%s\".\n",path)
    return -ENOENT;
  }
  // Check open permissions
  if(!XMountConfData.Writable && (fi->flags & 3)!=O_RDONLY) {
    // Attempt to open a read-only file for writing
    LOG_DEBUG("Attempt to open the read-only file \"%s\" for writing.\n",path)
    return -EACCES;
  }
  return 0;
}

/*
 * ReadVirtFile:
 *   FUSE read implementation
 *
 * Params:
 *   buf: Buffer where read data is written to
 *   size: Number of bytes to read
 *   offset: Offset to start reading at
 *   fi: ?? but not used
 *
 * Returns:
 *   Read bytes on success, negated error code on error
 */
static int ReadVirtFile(const char *path,
                        char *buf,
                        size_t size,
                        off_t offset,
                        struct fuse_file_info *fi)
{
  uint64_t len;

  if(strcmp(path,XMountConfData.pVirtualImagePath)==0) {
    // Get virtual image file size
    if(!GetVirtImageSize(&len)) {
      LOG_ERROR("Couldn't get virtual image size!\n")
      return 0;
    }
    if(offset<len) {
      if(offset+size>len) size=len-offset;
      if(GetVirtImageData(buf,offset,size)!=size) {
        LOG_ERROR("Couldn't read data from virtual image file!\n")
        return 0;
      }
    } else {
      LOG_DEBUG("Attempt to read past EOF of virtual image file\n");
      return 0;
    }
  } else if(strcmp(path,XMountConfData.pVirtualImageInfoPath)==0) {
    // Read data from virtual image info file
    len=strlen(pVirtualImageInfoFile);
    if(offset<len) {
      if(offset+size>len) {
        size=len-offset;
        LOG_DEBUG("Attempt ro read past EOF of virtual image info file")
      }
      sem_wait(&sem_info_read);
      memcpy(buf,pVirtualImageInfoFile+offset,size);
      sem_post(&sem_info_read);
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                " from virtual image info file\n",size,offset)
    } else {
      LOG_DEBUG("Attempt to read past EOF of virtual info file\n");
      return 0;
    }
  } else {
    // Attempt to read non existant file
    LOG_DEBUG("Attempt to read from non existant file \"%s\"\n",path)
    return -ENOENT;
  }

  return size;
}

/*
 * WriteVirtFile:
 *   FUSE write implementation
 *
 * Params:
 *   buf: Buffer containing data to write
 *   size: Number of bytes to write
 *   offset: Offset to start writing at
 *   fi: ?? but not used
 *
 * Returns:
 *   Written bytes on success, negated error code on error
 */
static int WriteVirtFile(const char *path,
                         const char *buf,
                         size_t size,
                         off_t offset,
                         struct fuse_file_info *fi)
{
   uint64_t len;

  if(strcmp(path,XMountConfData.pVirtualImagePath)==0) {
    // Get virtual image file size
    if(!GetVirtImageSize(&len)) {
      LOG_ERROR("Couldn't get virtual image size!\n")
      return 0;
    }
    if(offset<len) {
      if(offset+size>len) size=len-offset;
      if(SetVirtImageData(buf,offset,size)!=size) {
        LOG_ERROR("Couldn't write data to virtual image file!\n")
        return 0;
      }
    } else {
      LOG_DEBUG("Attempt to write past EOF of virtual image file\n");
      return 0;
    }
  } else if(strcmp(path,XMountConfData.pVirtualImageInfoPath)==0) {
    // Attempt to write data to read only image info file
    LOG_DEBUG("Attempt to write data to virtual info file\n");
    return -ENOENT;
  } else {
    // Attempt to write to non existant file
    LOG_DEBUG("Attempt to write to the non existant file \"%s\"\n",path)
    return -ENOENT;
  }

  return size;
}

//static int mountewf_release(const char *path,
//                            fuse_file_info *finfo)
//{
//  return 0;
//}

/*
 * InitVirtVdiHeader:
 *   Build and init virtual VDI file header
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitVirtVdiHeader() {
  // See http://forums.virtualbox.org/viewtopic.php?t=8046 for a
  // "description" of the various header fields

  uint64_t ImageSize;
  off_t offset;
  uint32_t i,BlockEntries;

  // Get input image size
  if(!GetOrigImageSize(&ImageSize)) {
    LOG_ERROR("Couldn't get input image size!\n")
    return FALSE;
  }

  // Calculate how many VDI blocks we need
  BlockEntries=ImageSize/VDI_IMAGE_BLOCK_SIZE;
  if((ImageSize%VDI_IMAGE_BLOCK_SIZE)!=0) BlockEntries++;
  VdiBlockMapSize=BlockEntries*sizeof(uint32_t);
  LOG_DEBUG("BlockMap: %d (%08X) entries, %d (%08X) bytes!\n",
            BlockEntries,
            BlockEntries,
            VdiBlockMapSize,
            VdiBlockMapSize)

  // Allocate memmory for vdi header and block map
  VdiFileHeaderSize=sizeof(TVdiFileHeader)+VdiBlockMapSize;
  if((pVdiFileHeader=malloc(VdiFileHeaderSize))==NULL) {
    LOG_ERROR("Couldn't allocate memmory for TVdiFileHeader!\n");
    return FALSE;
  }
  memset(pVdiFileHeader,0,VdiFileHeaderSize);
  pVdiBlockMap=((void*)pVdiFileHeader)+sizeof(TVdiFileHeader);

  // Init header values
  strncpy(pVdiFileHeader->szFileInfo,VDI_FILE_COMMENT,
          strlen(VDI_FILE_COMMENT)+1);
  pVdiFileHeader->u32Signature=VDI_IMAGE_SIGNATURE;
  pVdiFileHeader->u32Version=VDI_IMAGE_VERSION;
  pVdiFileHeader->cbHeader=0x00000180;  // No idea what this is for! Testimage had same value
  pVdiFileHeader->u32Type=VDI_IMAGE_TYPE_FIXED;
  pVdiFileHeader->fFlags=VDI_IMAGE_FLAGS;
  strncpy(pVdiFileHeader->szComment,VDI_HEADER_COMMENT,
          strlen(VDI_HEADER_COMMENT)+1);
  pVdiFileHeader->offData=VdiFileHeaderSize;
  pVdiFileHeader->offBlocks=sizeof(TVdiFileHeader);
  pVdiFileHeader->cCylinders=0; // Legacy info
  pVdiFileHeader->cHeads=0; // Legacy info
  pVdiFileHeader->cSectors=0; // Legacy info
  pVdiFileHeader->cbSector=512; // Legacy info
  pVdiFileHeader->u32Dummy=0;
  pVdiFileHeader->cbDisk=ImageSize;
  // Seems that VBox is always using 1MB as blocksize
  pVdiFileHeader->cbBlock=VDI_IMAGE_BLOCK_SIZE;
  pVdiFileHeader->cbBlockExtra=0;
  pVdiFileHeader->cBlocks=BlockEntries;
  pVdiFileHeader->cBlocksAllocated=BlockEntries;
  // Just generate some random UUIDS
  // VBox won't accept immages where create and modify UUIDS aren't set
  *((uint32_t*)(&(pVdiFileHeader->uuidCreate_l)))=rand();
  *((uint32_t*)(&(pVdiFileHeader->uuidCreate_l))+4)=rand();
  *((uint32_t*)(&(pVdiFileHeader->uuidCreate_h)))=rand();
  *((uint32_t*)(&(pVdiFileHeader->uuidCreate_h))+4)=rand();
  *((uint32_t*)(&(pVdiFileHeader->uuidModify_l)))=rand();
  *((uint32_t*)(&(pVdiFileHeader->uuidModify_l))+4)=rand();
  *((uint32_t*)(&(pVdiFileHeader->uuidModify_h)))=rand();
  *((uint32_t*)(&(pVdiFileHeader->uuidModify_h))+4)=rand();

  // Generate block map
  i=0;
  for(offset=0;offset<VdiBlockMapSize;offset+=4) {
    *((uint32_t*)(pVdiBlockMap+offset))=i;
    i++;
  }

  LOG_DEBUG("VDI header size = %u\n",VdiFileHeaderSize)

  return TRUE;
}

/*
 * InitVirtualVmdkFile:
 *   Init the virtual VMDK file
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitVirtualVmdkFile() {
  uint64_t ImageSize;

  // Get original image size
  if(!GetOrigImageSize(&ImageSize)) {
    LOG_ERROR("Couldn't get original image size!\n")
    return FALSE;
  }

  
}

/*
 * ParseCmdLine:
 *   Parse command line options
 *
 * Params:
 *   argc: Number of cmdline params
 *   argv: Array containing cmdline params
 *   pNargv: Number of FUSE options is written to this var
 *   pppNargv: FUSE options are written to this array
 *   pFilenameCount: Number of input image files is written to this var
 *   pppFilenames: Input image filenames are written to this array
 *   ppMountpoint: Mountpoint is written to this var
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int ParseCmdLine(const int argc,
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
  (*pppNargv)=(char**)malloc(opts*sizeof(char*));
  if((*pppNargv)==NULL) {
    LOG_ERROR("Couldn't allocate memmory!\n")
    return FALSE;
  }
  (*pppNargv)[opts-1]=(char*)malloc((strlen(argv[0])+1)*sizeof(char));
  if((*pppNargv)[opts-1]==NULL) {
    LOG_ERROR("Couldn't allocate memmory!\n")
    return FALSE;
  }
  strncpy((*pppNargv)[opts-1],argv[0],strlen(argv[0])+1);

  // Parse options
  while(i<argc && *argv[i]=='-') {
    if(strlen(argv[i])>1 && *(argv[i]+1)!='-') {
      opts++;
      (*pppNargv)=(char**)realloc((*pppNargv),opts*sizeof(char*));
      if((*pppNargv)==NULL) {
        LOG_ERROR("Couldn't allocate memmory for fuse options!\n")
        return FALSE;
      }
      (*pppNargv)[opts-1]=(char*)malloc((strlen(argv[i])+1)*sizeof(char));
      if((*pppNargv)[opts-1]==NULL) {
        LOG_ERROR("Couldn't allocate memmory for fuse options!\n")
        return FALSE;
      }
      strncpy((*pppNargv)[opts-1],argv[i],strlen(argv[i])+1);
      // React too on fuse's debug flag (-d)
      if(strcmp(argv[i],"-d")==0) XMountConfData.Debug=TRUE;
    } else {
      // Options beginning with -- are mountewf specific
      if(strcmp(argv[i],"--rw")==0) {
        // Emulate writable access to mounted image
        // Next parameter must be cache file to read/write changes from/to
        if((argc+1)>i) {
          i++;
          XMountConfData.pCacheFile=(char*)malloc((strlen(argv[i])+1)*sizeof(char));
          if(XMountConfData.pCacheFile==NULL) {
            LOG_ERROR("Couldn't alloc memmory!\n")
            return FALSE;
          }
          strncpy(XMountConfData.pCacheFile,argv[i],strlen(argv[i])+1);
          XMountConfData.Writable=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file to read/write data from/to!\n")
          return FALSE;
        }
        LOG_DEBUG("Enabling virtual write support using cache file \"%s\"\n",
                  XMountConfData.pCacheFile)
      } else if(strcmp(argv[i],"--in")==0) {
        // Specify input image type
        // Next parameter must be image type
        if((argc+1)>i) {
          i++;
          if(strcmp(argv[i],"dd")==0) {
            XMountConfData.OrigImageType=TOrigImageType_DD;
            LOG_DEBUG("Setting input image type to DD\n")
#ifdef HAVE_LIBEWF
          } else if(strcmp(argv[i],"ewf")==0) {
            XMountConfData.OrigImageType=TOrigImageType_EWF;
            LOG_DEBUG("Setting input image type to EWF\n")
#endif
#ifdef XMOUNT_SUPPORTS_AFF
          } else if(strcmp(argv[i],"aff")==0) {
            XMountConfData.OrigImageType=TOrigImageType_AFF;
            LOG_DEBUG("Setting input image type to AFF\n")
#endif
          } else {
            LOG_ERROR("Unknown input image type \"%s\"!\n",argv[i])
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify an input image type!\n");
          return FALSE;
        }
      } else if(strcmp(argv[i],"--out")==0) {
        // Specify output image type
        // Next parameter must be image type
        if((argc+1)>i) {
          i++;
          if(strcmp(argv[i],"dd")==0) {
            XMountConfData.VirtImageType=TVirtImageType_DD;
            LOG_DEBUG("Setting virtual image type to DD\n")
          } else if(strcmp(argv[i],"vdi")==0) {
            XMountConfData.VirtImageType=TVirtImageType_VDI;
            LOG_DEBUG("Setting virtual image type to VDI\n")
          } else if(strcmp(argv[i],"vmdk")==0) {
            XMountConfData.VirtImageType=TVirtImageType_VMDK;
            LOG_DEBUG("Setting virtual image type to VMDK\n")
          } else {
            LOG_ERROR("Unknown output image type \"%s\"!\n",argv[i])
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify an output image type!\n");
          return FALSE;
        }
      } else {
        LOG_ERROR("Unknown command line option \"%s\"\n",argv[i]);
        return FALSE;
      }
    }
    i++;
  }

  // Parse input image filename(s)
  while(i<(argc-1)) {
    files++;
    (*pppFilenames)=(char**)realloc((*pppFilenames),files*sizeof(char*));
    if((*pppFilenames)==NULL) {
      LOG_ERROR("Couldn't allocate memmory for input image filename(s)!\n")
      return FALSE;
    }
    (*pppFilenames)[files-1]=(char*)malloc((strlen(argv[i])+1)*sizeof(char));
    if((*pppFilenames)[files-1]==NULL) {
      LOG_ERROR("Couldn't allocate memmory for input image filename(s)!\n")
      return FALSE;
    }
    strncpy((*pppFilenames)[files-1],argv[i],strlen(argv[i])+1);
    i++;
  }
  *pFilenameCount=files;

  // Extract mountpoint
  if(argc>1) {
    (*ppMountpoint)=(char*)malloc((strlen(argv[argc-1])+1)*sizeof(char));
    if((*ppMountpoint)==NULL) {
      LOG_ERROR("Couldn't allocate memmory for mountpoint!\n")
      return FALSE;
    }
    strncpy(*ppMountpoint,argv[argc-1],strlen(argv[argc-1])+1);
    opts++;
    (*pppNargv)=(char**)realloc((*pppNargv),opts*sizeof(char*));
   if((*pppNargv)==NULL) {
      LOG_ERROR("Couldn't allocate memmory for mountpoint!\n")
      return FALSE;
    }
    (*pppNargv)[opts-1]=(char*)malloc((strlen(argv[i])+1)*sizeof(char));
    if((*pppNargv)[opts-1]==NULL) {
      LOG_ERROR("Couldn't allocate memmory for mountpoint!\n")
      return FALSE;
    }
    strncpy((*pppNargv)[opts-1],*ppMountpoint,strlen((*ppMountpoint))+1);
  }

  *pNargc=opts;

  return TRUE;
}

/*
 * InitVirtImageInfoFile:
 *   Create virtual image info file
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitVirtImageInfoFile() {
  char buf[200];
  uint8_t md5_buf[16];
  int ret;

#define M_CHECK_ALLOC { \
  if(pVirtualImageInfoFile==NULL) { \
    LOG_ERROR("Couldn't allocate memmory!\n"); \
    return FALSE; \
  } \
}

  // Add static header to file
  pVirtualImageInfoFile=(char*)malloc((strlen(IMAGE_INFO_HEADER)+1));
  M_CHECK_ALLOC
  strncpy(pVirtualImageInfoFile,IMAGE_INFO_HEADER,strlen(IMAGE_INFO_HEADER)+1);

  switch(XMountConfData.OrigImageType) {
    case TOrigImageType_DD:
      // Original image is a DD file. There isn't much info to extract. Perhaps
      // just add image size
      // TODO: Add infos to virtual image info file
      break;
#ifdef HAVE_LIBEWF

#define M_SAVE_VALUE(DESC) { \
  if(ret==1) {             \
    pVirtualImageInfoFile=(char*)realloc(pVirtualImageInfoFile, \
      (strlen(pVirtualImageInfoFile)+strlen(buf)+strlen(DESC)+2)); \
    M_CHECK_ALLOC \
    strncpy((pVirtualImageInfoFile+strlen(pVirtualImageInfoFile)),DESC,strlen(DESC)+1); \
    strncpy((pVirtualImageInfoFile+strlen(pVirtualImageInfoFile)),buf,strlen(buf)+1); \
    strncpy((pVirtualImageInfoFile+strlen(pVirtualImageInfoFile)),"\n",2); \
  } else if(ret==-1) { \
    LOG_ERROR("Couldn't query EWF image info!\n") \
    return FALSE; \
  } \
}

    case TOrigImageType_EWF:
      // Original image is an EWF file. Extract various infos from ewf file and
      // add them to the virtual image info file content.
      ret=libewf_get_header_value_case_number(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Case number: ")
      ret=libewf_get_header_value_description(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Description: ")
      ret=libewf_get_header_value_examiner_name(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Examiner: ")
      ret=libewf_get_header_value_evidence_number(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Evidence number: ")
      ret=libewf_get_header_value_notes(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Notes: ")
      ret=libewf_get_header_value_acquiry_date(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Acquiry date: ")
      ret=libewf_get_header_value_system_date(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("System date: ")
      ret=libewf_get_header_value_acquiry_operating_system(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Acquiry os: ")
      ret=libewf_get_header_value_acquiry_software_version(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Acquiry sw version: ")
      ret=libewf_get_hash_value_md5(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("MD5 hash: ")
      ret=libewf_get_hash_value_sha1(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("SHA1 hash: ")
      break;

#undef M_SAVE_VALUE

#endif
#ifdef XMOUNT_SUPPORTS_AFF
    case TOrigImageType_AFF:
      // Original image is an AFF file.
      // TODO: Implement AFF image type handling
      LOG_ERROR("AFF image type handling not implemented!\n")
      return FALSE;
      break;
#endif
    default:
      LOG_ERROR("Unsupported input image type!\n")
      return FALSE;
  }

#undef M_CHECK_ALLOC

  return TRUE;
}

/*
 * InitCacheFile:
 *   Create / load cache file to enable virtual write support
 *
 * Params:
 *   n/a
 *
 * Returns:
 *   "TRUE" on success, "FALSE" on error
 */
static int InitCacheFile() {
  uint64_t ImageSize=0;
  uint64_t BlockIndexSize=0;
  uint64_t CacheFileHeaderSize=0;
  uint64_t CacheFileSize=0;
  uint32_t NeededBlocks=0;
  uint64_t buf;

  hCacheFile=(FILE*)fopen64(XMountConfData.pCacheFile,"rb+");
  if(hCacheFile==NULL) {
    // As the c lib seems to have no possibility to open a file rw wether it
    // exists or not (w+ does not work because it truncates an existing file),
    // when r+ returns NULL the file could simply not exist
    LOG_DEBUG("Cache file does not exist. Creating new one\n")
    hCacheFile=(FILE*)fopen64(XMountConfData.pCacheFile,"wb+");
    if(hCacheFile==NULL) {
      // There is really a problem opening the file
      LOG_ERROR("Couldn't open cache file \"%s\"!\n",
                XMountConfData.pCacheFile)
      return FALSE;
    }
  }

  // Get input image size
  if(!GetOrigImageSize(&ImageSize)) {
    LOG_ERROR("Couldn't get input image size!\n")
    return FALSE;
  }

  // Calculate how many blocks are needed and how big the buffers must be
  // for the actual cache file version
  NeededBlocks=ImageSize/CACHE_BLOCK_SIZE;
  if((ImageSize%CACHE_BLOCK_SIZE)!=0) NeededBlocks++;
  BlockIndexSize=NeededBlocks*sizeof(TCacheFileBlockIndex);
  CacheFileHeaderSize=sizeof(TCacheFileHeader)+BlockIndexSize;
  LOG_DEBUG("Cache blocks: %u (%04X) entries, %zd (%08zX) bytes\n",
            NeededBlocks,
            NeededBlocks,
            BlockIndexSize,
            BlockIndexSize)

  // Get cache file size
  // fseeko64 had massive problems!
  if(fseeko(hCacheFile,0,SEEK_END)!=0) {
    LOG_ERROR("Couldn't seek to end of cache file!\n")
    return FALSE;
  }
  // Same here, ftello64 didn't work at all and returned 0 all the times
  CacheFileSize=ftello(hCacheFile);
  LOG_DEBUG("Cache file has %zd bytes\n",CacheFileSize)

  if(CacheFileSize>0) {
    // Cache file isn't empty, parse block header
    LOG_DEBUG("Cache file not empty. Parsing block header\n")
    if(fseeko(hCacheFile,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to beginning of cache file!\n")
      return FALSE;
    }
    // Read and check file signature
    if(fread(&buf,8,1,hCacheFile)!=1 || buf!=CACHE_FILE_SIGNATURE) {
      free(pCacheFileHeader);
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return FALSE;
    }
    // Now get cache file version (Has only 32bit!)
    if(fread(&buf,4,1,hCacheFile)!=1) {
      free(pCacheFileHeader);
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return FALSE;
    }
    switch((uint32_t)buf) {
      case 0x0001:
        // Actual version (1)
        if(fseeko(hCacheFile,0,SEEK_SET)!=0) {
          LOG_ERROR("Couldn't seek to beginning of cache file!\n")
          return FALSE;
        }
        // Alloc memmory for header and block index
        pCacheFileHeader=(pTCacheFileHeader)malloc(CacheFileHeaderSize);
        if(pCacheFileHeader==NULL) {
          LOG_ERROR("Couldn't alloc memmory for cache file header!\n")
          return FALSE;
        }
        memset(pCacheFileHeader,0,CacheFileHeaderSize);
        // Read header and block index from file
        if(fread(pCacheFileHeader,CacheFileHeaderSize,1,hCacheFile)!=1) {
          // Cache file isn't big enough
          free(pCacheFileHeader);
          LOG_ERROR("Cache file corrupt!\n")
          return FALSE;
        }
        break;
      default:
        LOG_ERROR("Unknown cache file version!\n")
        return FALSE;
    }
    // Set pointer to block index
    pCacheFileBlockIndex=(pTCacheFileBlockIndex)((void*)pCacheFileHeader+
                          pCacheFileHeader->pBlockIndex);
  } else {
    // New cache file, generate a new block header
    LOG_DEBUG("Cache file is empty. Generating new block header\n");
    // Alloc memmory for header and block index
    pCacheFileHeader=(pTCacheFileHeader)malloc(CacheFileHeaderSize);
    if(pCacheFileHeader==NULL) {
      LOG_ERROR("Couldn't alloc memmory for cache file header!\n")
      return FALSE;
    }
    memset(pCacheFileHeader,0,CacheFileHeaderSize);
    pCacheFileHeader->FileSignature=CACHE_FILE_SIGNATURE;
    pCacheFileHeader->CacheFileVersion=CACHE_FILE_VERSION;
    pCacheFileHeader->BlockCount=NeededBlocks;
    //pCacheFileHeader->UsedBlocks=0;
    // The following pointer is only usuable when reading data from cache file
    pCacheFileHeader->pBlockIndex=sizeof(TCacheFileHeader);
    pCacheFileBlockIndex=(pTCacheFileBlockIndex)((void*)pCacheFileHeader+
                         sizeof(TCacheFileHeader));
    pCacheFileHeader->VdiFileHeaderCached=0;
    pCacheFileHeader->pVdiFileHeader=0;
    // Write header to file
    if(fwrite(pCacheFileHeader,CacheFileHeaderSize,1,hCacheFile)!=1) {
      free(pCacheFileHeader);
      LOG_ERROR("Couldn't write cache file header to file!\n");
      return FALSE;
    }
  }
  return TRUE;
}

/*
 * Struct containing implemented FUSE functions
 */
static struct fuse_operations xmount_operations = {
  .getattr=GetVirtFileAttr,
  .readdir=GetVirtFiles,
  .open=OpenVirtFile,
  .read=ReadVirtFile,
  .write=WriteVirtFile
//  .release=mountewf_release,
};

/*
 * Main
 */
int main(int argc, char *argv[])
{
  char **ppInputFilenames=NULL;
  int InputFilenameCount=0;
  int nargc=0;
  char **ppNargv=NULL;
  char *pMountpoint=NULL;
  int ret=1;
  int i=0;

  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  // Init XMountConfData
  XMountConfData.OrigImageType=TOrigImageType_DD;
  XMountConfData.VirtImageType=TVirtImageType_DD;
  XMountConfData.Debug=FALSE;
  XMountConfData.pVirtualImagePath=NULL;
  XMountConfData.pVirtualImageInfoPath=NULL;
  XMountConfData.Writable=FALSE;
  XMountConfData.pCacheFile=NULL;
  XMountConfData.OrigImageSize=0;
  XMountConfData.VirtImageSize=0;

  // Parse command line options
  if(!ParseCmdLine(argc,
                   argv,
                   &nargc,
                   &ppNargv,
                   &InputFilenameCount,
                   &ppInputFilenames,
                   &pMountpoint))
  {
    LOG_ERROR("Couldn't parse command line options!\n")
    PrintUsage(argv[0]);
    return 1;
  }

  // Check command line options
  if(nargc<2 || InputFilenameCount==0 || pMountpoint==NULL) {
    LOG_ERROR("Couldn't parse command line options!\n")
    PrintUsage(argv[0]);
    return 1;
  }

#ifdef HAVE_LIBEWF
  // Check for valid ewf files
  if(XMountConfData.OrigImageType==TOrigImageType_EWF) {
    for(i=0;i<InputFilenameCount;i++) {
      if(libewf_check_file_signature(ppInputFilenames[i])!=1) {
        LOG_ERROR("File \"%s\" isn't a valid ewf file!\n",ppInputFilenames[i])
        return 1;
      }
    }
  }
#endif

  // TODO: Check if mountpoint is a valid dir

  // Init semaphores
  sem_init(&sem_img_read,0,1);
  sem_init(&sem_vdiheader_read,0,1);
  sem_init(&sem_info_read,0,1);
  sem_init(&sem_cachefile_rw,0,1);

  if(InputFilenameCount==1) {
    LOG_DEBUG("Extracting infos from image file \"%s\"...\n",
              ppInputFilenames[0])
  } else {
    LOG_DEBUG("Extracting infos from image files \"%s .. %s\"...\n",
              ppInputFilenames[0],
              ppInputFilenames[InputFilenameCount-1])
  }

  // Init random generator
  srand(time(NULL));

  // Open input image
  switch(XMountConfData.OrigImageType) {
    case TOrigImageType_DD:
      // Input image is a DD file
      hDdFile=(FILE*)fopen64(ppInputFilenames[0],"rb");
      if(hDdFile==NULL) {
        LOG_ERROR("Couldn't open DD file \"%s\"\n",ppInputFilenames[0])
        return 1;
      }
      break;
#ifdef HAVE_LIBEWF
    case TOrigImageType_EWF:
      // Input image is an EWF file or glob
      hEwfFile=libewf_open(ppInputFilenames,
                           InputFilenameCount,
                           libewf_get_flags_read());
      if(hEwfFile==NULL) {
        LOG_ERROR("Couldn't open EWF file(s)!\n")
        return 1;
      }
      // Parse EWF header
      if(libewf_parse_header_values(hEwfFile,0)!=1) {
        LOG_ERROR("Couldn't parse ewf header values!\n")
        return 1;
      }
      break;
#endif
#ifdef XMOUNT_SUPPORTS_AFF
    case TOrigImageType_AFF:
      // Input image is an AFF file
      // TODO: Handle AFF image type
      LOG_ERROR("AFF image type handling isn't implemented!\n")
      return 1;
      //hAffFile=af_open(ppInputFilenames[0],
      break;
#endif
    default:
      LOG_ERROR("Unsupported input image type specified!\n")
      return 1;
  }
  LOG_DEBUG("Input image file opened successfully\n")

  // Gather infos for info file
  if(!InitVirtImageInfoFile()) {
    LOG_ERROR("Couldn't gather infos for virtual image info file!\n")
    return 1;
  }
  LOG_DEBUG("Virtual image info file build successfully\n")

  // Do some virtual image type specific initialisations
  switch(XMountConfData.VirtImageType) {
    case TVirtImageType_VDI:
      // When mounting as VDI, we need to construct a vdi header
      if(!InitVirtVdiHeader()) {
        LOG_ERROR("Couldn't initialize virtual VDI file header!\n")
        return 1;
      }
      LOG_DEBUG("Virtual VDI file header build successfully\n")
      break;
    case TVirtImageType_VMDK:
      // When mounting as VMDK, we need to construct the VMDK descripto file
      // TODO: Construct VMDK file descriptor file
      break;
  }

  if(XMountConfData.Writable) {
    // Init cache file and cache file block index
    if(!InitCacheFile()) {
      LOG_ERROR("Couldn't initilaize cache file!\n")
      return 1;
    }
    LOG_DEBUG("Cache file initialized successfully\n")
  }

  if(!ExtractVirtFileNames(ppInputFilenames[0])) {
    LOG_ERROR("Couldn't extract virtual file names!\n");
    return 1;
  }
  LOG_DEBUG("Virtual file names extracted successfully\n")

  // Call fuse_main to do the fuse magic
  ret=fuse_main(nargc,ppNargv,&xmount_operations,NULL);

  // TODO: Perhaps wait for unposted sem's
  sem_destroy(&sem_cachefile_rw);
  sem_destroy(&sem_info_read);
  sem_destroy(&sem_vdiheader_read);
  sem_destroy(&sem_img_read);

  // Close input image
  switch(XMountConfData.OrigImageType) {
    case TOrigImageType_DD:
      fclose(hDdFile);
      break;
#ifdef HAVE_LIBEWF
    case TOrigImageType_EWF:
      libewf_close(hEwfFile);
      break;
#endif
#ifdef XMOUNT_SUPPORTS_AFF
    case TOrigImageType_AFF:
      af_close(hAffFile);
      break;
#endif
    default:
      LOG_ERROR("Couldn't close unsupported input image type!\n");
  }

  if(XMountConfData.Writable) {
    // Write support was enabled, close cache file
    fclose(hCacheFile);
    free(pCacheFileHeader);
  }

  // Free allocated memmory
  if(XMountConfData.VirtImageType==TVirtImageType_VDI) {
    // Free constructed VDI header
    free(pVdiFileHeader);
  }
  for(i=0;i<InputFilenameCount;i++) free(ppInputFilenames[i]);
  free(ppInputFilenames);
  for(i=0;i<nargc;i++) free(ppNargv[i]);
  free(ppNargv);
  free(XMountConfData.pVirtualImagePath);
  free(XMountConfData.pVirtualImageInfoPath);
  free(XMountConfData.pCacheFile);

  return ret;
}

/*
  ----- Change history -----
  20090131: v0.1.0 released
            * Some minor things have still to be done.
            * Mounting ewf as dd: Seems to work. Diff didn't complain about
              changes betwenn original dd and emulated dd.
            * Mounting ewf as vdi: Seems to work too. VBox accepts the emulated
              vdi as valid vdi file and I was able to mount the containing fs
              under Debian. INFO: Debian freezed when not using mount -r !!
  20090203: v0.1.1 released
            * Multiple code improvements. For ex. cleaner vdi header allocation.
            * Fixed severe bug in image block calculation. Didn't check for odd
              input in conversion from bytes to megabytes.
            * Added more debug output
  20090210: v0.1.2 released
            * Fixed compilation problem (Typo in image_init_info() function).
            * Fixed some problems with the debian scripts to be able to build
              packages.
            * Added random generator initialisation (Makes it possible to use
              more than one image in VBox at a time).
  20090215: * Added function init_cache_blocks which creates / loads a cache
              file used to implement virtual write capability.
  20090217: * Implemented the fuse write function. Did already some basic tests
              with dd and it seems to work. But there are certainly still some
              bugs left as there are also still some TODO's left.
  20090226: * Changed program name from mountewf to xmount.
            * Began with massive code cleanups to ease full implementation of
              virtual write support and to be able to support multiple input
              image formats (DD, EWF and AFF are planned for now).
            * Added defines for supported input formats so it should be possible
              to compile mountimg without supporting all input formats. (DD
              input images are always supported as these do not require any
              additional libs). Input formats should later be en/disabled
              by the configure script in function to which libs it detects.
            * GetOrigImageSize function added to get the size of the original
              image whatever type it is in.
            * GetOrigImageData function added to retrieve data from original
              image file whatever type it is in.
            * GetVirtImageSize function added to get the size of the virtual
              image file.
            * Cleaned function mountewf_getattr and renamed it to
              GetVirtFileAttr
            * Cleaned function mountewf_readdir and renamed it to GetVirtFiles
            * Cleaned function mountewf_open and renamed it to OpenVirtFile
  20090227: * Cleaned function init_info_file and renamed it to
              InitVirtImageInfoFile
  20090228: * Cleaned function init_cache_blocks and renamed it to
              InitCacheFile
            * Added LogMessage function to ease error and debug logging (See
              also LOG_ERROR and LOG_DEBUG macros in mountimg.h)
            * Cleaned function init_vdi_header and renamed it to
              InitVirtVdiHeader
            * Added PrintUsage function to print out mountimg usage informations
            * Cleaned function parse_cmdline and renamed it to ParseCmdLine
            * Cleaned function main
            * Added ExtractVirtFileNames function to extract virtual file names
              from input image name
            * Added function GetVirtImageData to retrieve data from the virtual
              image file. This includes reading data from cache file if virtual
              write support is enabled.
            * Added function ReadVirtFile to replace mountewf_read
  20090229: * Fixed a typo in virtual file name creation
            * Added function SetVirtImageData to write data to virtual image
              file. This includes writing data to cache file and caching entire
              new blocks
            * Added function WriteVirtFile to replace mountewf_write
  20090305: * Solved a problem that made it impossible to access offsets >32bit
  20090308: * Added SetVdiFileHeaderData function to handle virtual image type
              specific data to be cached. This makes cache files independent
              from virtual image type
  20090316: v0.2.0 released
*/
