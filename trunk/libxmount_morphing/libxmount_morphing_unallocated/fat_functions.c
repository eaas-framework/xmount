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

#include "fat_functions.h"
#include "libxmount_morphing_unallocated_retvalues.h"

#include <string.h> // For memset

#define LOG_DEBUG(...) {                                  \
    LIBXMOUNT_LOG_DEBUG(p_fat_handle->debug,__VA_ARGS__); \
}

/*
 * ReadFatHeader
 */
int ReadFatHeader(pts_FatHandle p_fat_handle,
                  pts_LibXmountMorphingInputFunctions p_input_functions,
                  uint8_t debug)
{
  pts_FatVH p_fat_vh;
  int ret;
  size_t bytes_read;
  uint32_t root_dir_sectors;
  uint32_t fat_size;
  uint32_t total_sectors;
  uint32_t data_sectors;
  uint32_t cluster_count;

  // Init FAT handle
  memset(p_fat_handle,0,sizeof(ts_FatHandle));
  p_fat_handle->fat_type=FatType_Unknown;
  p_fat_handle->debug=debug;

  LOG_DEBUG("Trying to read FAT volume header\n");

  // Alloc buffer for header
  p_fat_vh=calloc(1,sizeof(ts_FatVH));
  if(p_fat_vh==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Read VH from input image
  ret=p_input_functions->Read(0,
                              (char*)(p_fat_vh),
                              0,
                              sizeof(ts_FatVH),
                              &bytes_read);
  if(ret!=0 || bytes_read!=sizeof(ts_FatVH)) {
    free(p_fat_vh);
    return UNALLOCATED_FAT_CANNOT_READ_HEADER;
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
    return UNALLOCATED_FAT_INVALID_HEADER;
  }

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
    LOG_DEBUG("FAT is of unsupported type FAT12\n");
    free(p_fat_vh);
    return UNALLOCATED_FAT_UNSUPPORTED_FS_TYPE;
  } else if(cluster_count<65525) {
    LOG_DEBUG("FAT is of type FAT16\n");
    p_fat_handle->fat_type=FatType_Fat16;
  } else {
    LOG_DEBUG("FAT is of type FAT32\n");
    p_fat_handle->fat_type=FatType_Fat32;
  }
  // TODO: What about newer version of FAT like exFAT etc... ??

  p_fat_handle->p_fat_vh=p_fat_vh;
  return UNALLOCATED_OK;
}

/*
 * FreeFatHeader
 */
void FreeFatHeader(pts_FatHandle p_fat_handle) {
  if(p_fat_handle==NULL) return;
  if(p_fat_handle->p_fat_vh!=NULL) free(p_fat_handle->p_fat_vh);
  switch(p_fat_handle->fat_type) {
    case FatType_Fat16: {
      if(p_fat_handle->u_fat.p_fat16!=NULL) free(p_fat_handle->u_fat.p_fat16);
      break;
    }
    case FatType_Fat32: {
      if(p_fat_handle->u_fat.p_fat32!=NULL) free(p_fat_handle->u_fat.p_fat32);
      break;
    }
    case FatType_Unknown:
    default:
      break;
  }
}

/*
 * ReadFat
 */
