/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

/*
 * ReadHfsPlusHeader
 */
int ReadHfsPlusHeader(pts_UnallocatedHandle p_unallocated_handle) {
  pts_HfsPlusVH p_hfsplus_vh;
  int ret;
  size_t bytes_read;
  pts_HfsPlusExtend p_extend;

  LOG_DEBUG("Trying to read HFS+ volume header\n");

  // Alloc buffer for header
  p_hfsplus_vh=calloc(1,sizeof(ts_HfsPlusVH));
  if(p_hfsplus_vh==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Read VH from input image
  ret=p_unallocated_handle->
        p_input_functions->
          Read(0,
               (char*)(p_hfsplus_vh),
               HFSPLUS_VH_OFFSET,
               sizeof(ts_HfsPlusVH),
               &bytes_read);
  if(ret!=0 || bytes_read!=sizeof(ts_HfsPlusVH)) {
    free(p_hfsplus_vh);
    p_hfsplus_vh=NULL;
    return UNALLOCATED_CANNOT_READ_HFSPLUS_HEADER;
  }

  // Convert VH to host endianness (HFS values are always stored in big endian)
  p_hfsplus_vh->signature=be16toh(p_hfsplus_vh->signature);
  p_hfsplus_vh->version=be16toh(p_hfsplus_vh->version);
  p_hfsplus_vh->block_size=be32toh(p_hfsplus_vh->block_size);
  p_hfsplus_vh->total_blocks=be32toh(p_hfsplus_vh->total_blocks);
  p_hfsplus_vh->free_blocks=be32toh(p_hfsplus_vh->free_blocks);
  p_hfsplus_vh->alloc_file_size=be64toh(p_hfsplus_vh->alloc_file_size);
  p_hfsplus_vh->alloc_file_clump_size=
    be32toh(p_hfsplus_vh->alloc_file_clump_size);
  p_hfsplus_vh->alloc_file_total_blocks=
    be32toh(p_hfsplus_vh->alloc_file_total_blocks);
  for(int i=0;i<8;i++) {
    p_extend=&(p_hfsplus_vh->alloc_file_extends[i]);
    p_extend->start_block=be32toh(p_extend->start_block);
    p_extend->block_count=be32toh(p_extend->block_count);
  }

  LOG_DEBUG("HFS+ VH signature: 0x%04X\n",p_hfsplus_vh->signature);
  LOG_DEBUG("HFS+ VH version: %" PRIu16 "\n",p_hfsplus_vh->version);
  LOG_DEBUG("HFS+ block size: %" PRIu32 " bytes\n",p_hfsplus_vh->block_size);
  LOG_DEBUG("HFS+ total blocks: %" PRIu32 "\n",p_hfsplus_vh->total_blocks);
  LOG_DEBUG("HFS+ free blocks: %" PRIu32 "\n",p_hfsplus_vh->free_blocks);
  LOG_DEBUG("HFS+ allocation file size: %" PRIu64 " bytes\n",
            p_hfsplus_vh->alloc_file_size);
  LOG_DEBUG("HFS+ allocation file blocks: %" PRIu32 "\n",
            p_hfsplus_vh->alloc_file_total_blocks);

  // Check header signature and version
  if(p_hfsplus_vh->signature!=HFSPLUS_VH_SIGNATURE ||
     p_hfsplus_vh->version!=HFSPLUS_VH_VERSION)
  {
    free(p_hfsplus_vh);
    p_hfsplus_vh=NULL;
    return UNALLOCATED_INVALID_HFSPLUS_HEADER;
  }

  LOG_DEBUG("HFS+ volume header read successfully\n");

  p_unallocated_handle->p_hfsplus_vh=p_hfsplus_vh;
  return UNALLOCATED_OK;
}

/*
 * ReadHfsPlusAllocFile
 */
int ReadHfsPlusAllocFile(pts_UnallocatedHandle p_unallocated_handle,
                         uint8_t **pp_alloc_file)
{
  pts_HfsPlusVH p_hfsplus_vh=p_unallocated_handle->p_hfsplus_vh;
  uint8_t *p_alloc_file;
  pts_HfsPlusExtend p_extend;
  int ret;
  char *p_buf;
  size_t bytes_read;
  uint64_t total_bytes_read=0;

  LOG_DEBUG("Trying to read HFS+ allocation file\n");

  // Alloc buffer for file
  p_alloc_file=calloc(1,p_hfsplus_vh->alloc_file_size);
  if(p_alloc_file==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Loop over extends and read data
  p_buf=(char*)(p_alloc_file);
  for(int i=0;i<8;i++) {
    p_extend=&(p_hfsplus_vh->alloc_file_extends[i]);

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
                p_hfsplus_vh->block_size,
                p_extend->start_block+ii,
                (uint64_t)((p_extend->start_block+ii)*
                  p_hfsplus_vh->block_size));

      ret=p_unallocated_handle->
            p_input_functions->
              Read(0,
                   p_buf,
                   (p_extend->start_block+ii)*p_hfsplus_vh->block_size,
                   p_hfsplus_vh->block_size,
                   &bytes_read);
      if(ret!=0 || bytes_read!=p_hfsplus_vh->block_size) {
        free(p_alloc_file);
        return UNALLOCATED_CANNOT_READ_HFSPLUS_ALLOC_FILE;
      }
      p_buf+=p_hfsplus_vh->block_size;
      total_bytes_read+=p_hfsplus_vh->block_size;
    }
  }

  // Alloc files with more than 8 extends aren't supported yet
  if(total_bytes_read!=p_hfsplus_vh->alloc_file_size) {
    free(p_alloc_file);
    return UNALLOCATED_ALLOC_FILE_HAS_TOO_MUCH_EXTENDS;
  }

  LOG_DEBUG("HFS+ allocation file read successfully\n");

  *pp_alloc_file=p_alloc_file;
  return UNALLOCATED_OK;
}

