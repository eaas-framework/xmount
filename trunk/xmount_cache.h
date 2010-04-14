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

#ifndef XMOUNT_CACHE_H
#define XMOUNT_CACHE_H

#include <inttypes.h>
#include <stdio.h>
#include <pthread.h>

#define XM_CACHEFILE_CURVERSION 0x03
#define XM_CACHEFILE_MINVERSION 0x03
#define XM_CACHEFILE_BLOCKSIZE (1024*1024) // 1Mb

/*
 * xmount cachefile header
 */
#define XM_CACHEFILE_SIGNATURE "xmount\xFF\xFF"
typedef struct TXMCacheFileHeader {
  /** Cachefile signature (XM_CACHEFILE_SIGNATURE) */
  char signature[8];
  /** Cachefile version */
  uint32_t version;
  /** Used blocksize for caching data */
  uint64_t blocksize;
  /** Size of original image file */
  uint64_t imagesize;
  /** Amount of data to hash for following MD5 */
  uint64_t hashsize;
  /** MD5 hash of partial original image file */
  char imagehash[16];
  /** Offset to first image map entry */
  uint64_t off_imagemap;
  /** Offset to first file index entry */
  uint64_t off_fileindex;
  /** Offset to first file map entry */
  uint64_t off_filemap;
  /** Padding to get 512 byte alignment and for further additions */
  char padding[432];
} TXMCacheFileHeader, *pTXMCacheFileHeader;

/*
 * xmount cachefile imagemap entry
 */
#ifdef __LP64__
  #define XM_CACHEFILE_IMAGEENTRY_UNASSIGNED 0xFFFFFFFFFFFFFFFF
#else
  #define XM_CACHEFILE_IMAGEENTRY_UNASSIGNED 0xFFFFFFFFFFFFFFFFLL
#endif
typedef struct TXMImageMapEntry {
  /** Offset to raw data or 0xFFFFFFFFFFFFFFFF when block isn't assigned */
  uint64_t off_data;
} TXMImageMapEntry, *pTXMImageMapEntry;

/*
 * xmount cachefile file index entry
 */
#define XM_CACHEFILE_FILEINDEX_MAXPATHLEN 512
#define XM_CACHEFILE_FILEINDEX_MAXFILELEN 512
typedef struct TXMFileIndexEntry {
  /** File path */
  char filepath[XM_CACHEFILE_FILEINDEX_MAXPATHLEN];
  /** File name or '\0' when entry represents a folder */
  char filename[XM_CACHEFILE_FILEINDEX_MAXFILELEN];
  /** File size */
  uint64_t filesize;
  /** Number of first file map entry */
  uint32_t first_block;
  /** Flags: */
  /** Bit 1 : Internal (hidden) file (f.ex. vdi header...) */
  /** Bit 2 : File is VDI related */
  /** Bit 4 : File is VMDK related */
  /** Bit 8-265 : unused */
  char flags;
  /** Padding to get 512 byte alignment and for further additions */
  char padding[507];
} TXMFileIndexEntry, *pTXMFileIndexEntry;

/*
 * xmount cachefile file index
 */
#define XM_CACHEFILE_FILEINDEX_MAXENTRYS 682
typedef struct TXMFileIndex {
  /** Room for 682 files / directories */
  TXMFileIndexEntry FileIndexEntrys[XM_CACHEFILE_FILEINDEX_MAXENTRYS];
  /** Padding to get 512 byte alignment */
  char padding[1024];
} TXMFileIndex, *pTXMFileIndex;

/*
 * xmount cachefile filemap entry
 */
#ifdef __LP64__
  #define XM_CACHEFILE_FILEENTRY_UNASSIGNED 0xFFFFFFFFFFFFFFFF
#else
  #define XM_CACHEFILE_FILEENTRY_UNASSIGNED 0xFFFFFFFFFFFFFFFFLL
#endif
#define XM_CACHEFILE_FILEMAPENTRY_LAST 0xFFFFFFFF
#define XM_CACHEFILE_FILEMAPENTRY_UNUSED 0x00000000
typedef struct TXMFileMapEntry {
  /** Offset to raw data or 0xFFFFFFFFFFFFFFFF if block isn't assigned yet */
  uint64_t off_data;
  /** Number of next block belonging to this file, 0xffffffff if this is the */
  /** last block of this file or 0x00000000 if block is assigned but unused */
  uint32_t next_block;
} TXMFileMapEntry, *pTXMFileMapEntry;

/*
 * xmount cachefile filemap
 */
#define XM_CACHEFILE_FILEMAP_LAST 0x00000000
#define XM_CACHEFILE_FILEMAP_MAXENTRYS 87380
typedef struct TXMFileMap {
  /** Room for 87380 cached blocks */
  TXMFileMapEntry FileMapEntrys[XM_CACHEFILE_FILEMAP_MAXENTRYS];
  /** Offset of next TXMFileMap structure if needed or 0x00000000 if this is */
  /** the last one */
  uint64_t off_next_filemap;
  /** Padding to get 512 byte alignment */
  char padding[8];
} TXMFileMap, *pTXMFileMap;

/*
 * Public API
 */

/** Main xmount cache file object */
typedef struct TXMCacheFile {
  FILE *hCacheFile;
  pTXMCacheFileHeader pCacheFileHeader;
  pTXMImageMapEntry pImageMap;
  pTXMFileIndex pFileIndex;
  pTXMFileMap pFileMap;
  pthread_mutex_t mutex_rw;
} TXMCacheFile, *pTXMCacheFile;

/** General functions */
int xmcache_open(pTXMCacheFile pCacheFile, char *pFileName);
void xmcache_close(pTXMCacheFile pCacheFile);
uint64_t xmcache_get_blocksize(pTXMCacheFile pCacheFile);

/** Image cache functions */
int xmcache_is_block_cached(pTXMCacheFile pCacheFile,
                            uint64_t block);
int xmcache_image_read(pTXMCacheFile pCacheFile,
                       char *buf,
                       uint64_t block,
                       uint64_t offset,
                       uint64_t size);
int xmcache_image_write(pTXMCacheFile pCacheFile,
                        char *buf,
                        uint64_t block,
                        uint64_t offset,
                        uint64_t size);

/** Additional cached files functions */
char **xmcache_ls(pTXMCacheFile pCacheFile,
                  int ListInternal);

int xmcache_file_exists(pTXMCacheFile pCacheFile,
                        char *pFileName);

int xmcache_file_create(pTXMCacheFile pCacheFile,
                        char *pFilePath,
                        char *pFileName);

int xmcache_file_delete(pTXMCacheFile pCacheFile,
                        char *pFilePath,
                        char *pFileName);

int xmcache_file_read(pTXMCacheFile pCacheFile,
                      char *pFileName,
                      char *buf,
                      uint64_t offset,
                      uint64_t size);

int xmcache_file_write(pTXMCacheFile pCacheFile,
                       char *pFileName,
                       char *buf,
                       uint64_t offset,
                       uint64_t size);

#endif // #indef XMOUNT_CACHE_H