int ReadFat(pts_FatHandle p_fat_handle,
            pts_LibXmountMorphingInputFunctions p_input_functions)
{
  pts_FatVH p_fat_vh=p_fat_handle->p_fat_vh;
  int ret;
  size_t fat_size;
  off_t fat_offset;
  size_t bytes_read;

  LOG_DEBUG("Trying to read FAT\n");

  // Determine FAT size
  if(p_fat_vh->fat16_sectors!=0) fat_size=p_fat_vh->fat16_sectors;
  else fat_size=p_fat_vh->fat32_sectors;
  fat_size*=p_fat_vh->bytes_per_sector;

  // Calculate FAT offset
  fat_offset=p_fat_vh->reserved_sectors*p_fat_vh->bytes_per_sector;

  LOG_DEBUG("FAT consists of %zu bytes starting at offset %zu\n",
            fat_size,
            fat_offset);

  // Alloc buffer and Read FAT
  if(p_fat_handle->fat_type==FatType_Fat32) {
    // Alloc buffer
    p_fat_handle->u_fat.p_fat32=(uint32_t*)calloc(1,fat_size);
    if(p_fat_handle->u_fat.p_fat32==NULL) return UNALLOCATED_MEMALLOC_FAILED;

    // Read FAT
    ret=p_input_functions->Read(0,
                                (char*)(p_fat_handle->u_fat.p_fat32),
                                fat_offset,
                                fat_size,
                                &bytes_read);
    if(ret!=0 || bytes_read!=fat_size) {
      free(p_fat_handle->u_fat.p_fat32);
      p_fat_handle->u_fat.p_fat32=NULL;
      return UNALLOCATED_FAT_CANNOT_READ_FAT;
    }

    // Convert FAT to host endianness
    for(uint64_t i=0;i<fat_size/sizeof(uint32_t);i++) {
      p_fat_handle->u_fat.p_fat32[i]=le32toh(p_fat_handle->u_fat.p_fat32[i]);
    }
  } else {
    // Alloc buffer
    p_fat_handle->u_fat.p_fat16=(uint16_t*)calloc(1,fat_size);
    if(p_fat_handle->u_fat.p_fat16==NULL) return UNALLOCATED_MEMALLOC_FAILED;

    // Read FAT
    ret=p_input_functions->Read(0,
                                (char*)(p_fat_handle->u_fat.p_fat16),
                                fat_offset,
                                fat_size,
                                &bytes_read);
    if(ret!=0 || bytes_read!=fat_size) {
      free(p_fat_handle->u_fat.p_fat16);
      p_fat_handle->u_fat.p_fat16=NULL;
      return UNALLOCATED_FAT_CANNOT_READ_FAT;
    }

    // Convert FAT to host endianness
    for(uint64_t i=0;i<fat_size/sizeof(uint16_t);i++) {
      p_fat_handle->u_fat.p_fat16[i]=le16toh(p_fat_handle->u_fat.p_fat16[i]);
    }
  }

  LOG_DEBUG("FAT read successfully\n");

  return UNALLOCATED_OK;
}

/*
 * BuildFatBlockMap
 */
int BuildFatBlockMap(pts_FatHandle p_fat_handle,
                     uint64_t **pp_free_block_map,
                     uint64_t *p_free_block_map_size,
                     uint64_t *p_block_size)
{
  pts_FatVH p_fat_vh=p_fat_handle->p_fat_vh;
  uint64_t data_offset;
  uint64_t total_clusters;
  uint64_t *p_free_block_map=NULL;
  uint64_t free_block_map_size=0;

  LOG_DEBUG("Searching unallocated FAT clusters\n");

  // Calculate offset of first data cluster
  data_offset=p_fat_vh->reserved_sectors+
    (((p_fat_vh->root_entry_count*32)+(p_fat_vh->bytes_per_sector-1))/
    p_fat_vh->bytes_per_sector);
  if(p_fat_vh->fat16_sectors!=0) {
    data_offset+=(p_fat_vh->fat_count*p_fat_vh->fat16_sectors);
  } else {
    data_offset+=(p_fat_vh->fat_count*p_fat_vh->fat32_sectors);
  }
  data_offset*=p_fat_vh->bytes_per_sector;

  // Calculate total amount of data clusters
  if(p_fat_vh->total_sectors_16!=0) total_clusters=p_fat_vh->total_sectors_16;
  else total_clusters=p_fat_vh->total_sectors_32;
  total_clusters-=(data_offset/p_fat_vh->bytes_per_sector);
  total_clusters/=p_fat_vh->sectors_per_cluster;
  // Add 2 clusters as clusters 0 and 1 do not exist
  total_clusters+=2;

  LOG_DEBUG("Filesystem contains a total of %" PRIu64 " (2-%" PRIu64 ") "
              " data clusters starting at offset %" PRIu64 "\n",
            total_clusters-2,
            total_clusters-1,
            data_offset);

  // Save offset of every unallocated cluster in block map
  // Clusters 0 and 1 can not hold data in a FAT fs
  if(p_fat_handle->fat_type==FatType_Fat32) {
    for(uint64_t cur_cluster=2;cur_cluster<total_clusters;cur_cluster++) {
      if((p_fat_handle->u_fat.p_fat32[cur_cluster] & 0x0FFFFFFF)==0 ||
         (p_fat_handle->u_fat.p_fat32[cur_cluster] & 0x0FFFFFFF)==0x0FFFFFF7)
      {
        p_free_block_map=realloc(p_free_block_map,
                                 (free_block_map_size+1)*sizeof(uint64_t));
        if(p_free_block_map==NULL) return UNALLOCATED_MEMALLOC_FAILED;
        p_free_block_map[free_block_map_size]=data_offset+((cur_cluster-2)*
          p_fat_vh->bytes_per_sector*p_fat_vh->sectors_per_cluster);

        LOG_DEBUG("Cluster %" PRIu64 " is unallocated "
                    "(FAT value 0x%04X, Image offset %" PRIu64 ")\n",
                  cur_cluster,
                  p_fat_handle->u_fat.p_fat32[cur_cluster],
                  p_free_block_map[free_block_map_size]);

        free_block_map_size++;
      } else {
        LOG_DEBUG("Cluster %" PRIu64 " is allocated (FAT value 0x%08X)\n",
                  cur_cluster,
                  p_fat_handle->u_fat.p_fat32[cur_cluster]);
      }
    }
  } else {
    for(uint64_t cur_cluster=2;cur_cluster<total_clusters;cur_cluster++) {
      if((p_fat_handle->u_fat.p_fat16[cur_cluster] & 0x0FFF)==0 ||
         (p_fat_handle->u_fat.p_fat16[cur_cluster] & 0x0FFF)==0x0FF7)
      {
        p_free_block_map=realloc(p_free_block_map,
                                 (free_block_map_size+1)*sizeof(uint64_t));
        if(p_free_block_map==NULL) return UNALLOCATED_MEMALLOC_FAILED;
        p_free_block_map[free_block_map_size]=data_offset+((cur_cluster-2)*
          p_fat_vh->sectors_per_cluster*p_fat_vh->bytes_per_sector);

        LOG_DEBUG("Cluster %" PRIu64 " is unallocated "
                    "(FAT value 0x%04X, Image offset %" PRIu64 ")\n",
                  cur_cluster,
                  p_fat_handle->u_fat.p_fat16[cur_cluster],
                  p_free_block_map[free_block_map_size]);
  
        free_block_map_size++;
      } else {
        LOG_DEBUG("Cluster %" PRIu64 " is allocated (FAT value 0x%04X)\n",
                  cur_cluster,
                  p_fat_handle->u_fat.p_fat16[cur_cluster]);
      }
    }
  }

  LOG_DEBUG("Found %" PRIu64 " unallocated FAT clusters\n",
            free_block_map_size);

  // Free FAT as it is no longer needed
  if(p_fat_handle->fat_type==FatType_Fat32) {
    free(p_fat_handle->u_fat.p_fat32);
    p_fat_handle->u_fat.p_fat32=NULL;
  } else {
    free(p_fat_handle->u_fat.p_fat16);
    p_fat_handle->u_fat.p_fat16=NULL;
  }

  // TODO: There may be trailing unclustered sectors. Add them?

  *pp_free_block_map=p_free_block_map;
  *p_free_block_map_size=free_block_map_size;
  *p_block_size=p_fat_vh->bytes_per_sector*p_fat_vh->sectors_per_cluster;
  return UNALLOCATED_OK;
}

