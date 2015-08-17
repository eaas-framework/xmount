/*******************************************************************************
* xmount Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#include "hfs_functions.h"
#include "libxmount_morphing_unallocated_retvalues.h"

#define LOG_DEBUG(...) {                                  \
    LIBXMOUNT_LOG_DEBUG(p_hfs_handle->debug,__VA_ARGS__); \
}

/*
 * ReadHfsHeader
 */
int ReadHfsHeader(pts_HfsHandle p_hfs_handle,
                  pts_LibXmountMorphingInputFunctions p_input_functions,
                  uint8_t debug)
{
  pts_HfsVH p_hfs_vh;
  int ret;
  size_t bytes_read;
  pts_HfsExtend p_extend;

  // Init HFS handle
  p_hfs_handle->p_hfs_vh=NULL;
  p_hfs_handle->p_alloc_file=NULL;
  p_hfs_handle->debug=debug;

  LOG_DEBUG("Trying to read HFS volume header\n");

  // Alloc buffer for header
  p_hfs_vh=calloc(1,sizeof(ts_HfsVH));
  if(p_hfs_vh==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Read VH from input image
  ret=p_input_functions->Read(0,
                              (char*)(p_hfs_vh),
                              HFS_VH_OFFSET,
                              sizeof(ts_HfsVH),
                              &bytes_read);
  if(ret!=0 || bytes_read!=sizeof(ts_HfsVH)) {
    free(p_hfs_vh);
    p_hfs_vh=NULL;
    return UNALLOCATED_HFS_CANNOT_READ_HEADER;
  }

  // Convert VH to host endianness (HFS values are always stored in big endian)
  p_hfs_vh->signature=be16toh(p_hfs_vh->signature);
  p_hfs_vh->version=be16toh(p_hfs_vh->version);
  p_hfs_vh->block_size=be32toh(p_hfs_vh->block_size);
  p_hfs_vh->total_blocks=be32toh(p_hfs_vh->total_blocks);
  p_hfs_vh->free_blocks=be32toh(p_hfs_vh->free_blocks);
  p_hfs_vh->alloc_file_size=be64toh(p_hfs_vh->alloc_file_size);
  p_hfs_vh->alloc_file_clump_size=be32toh(p_hfs_vh->alloc_file_clump_size);
  p_hfs_vh->alloc_file_total_blocks=be32toh(p_hfs_vh->alloc_file_total_blocks);
  for(int i=0;i<8;i++) {
    p_extend=&(p_hfs_vh->alloc_file_extends[i]);
    p_extend->start_block=be32toh(p_extend->start_block);
    p_extend->block_count=be32toh(p_extend->block_count);
  }

  LOG_DEBUG("HFS VH signature: 0x%04X\n",p_hfs_vh->signature);
  LOG_DEBUG("HFS VH version: %" PRIu16 "\n",p_hfs_vh->version);
  LOG_DEBUG("HFS block size: %" PRIu32 " bytes\n",p_hfs_vh->block_size);
  LOG_DEBUG("HFS total blocks: %" PRIu32 "\n",p_hfs_vh->total_blocks);
  LOG_DEBUG("HFS free blocks: %" PRIu32 "\n",p_hfs_vh->free_blocks);
  LOG_DEBUG("HFS allocation file size: %" PRIu64 " bytes\n",
            p_hfs_vh->alloc_file_size);
  LOG_DEBUG("HFS allocation file blocks: %" PRIu32 "\n",
            p_hfs_vh->alloc_file_total_blocks);

  // Check header signature and version
  if(p_hfs_vh->signature!=HFS_VH_SIGNATURE ||
     p_hfs_vh->version!=HFS_VH_VERSION)
  {
    free(p_hfs_vh);
    p_hfs_vh=NULL;
    return UNALLOCATED_HFS_INVALID_HEADER;
  }

  // We currently only support HFS+
  p_hfs_handle->hfs_type=HfsType_HfsPlus;

  LOG_DEBUG("HFS volume header read successfully\n");

  p_hfs_handle->p_hfs_vh=p_hfs_vh;
  return UNALLOCATED_OK;
}

/*
 * FreeHfsHeader
 */
void FreeHfsHeader(pts_HfsHandle p_hfs_handle) {
  if(p_hfs_handle==NULL) return;
  if(p_hfs_handle->p_hfs_vh!=NULL) free(p_hfs_handle->p_hfs_vh);
  if(p_hfs_handle->p_alloc_file!=NULL) free(p_hfs_handle->p_alloc_file);
}

/*
 * ReadHfsAllocFile
 */
int ReadHfsAllocFile(pts_HfsHandle p_hfs_handle,
                     pts_LibXmountMorphingInputFunctions p_input_functions)
{
  uint8_t *p_alloc_file;
  pts_HfsExtend p_extend;
  int ret;
  char *p_buf;
  size_t bytes_read;
  uint64_t total_bytes_read=0;

  LOG_DEBUG("Trying to read HFS allocation file\n");

  // Alloc buffer for file
  p_alloc_file=calloc(1,p_hfs_handle->p_hfs_vh->alloc_file_size);
  if(p_alloc_file==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Loop over extends and read data
  p_buf=(char*)(p_alloc_file);
  for(int i=0;i<8;i++) {
    p_extend=&(p_hfs_handle->p_hfs_vh->alloc_file_extends[i]);

    // If start_block and block_count are zero, we parsed last extend
    if(p_extend->start_block==0 && p_extend->block_count==0) break;

    LOG_DEBUG("Extend %d contains %" PRIu32
                " block(s) starting with block %" PRIu32 "\n",
              i,
              p_extend->block_count,
              p_extend->start_block);

    // Read data
    for(uint32_t ii=0;ii<p_extend->block_count;ii++) {
      LOG_DEBUG("Reading %" PRIu32 " bytes from block %" PRIu32
                  " at offset %" PRIu64 "\n",
                p_hfs_handle->p_hfs_vh->block_size,
                p_extend->start_block+ii,
                (uint64_t)((p_extend->start_block+ii)*
                  p_hfs_handle->p_hfs_vh->block_size));

      ret=p_input_functions->Read(0,
                                  p_buf,
                                  (p_extend->start_block+ii)*
                                    p_hfs_handle->p_hfs_vh->block_size,
                                  p_hfs_handle->p_hfs_vh->block_size,
                                  &bytes_read);
      if(ret!=0 || bytes_read!=p_hfs_handle->p_hfs_vh->block_size) {
        free(p_alloc_file);
        return UNALLOCATED_HFS_CANNOT_READ_ALLOC_FILE;
      }
      p_buf+=p_hfs_handle->p_hfs_vh->block_size;
      total_bytes_read+=p_hfs_handle->p_hfs_vh->block_size;
    }
  }

  // Alloc files with more than 8 extends aren't supported yet
  if(total_bytes_read!=p_hfs_handle->p_hfs_vh->alloc_file_size) {
    free(p_alloc_file);
    return UNALLOCATED_HFS_ALLOC_FILE_HAS_TOO_MUCH_EXTENDS;
  }

  LOG_DEBUG("HFS allocation file read successfully\n");

  p_hfs_handle->p_alloc_file=p_alloc_file;
  return UNALLOCATED_OK;
}

/*
 * BuildHfsBlockMap
 */
int BuildHfsBlockMap(pts_HfsHandle p_hfs_handle,
                     uint64_t **pp_free_block_map,
                     uint64_t *p_free_block_map_size,
                     uint64_t *p_block_size)
{
  uint64_t *p_free_block_map=NULL;
  uint64_t free_block_map_size=0;
  
  LOG_DEBUG("Searching unallocated HFS blocks\n");

  // Save offset of every unallocated block in block map
  for(uint32_t cur_block=0;
      cur_block<p_hfs_handle->p_hfs_vh->total_blocks;
      cur_block++)
  {
    if((p_hfs_handle->p_alloc_file[cur_block/8] & (1<<(7-(cur_block%8))))==0) {
      p_free_block_map=realloc(p_free_block_map,
                               (free_block_map_size+1)*sizeof(uint64_t));
      if(p_free_block_map==NULL) return UNALLOCATED_MEMALLOC_FAILED;
      p_free_block_map[free_block_map_size]=
        cur_block*p_hfs_handle->p_hfs_vh->block_size;
      free_block_map_size++;
    }
  }

  LOG_DEBUG("Found %" PRIu64 " unallocated HFS blocks\n",
            free_block_map_size);

  if(p_hfs_handle->p_hfs_vh->free_blocks!=free_block_map_size) {
      LIBXMOUNT_LOG_WARNING("According to VH, there should be %" PRIu64
                              " unallocated blocks but I found %" PRIu64 "\n",
                            p_hfs_handle->p_hfs_vh->free_blocks,
                            free_block_map_size);
  }

  // Free alloc file as it is no longer needed
  free(p_hfs_handle->p_alloc_file);
  p_hfs_handle->p_alloc_file=NULL;

  *pp_free_block_map=p_free_block_map;
  *p_free_block_map_size=free_block_map_size;
  *p_block_size=p_hfs_handle->p_hfs_vh->block_size;
  return UNALLOCATED_OK;
}

/*
 * GetHfsInfos
 */
int GetHfsInfos(pts_HfsHandle p_hfs_handle, char **pp_buf) {
  pts_HfsVH p_hfs_vh=p_hfs_handle->p_hfs_vh;
  char *p_buf=NULL;
  int ret;

  ret=asprintf(&p_buf,
               "HFS filesystem type: HFS+\n"
                 "HFS VH signature: 0x%04X\n"
                 "HFS VH version: %" PRIu16 "\n"
                 "HFS block size: %" PRIu32 " bytes\n"
                 "HFS total blocks: %" PRIu32 "\n"
                 "HFS free blocks: %" PRIu32 "\n"
                 "HFS allocation file size: %" PRIu64 " bytes\n"
                 "HFS allocation file blocks: %" PRIu32,
               p_hfs_vh->signature,
               p_hfs_vh->version,
               p_hfs_vh->block_size,
               p_hfs_vh->total_blocks,
               p_hfs_vh->free_blocks,
               p_hfs_vh->alloc_file_size,
               p_hfs_vh->alloc_file_total_blocks);

  // Check if asprintf worked
  if(ret<0 || p_buf==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  *pp_buf=p_buf;
  return UNALLOCATED_OK;
}