/*
 * BuildHfsPlusBlockMap
 */
int BuildHfsPlusBlockMap(pts_UnallocatedHandle p_unallocated_handle,
                         uint8_t *p_alloc_file)
{
  pts_HfsPlusVH p_hfsplus_vh=p_unallocated_handle->p_hfsplus_vh;

  LOG_DEBUG("Searching unallocated HFS+ blocks\n");

  // Save offset of every unallocated block in block map
  for(uint32_t cur_block=0;
      cur_block<p_hfsplus_vh->total_blocks;
      cur_block++)
  {
    if((p_alloc_file[cur_block/8] & (1<<(7-(cur_block%8))))==0) {
      p_unallocated_handle->p_free_block_map=
        realloc(p_unallocated_handle->p_free_block_map,
                (p_unallocated_handle->free_block_map_size+1)*sizeof(uint64_t));
      if(p_unallocated_handle->p_free_block_map==NULL) {
        p_unallocated_handle->free_block_map_size=0;
        return UNALLOCATED_MEMALLOC_FAILED;
      }
      p_unallocated_handle->
        p_free_block_map[p_unallocated_handle->free_block_map_size]=
          cur_block*p_hfsplus_vh->block_size;
      p_unallocated_handle->free_block_map_size++;
    }
  }

  LOG_DEBUG("Found %" PRIu64 " unallocated HFS+ blocks\n",
            p_unallocated_handle->free_block_map_size);

  if(p_hfsplus_vh->free_blocks!=p_unallocated_handle->free_block_map_size) {
      LOG_WARNING("According to VH, there should be %" PRIu64
                    " unallocated blocks but I found %" PRIu64 "\n",
                  p_hfsplus_vh->free_blocks,
                  p_unallocated_handle->free_block_map_size);
  }

  // Save used block size in handle and return
  p_unallocated_handle->block_size=p_hfsplus_vh->block_size;
  return UNALLOCATED_OK;
}