/*
 * GetFatInfos
 */
int GetFatInfos(pts_FatHandle p_fat_handle, char **pp_buf) {
  pts_FatVH p_fat_vh=p_fat_handle->p_fat_vh;
  int ret;
  char *p_buf=NULL;
  char *p_fat_type;

  switch(p_fat_handle->fat_type) {
    case FatType_Fat16:
      p_fat_type="FAT16";
      break;
    case FatType_Fat32:
      p_fat_type="FAT32";
      break;
    case FatType_Unknown:
    default:
      p_fat_type="Unknown";
  }

  ret=asprintf(&p_buf,
               "FAT filesystem type: %s\n"
                 "FAT bytes per sector: %" PRIu16 "\n"
                 "FAT sectors per cluster: %" PRIu8 "\n"
                 "FAT reserved sectors: %" PRIu16 "\n"
                 "FAT count: %" PRIu8 "\n"
                 "FAT root entry count: %" PRIu16 "\n"
                 "FAT media type: 0x%02X\n"
                 "FAT total sector count (16bit): %" PRIu16 "\n"
                 "FAT sectors per FAT (16bit): %" PRIu16 "\n"
                 "FAT total sector count (32bit): %" PRIu32 "\n"
                 "FAT sectors per FAT (32bit): %" PRIu32,
               p_fat_type,
               p_fat_vh->bytes_per_sector,
               p_fat_vh->sectors_per_cluster,
               p_fat_vh->reserved_sectors,
               p_fat_vh->fat_count,
               p_fat_vh->root_entry_count,
               p_fat_vh->media_type,
               p_fat_vh->total_sectors_16,
               p_fat_vh->fat16_sectors,
               p_fat_vh->total_sectors_32,
               p_fat_vh->fat32_sectors);

  // Check if asprintf worked
  if(ret<0 || p_buf==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  *pp_buf=p_buf;
  return UNALLOCATED_OK;
}

