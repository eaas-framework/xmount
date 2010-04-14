/*******************************************************************************
* xmount Copyright (c) 2008,2009 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#include "xmount_cache.h"
#include "xmount_log.h"
#include "xmount_macros.h"
#include "xmount_options.h"

#include <sys/ioctl.h>
#include <linux/fs.h>

/******************************** xmcache_open ********************************/
int xmcache_open(pTXMCacheFile pCacheFile, char *pFileName) {
  uint64_t CacheFileSize=0;
  uint64_t NeededBlocks=0;
  uint64_t ImageMapSize=0;
  
  uint32_t tmp_vers;
  char tmp_sig[8];

  if(XMOptions.OverwriteCache!=1) {
    // Try to open an existing cache file or create a new one
    LOG_DEBUG("Trying to open existing cache file \"%s\"\n",pFileName)
    pCacheFile->hCacheFile=(FILE*)fopen64(pFileName,"rb+");
    if(pCacheFile->hCacheFile==NULL) {
      // As the c lib seems to have no possibility to open a file rw wether it
      // exists or not (w+ does not work because it truncates an existing file),
      // when r+ returns NULL the file could simply not exist
      LOG_DEBUG("Cache file does not exist. Trying to create a new one\n")
      pCacheFile->hCacheFile=(FILE*)fopen64(pFileName,"wb+");
      if(pCacheFile->hCacheFile==NULL) {
        // There is really a problem opening the file
        LOG_ERROR("Couldn't open cache file \"%s\"!\n",pFileName)
        return 0;
      }
      LOG_DEBUG("Cache file created sucsessfully\n")
    } else {
      LOG_DEBUG("Cache file opened sucsessfully\n")
    }
  } else {
    // Overwrite existing cache file or create a new one
    LOG_DEBUG("Trying to create or overwrite the cache file \"%s\"\n",
              pFileName)
    pCacheFile->hCacheFile=(FILE*)fopen64(pFileName,"wb+");
    if(pCacheFile->hCacheFile==NULL) {
      LOG_ERROR("Couldn't open cache file \"%s\"!\n",pFileName)
      return 0;
    }
    LOG_DEBUG("Cache file created sucsessfully\n")
  }

  // Get cache file size
  if(fseeko(pCacheFile->hCacheFile,0,SEEK_END)!=0) {
    LOG_ERROR("Couldn't seek to end of cache file!\n")
    return 0;
  }
  CacheFileSize=ftello(pCacheFile->hCacheFile);
  LOG_DEBUG("Cache file size: %" PRIu64 " bytes\n",CacheFileSize)

  if(CacheFileSize==0) {
    // Empty cache file
    LOG_DEBUG("Generating new header segments\n")
    // Calculate how many blocks are needed for caching the entire image file
    // and how big the ImageMap must be
    // TODO: Allow custom block size specified on cmd line here
    NeededBlocks=XMOptions.OrigImageSize/XM_CACHEFILE_BLOCKSIZE;
    if((XMOptions.OrigImageSize%XM_CACHEFILE_BLOCKSIZE)!=0) NeededBlocks++;
    ImageMapSize=NeededBlocks*sizeof(TXMImageMapEntry);
    // Alloc memory for cache all file segments
    uint64_t TotalToAlloc=sizeof(TXMCacheFileHeader)+
                          ImageMapSize+
                          sizeof(TXMFileIndex)+
                          sizeof(TXMFileMap);
    LOG_DEBUG("Size of all cache file header segments: %" PRIu64 " bytes\n",
              TotalToAlloc)
    LOG_DEBUG("Cache file header size: %u bytes\n",sizeof(TXMCacheFileHeader))
    LOG_DEBUG("Image map size: %" PRIu64 " bytes (%" PRIu64 " addressable " \
              "blocks holding %u bytes each)\n",ImageMapSize,NeededBlocks,
              XM_CACHEFILE_BLOCKSIZE)
    LOG_DEBUG("File index size: %u bytes\n",sizeof(TXMFileIndex))
    LOG_DEBUG("File map size: %u bytes\n",sizeof(TXMFileMap))
    XMOUNT_MALLOC(pCacheFile->pCacheFileHeader,
                  pTXMCacheFileHeader,
                  TotalToAlloc)
    // Set segment pointers and init with default values
    TotalToAlloc-=sizeof(TXMFileMap);
    pCacheFile->pFileMap=
      (pTXMFileMap)(((void*)(pCacheFile->pCacheFileHeader))+TotalToAlloc);
    memset(pCacheFile->pFileMap,0xffff,sizeof(TXMFileMap));
    TotalToAlloc-=sizeof(TXMFileIndex);
    pCacheFile->pFileIndex=
      (pTXMFileIndex)(((void*)(pCacheFile->pCacheFileHeader))+TotalToAlloc);
    memset(pCacheFile->pFileIndex,0x0000,sizeof(TXMFileIndex));
    TotalToAlloc-=ImageMapSize;
    pCacheFile->pImageMap=
      (pTXMImageMapEntry)(((void*)(pCacheFile->pCacheFileHeader))+TotalToAlloc);
    memset(pCacheFile->pImageMap,0xffff,ImageMapSize);
    memset(pCacheFile->pCacheFileHeader,0x0000,sizeof(TXMCacheFileHeader));
    // Set header values
    strcpy(pCacheFile->pCacheFileHeader->signature,XM_CACHEFILE_SIGNATURE);
    pCacheFile->pCacheFileHeader->version=XM_CACHEFILE_CURVERSION;
    pCacheFile->pCacheFileHeader->blocksize=XM_CACHEFILE_BLOCKSIZE;
    pCacheFile->pCacheFileHeader->imagesize=XMOptions.OrigImageSize;
    pCacheFile->pCacheFileHeader->hashsize=XMOptions.OrigImageHashSize;
    strncpy(pCacheFile->pCacheFileHeader->imagehash,
            XMOptions.pOrigImageHash,
            16);
    pCacheFile->pCacheFileHeader->off_imagemap=sizeof(TXMCacheFileHeader);
    LOG_DEBUG("Offset to image map: %u bytes\n",sizeof(TXMCacheFileHeader))
    pCacheFile->pCacheFileHeader->off_fileindex=
      sizeof(TXMCacheFileHeader)+ImageMapSize;
    LOG_DEBUG("Offset to file index: %" PRIu64 " bytes\n",
              sizeof(TXMCacheFileHeader)+ImageMapSize)
    pCacheFile->pCacheFileHeader->off_filemap=
      sizeof(TXMCacheFileHeader)+ImageMapSize+sizeof(TXMFileIndex);
    LOG_DEBUG("Offset to file map: %" PRIu64 " bytes\n",
              sizeof(TXMCacheFileHeader)+ImageMapSize+sizeof(TXMFileIndex))
    // Write all header segments to cache file
    if(fwrite(pCacheFile->pCacheFileHeader,
              sizeof(TXMCacheFileHeader)+
                ImageMapSize+
                sizeof(TXMFileIndex)+
                sizeof(TXMFileMap),
              1,
              pCacheFile->hCacheFile)!=1)
    {
      free(pCacheFile->hCacheFile);
      LOG_ERROR("Couldn't write cache file header segments to file!\n");
      return 0;
    }
    // Now flush all buffers to make sure data is written to file
    fflush(pCacheFile->hCacheFile);
    ioctl(fileno(pCacheFile->hCacheFile),BLKFLSBUF,0);
    LOG_DEBUG("Successfully generated and written cache file header " \
              "segments\n");
  } else {
    // Trying to load existing cache file
    LOG_DEBUG("Trying to load existing cache file\n")
    if(fseeko(pCacheFile->hCacheFile,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to beginning of cache file!\n")
      return 0;
    }
    // Read and check file signature
    if(fread(&tmp_sig,8,1,pCacheFile->hCacheFile)!=1) {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return 0;
    } else if(strcmp(tmp_sig,XM_CACHEFILE_SIGNATURE)!=0) {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return 0;
    }
    // Read and check file version
    if(fread(&tmp_vers,4,1,pCacheFile->hCacheFile)!=1) {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Not an xmount cache file or cache file corrupt!\n")
      return 0;
    } else if(tmp_vers<XM_CACHEFILE_MINVERSION) {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Cache file version %u isn't supported any more! " \
                "Please use xmount-tool to convert it to the actual version.\n",
                tmp_vers)
      return 0;
    }
    // It should now be reasonably secure to load the cache file header
    if(fseeko(pCacheFile->hCacheFile,0,SEEK_SET)!=0) {
      LOG_ERROR("Couldn't seek to beginning of cache file!\n")
      return 0;
    }
    XMOUNT_MALLOC(pCacheFile->pCacheFileHeader,
                  pTXMCacheFileHeader,
                  sizeof(TXMCacheFileHeader))
    if(fread(pCacheFile->pCacheFileHeader,
             sizeof(TXMCacheFileHeader),
             1,
             pCacheFile->hCacheFile)!=1)
    {
      fclose(pCacheFile->hCacheFile);
      free(pCacheFile->hCacheFile);
      LOG_ERROR("Couldn't read cache file header!\n")
      return 0;
    }
    // Compare image size with that saved in cache file
    if(pCacheFile->pCacheFileHeader->imagesize!=XMOptions.OrigImageSize) {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Input image size (%" PRIu64 " bytes) does not match the size "
                "saved in the cache file (%" PRIu64 " bytes)!\n",
                XMOptions.OrigImageSize,pCacheFile->pCacheFileHeader->imagesize)
      return 0;
    }
    // Calculate how many blocks are needed for caching the entire image file
    // and how big the ImageMap must be
    NeededBlocks=
      XMOptions.OrigImageSize/pCacheFile->pCacheFileHeader->blocksize;
    if((XMOptions.OrigImageSize%pCacheFile->pCacheFileHeader->blocksize)!=0)
      NeededBlocks++;
    ImageMapSize=NeededBlocks*sizeof(TXMImageMapEntry);
    // Alloc memory to cache all header segments
    uint64_t TotalToAlloc=sizeof(TXMCacheFileHeader)+
                          ImageMapSize+
                          sizeof(TXMFileIndex)+
                          sizeof(TXMFileMap);
    LOG_DEBUG("Size of all cache file header segments: %" PRIu64 " bytes\n",
              TotalToAlloc)
    LOG_DEBUG("Cache file header size: %u bytes\n",sizeof(TXMCacheFileHeader))
    LOG_DEBUG("Image map size: %" PRIu64 " bytes (%" PRIu64 " addressable " \
              "blocks holding %u bytes each)\n",ImageMapSize,NeededBlocks,
              XM_CACHEFILE_BLOCKSIZE)
    LOG_DEBUG("File index size: %u bytes\n",sizeof(TXMFileIndex))
    LOG_DEBUG("File map size: %u bytes\n",sizeof(TXMFileMap))
    XMOUNT_REALLOC(pCacheFile->pCacheFileHeader,
                   pTXMCacheFileHeader,
                   TotalToAlloc)
    // Set segment pointers and init with data from file
    TotalToAlloc-=sizeof(TXMFileMap);
    pCacheFile->pFileMap=
      (pTXMFileMap)(((void*)(pCacheFile->pCacheFileHeader))+TotalToAlloc);
    TotalToAlloc-=sizeof(TXMFileIndex);
    pCacheFile->pFileIndex=
      (pTXMFileIndex)(((void*)(pCacheFile->pCacheFileHeader))+TotalToAlloc);
    TotalToAlloc-=ImageMapSize;
    pCacheFile->pImageMap=
      (pTXMImageMapEntry)(((void*)(pCacheFile->pCacheFileHeader))+TotalToAlloc);
    // Read data from cache file into memory
    if(fread(pCacheFile->pImageMap,
             ImageMapSize,
             1,
             pCacheFile->hCacheFile)!=1)
    {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Couldn't read image map from cache file!\n")
      return 0;
    }
    if(fread(pCacheFile->pFileIndex,
             sizeof(TXMFileIndex),
             1,
             pCacheFile->hCacheFile)!=1)
    {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Couldn't read file index from cache file!\n")
      return 0;
    }
    if(fread(pCacheFile->pFileMap,
             sizeof(TXMFileMap),
             1,
             pCacheFile->hCacheFile)!=1)
    {
      fclose(pCacheFile->hCacheFile);
      LOG_ERROR("Couldn't read file map from cache file!\n")
      return 0;
    }
    // TODO: Read all file maps into memory
    LOG_DEBUG("Successfully loaded cache file\n")
  }

  // Init cache mutex
  pthread_mutex_init(&(pCacheFile->mutex_rw),NULL);

  return 1;
}

