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

#include "fat_functions.h"

/*
 * ReadFatHeader
 */
int ReadFatHeader(pts_UnallocatedHandle p_unallocated_handle) {
  pts_FatVH p_fat_vh;
  int ret;
  size_t bytes_read;

  LOG_DEBUG("Trying to read FAT volume header\n");

  // Alloc buffer for header
  p_fat_vh=calloc(1,sizeof(ts_FatVH));
  if(p_fat_vh==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Read VH from input image
  ret=p_unallocated_handle->
        p_input_functions->
          Read(0,
               (char*)(p_fat_vh),
               0,
               sizeof(ts_FatVH),
               &bytes_read);
  if(ret!=0 || bytes_read!=sizeof(ts_FatVH)) {
    free(p_fat_vh);
    p_fat_vh=NULL;
    return UNALLOCATED_CANNOT_READ_HFSPLUS_HEADER;
  }

  // Convert values to host endianness (FAT values are always stored in little
  // endian)
  p_fat_vh->bytes_per_sector=le16toh(p_fat_vh->bytes_per_sector);
  p_fat_vh->reserved_sectors=le16toh(p_fat_vh->reserved_sectors);
  p_fat_vh->root_entry_count=le16toh(p_fat_vh->root_entry_count);
  p_fat_vh->total_sectors_16=le16toh(p_fat_vh->total_sectors_16);
  p_fat_vh->fat16_sectors=le16toh(p_fat_vh->fat16_sectors);
  p_fat_vh->total_sectors_32=le32toh(p_fat_vh->total_sectors_32);
  p_fat_vh->fat32_sectors=le32toh(p_fat_vh->fat32_sectors);

  LOG_DEBUG("FAT VH jump instruction 1: 0x%02X\n",p_fat_vh->jump_inst[0]);
  LOG_DEBUG("FAT bytes per sector: %" PRIu16 "\n",
            p_fat_vh->bytes_per_sector);
  LOG_DEBUG("FAT sectors per cluster: %" PRIu8 "\n",
            p_fat_vh->sectors_per_cluster);
  LOG_DEBUG("FAT reserved sectors: %" PRIu16 "\n",
            p_fat_vh->reserved_sectors);
  LOG_DEBUG("FAT count: %" PRIu8 "\n",p_fat_vh->fat_count);
  LOG_DEBUG("FAT root entry count: %" PRIu16 "\n",
            p_fat_vh->root_entry_count);
  LOG_DEBUG("FAT media type: %02X\n",p_fat_vh->media_type);
  LOG_DEBUG("FAT total sector count (16bit): %" PRIu16 "\n",
            p_fat_vh->total_sectors_16);
  LOG_DEBUG("FAT sectors per FAT (16bit): %" PRIu16 "\n",
            p_fat_vh->fat16_sectors);
  LOG_DEBUG("FAT total sector count (32bit): %" PRIu32 "\n",
            p_fat_vh->total_sectors_32);
  LOG_DEBUG("FAT sectors per FAT (32bit): %" PRIu32 "\n",
            p_fat_vh->fat32_sectors);

  // Check header values
  if((p_fat_vh->jump_inst[0]!=0xEB && p_fat_vh->jump_inst[0]!=0xE9) ||
     p_fat_vh->bytes_per_sector==0 ||
     p_fat_vh->bytes_per_sector%512!=0 ||
     p_fat_vh->sectors_per_cluster==0 ||
     p_fat_vh->sectors_per_cluster%2!=0 ||
     p_fat_vh->reserved_sectors==0 ||
     p_fat_vh->fat_count==0 ||
     (p_fat_vh->total_sectors_16==0 && p_fat_vh->total_sectors_32==0) ||
     (p_fat_vh->total_sectors_16!=0 && p_fat_vh->total_sectors_32!=0))
  {
    free(p_fat_vh);
    p_fat_vh=NULL;
    return UNALLOCATED_INVALID_FAT_HEADER;
  }

  // If FAT type was not specified, try to detect it
  if(p_unallocated_handle->fs_type==UnallocatedFsType_Unknown) {
    uint32_t root_dir_sectors;
    uint32_t fat_size;
    uint32_t total_sectors;
    uint32_t data_sectors;
    uint32_t cluster_count;

    LOG_DEBUG("Determining FAT type\n");

    // Determine the count of sectors occupied by the root directory
    root_dir_sectors=((p_fat_vh->root_entry_count*32)+
      (p_fat_vh->bytes_per_sector-1))/p_fat_vh->bytes_per_sector;

    // Determine the count of sectors in the data region
    if(p_fat_vh->fat16_sectors!=0) fat_size=p_fat_vh->fat16_sectors;
    else fat_size=p_fat_vh->fat32_sectors;
    if(p_fat_vh->total_sectors_16!=0) total_sectors=p_fat_vh->total_sectors_16;
    else total_sectors=p_fat_vh->total_sectors_32;
    data_sectors=total_sectors-(p_fat_vh->reserved_sectors+
      (p_fat_vh->fat_count*fat_size)+root_dir_sectors);

    // Determine the count of clusters
    cluster_count=data_sectors/p_fat_vh->sectors_per_cluster;

    // Determine FAT type
    if(cluster_count<4085) {
      LOG_DEBUG("FAT is of type FAT12\n");
      p_unallocated_handle->fs_type=UnallocatedFsType_Fat12;
    } else if(cluster_count<65525) {
      LOG_DEBUG("FAT is of type FAT16\n");
      p_unallocated_handle->fs_type=UnallocatedFsType_Fat16;
    } else {
      LOG_DEBUG("FAT is of type FAT32\n");
      p_unallocated_handle->fs_type=UnallocatedFsType_Fat32;
    }
  }

  p_unallocated_handle->p_fat_vh=p_fat_vh;
  return UNALLOCATED_OK;
}

