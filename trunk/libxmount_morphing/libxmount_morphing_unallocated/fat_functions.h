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

#ifndef FAT_FUNCTIONS_H
#define FAT_FUNCTIONS_H

#include "libxmount_morphing_unallocated.h"

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

int ReadFatHeader(pts_UnallocatedHandle p_unallocated_handle);

#endif // FAT_FUNCTIONS_H