/******************************* xmcache_close ********************************/
void xmcache_close(pTXMCacheFile pCacheFile) {
  // Flush all buffers to make sure data is written to file
  fflush(pCacheFile->hCacheFile);
  ioctl(fileno(pCacheFile->hCacheFile),BLKFLSBUF,0);
  // Close file handle
  fclose(pCacheFile->hCacheFile);
  // Destroy cache mutex
  pthread_mutex_destroy(&(pCacheFile->mutex_rw));
  // Free buffers
  free(pCacheFile->pCacheFileHeader);
  // TODO: Free all entrys !!!
  //free(pCacheFile->pImageMap);
  //free(pCacheFile->pFileIndex);
  //free(pCacheFile->pFileMap);
}

/*************************** xmcache_get_blocksize ****************************/
uint64_t xmcache_get_blocksize(pTXMCacheFile pCacheFile) {
  return pCacheFile->pCacheFileHeader->blocksize;
}

/************************** xmcache_is_block_cached ***************************/
int xmcache_is_block_cached(pTXMCacheFile pCacheFile, uint64_t block) {
  if(pCacheFile->pImageMap[block].off_data==
     XM_CACHEFILE_IMAGEENTRY_UNASSIGNED)
  {
    LOG_DEBUG("Block %" PRIu64 " isn't assigned yet\n",block)
    return 0;
  } else {
    LOG_DEBUG("Block %" PRIu64 " is already assigned\n",block)
    return 0;
  }
}

