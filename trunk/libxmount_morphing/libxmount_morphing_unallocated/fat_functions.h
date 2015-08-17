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

#ifndef FAT_FUNCTIONS_H
#define FAT_FUNCTIONS_H

#include "../libxmount_morphing.h"

// Supported FAT types
typedef enum e_FatType {
  FatType_Unknown=0,
  FatType_Fat16,
  FatType_Fat32
} te_FatType;

// Needed parts of the FAT volume header
typedef struct s_FatVH {
  uint8_t jump_inst[3];
  uint8_t oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fat_count;
  uint16_t root_entry_count;
  uint16_t total_sectors_16;
  uint8_t media_type;
  uint16_t fat16_sectors;
  uint64_t unused01;
  uint32_t total_sectors_32;
  // Following value is only valid for FAT32
  uint32_t fat32_sectors;
} __attribute__ ((packed)) ts_FatVH, *pts_FatVH;

// FAT handle
typedef struct s_FatHandle {
  te_FatType fat_type;
  pts_FatVH p_fat_vh;
  union {
    uint16_t *p_fat16;
    uint32_t *p_fat32;
  } u_fat;
  uint8_t debug;
} ts_FatHandle, *pts_FatHandle;

int ReadFatHeader(pts_FatHandle p_fat_handle,
                  pts_LibXmountMorphingInputFunctions p_input_functions,
                  uint8_t debug);
void FreeFatHeader(pts_FatHandle p_fat_handle);
int ReadFat(pts_FatHandle p_fat_handle,
            pts_LibXmountMorphingInputFunctions p_input_functions);
int BuildFatBlockMap(pts_FatHandle p_fat_handle,
                     uint64_t **pp_free_block_map,
                     uint64_t *p_free_block_map_size,
                     uint64_t *p_block_size);
int GetFatInfos(pts_FatHandle p_fat_handle, char **pp_buf);

#endif // FAT_FUNCTIONS_H