/***************************** xmcache_image_read *****************************/
int xmcache_image_read(pTXMCacheFile pCacheFile,
                       char *buf,
                       uint64_t block,
                       uint64_t offset,
                       uint64_t size)
{
  // Wait for other threads to end reading/writing data
  pthread_mutex_lock(&(pCacheFile->mutex_rw));
  LOG_DEBUG("Trying to read %" PRIu64 " bytes at block offset %" PRIu64
            " from block %" PRIu64 "\n",size,offset,block)
  if(pCacheFile->pImageMap[block].off_data!=
     XM_CACHEFILE_IMAGEENTRY_UNASSIGNED)
  {
    // Seek to correct place in cache file
    if(fseeko(pCacheFile->hCacheFile,
              pCacheFile->pImageMap[block].off_data+offset,
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to offset %" PRIu64 " of block %" PRIu64 "!\n",
      offset,block)
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    // Adjusting size if trying to read beyond block
    if((size+offset)>pCacheFile->pCacheFileHeader->blocksize)
      size=pCacheFile->pCacheFileHeader->blocksize-offset;
    // Read data from cache
    if(fread(buf,size,1,pCacheFile->hCacheFile)!=1) {
      LOG_ERROR("Couldn't read %" PRIu64 " bytes at block offset %" PRIu64
                " from block %" PRIu64 " at file offset %" PRIu64 "!\n",
                size,offset,block,pCacheFile->pImageMap[block].off_data)
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    LOG_DEBUG("Read %" PRIu64 " bytes\n",size)
    pthread_mutex_unlock(&(pCacheFile->mutex_rw));
    return size;
  } else {
    LOG_ERROR("Attempt to read from non assigned block %" PRIu64 "!\n",block)
    pthread_mutex_unlock(&(pCacheFile->mutex_rw));
    return -1;
  }
}

/**************************** xmcache_image_write *****************************/
int xmcache_image_write(pTXMCacheFile pCacheFile,
                        char *buf,
                        uint64_t block,
                        uint64_t offset,
                        uint64_t size)
{
  // Wait for other threads to end reading/writing data
  pthread_mutex_lock(&(pCacheFile->mutex_rw));
  LOG_DEBUG("Trying to write %" PRIu64 " bytes at block offset %" PRIu64
            " to block %" PRIu64 "\n",size,offset,block)
  if(pCacheFile->pImageMap[block].off_data!=
     XM_CACHEFILE_IMAGEENTRY_UNASSIGNED)
  {
    // Seek to correct place in cache file
    if(fseeko(pCacheFile->hCacheFile,
              pCacheFile->pImageMap[block].off_data+offset,
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to offset %" PRIu64 " of block %" PRIu64 "!\n",
      offset,block)
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    // Adjusting size if trying to write beyond block
    if((size+offset)>pCacheFile->pCacheFileHeader->blocksize)
      size=pCacheFile->pCacheFileHeader->blocksize-offset;
    if(fwrite(buf,size,1,pCacheFile->hCacheFile)!=1) {
      LOG_ERROR("Couldn't write %" PRIu64 " bytes at block offset %" PRIu64
                " to block %" PRIu64 " at file offset %" PRIu64 "!\n",
                size,offset,block,pCacheFile->pImageMap[block].off_data)
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    LOG_DEBUG("Wrote %" PRIu64 " bytes\n",size)
    pthread_mutex_unlock(&(pCacheFile->mutex_rw));
    return size;
  } else {
    // Allocate new block. When allocating new blocks, the first write must
    // fill the whole block with data!
    if(size!=pCacheFile->pCacheFileHeader->blocksize || offset!=0) {
      LOG_ERROR("Attempt to write partial data to unallocated block!\n")
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    // Seek to end of cache file
    if(fseeko(pCacheFile->hCacheFile,
              0,
              SEEK_END)!=0)
    {
      LOG_ERROR("Couldn't seek to end of cache file!\n")
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    // Save offset for new block to image map
    pCacheFile->pImageMap[block].off_data=ftello(pCacheFile->hCacheFile);
    // Write new block
    if(fwrite(buf,size,1,pCacheFile->hCacheFile)!=1) {
      LOG_ERROR("Couldn't write %" PRIu64 " bytes at block offset %" PRIu64
                " to block %" PRIu64 " at file offset %" PRIu64 "!\n",
                size,offset,block,pCacheFile->pImageMap[block].off_data)
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    LOG_DEBUG("Wrote %" PRIu64 " bytes to new block at file offset %" PRIu64
              "\n",size,pCacheFile->pImageMap[block].off_data)
    // Now that data has been written, update image map entry on disk
    LOG_DEBUG("Trying to update image map entry %" PRIu64 " at file offset %" \
              PRIu64 "\n",block,pCacheFile->pCacheFileHeader->off_imagemap+
              (block*sizeof(TXMImageMapEntry)))
    if(fseeko(pCacheFile->hCacheFile,
              pCacheFile->pCacheFileHeader->off_imagemap+
                (block*sizeof(TXMImageMapEntry)),
              SEEK_SET)!=0)
    {
      LOG_ERROR("Couldn't seek to image map entry %" PRIu64 "!\n",block)
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    if(fwrite(&(pCacheFile->pImageMap[block].off_data),
       sizeof(TXMImageMapEntry),
       1,
       pCacheFile->hCacheFile)!=1)
    {
      LOG_ERROR("Couldn't update image map entry!\n")
      pthread_mutex_unlock(&(pCacheFile->mutex_rw));
      return -1;
    }
    LOG_DEBUG("Image map entry updated successfully\n")
    pthread_mutex_unlock(&(pCacheFile->mutex_rw));
    return size;
  }
}

/********************************* xmcache_ls *********************************/
char **xmcache_ls(pTXMCacheFile pCacheFile, int ListInternal) {
  char **ppRes=NULL;
  uint64_t i;
  
  // Iterate over all file index entries
  for(i=0;i<XM_CACHEFILE_FILEINDEX_MAXENTRYS;i++) {
    if(pCacheFile->pFileIndex.FileIndexEntrys[i].filepath!='\0' ||
       pCacheFile->pFileIndex.FileIndexEntrys[i].filename!='\0')
    {
      // This entry holds data
      
      
      
      
    }
  }
  
  
  return ppRes;
}













