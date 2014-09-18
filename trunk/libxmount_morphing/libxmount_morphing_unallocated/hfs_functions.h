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

#ifndef HFS_FUNCTIONS_H
#define HFS_FUNCTIONS_H

#include "libxmount_morphing_unallocated.h"

// HFS+ extend
typedef struct s_HfsPlusExtend {
  uint32_t start_block;
  uint32_t block_count;
} __attribute__ ((packed)) ts_HfsPlusExtend, *pts_HfsPlusExtend;

// Needed parts of the HFS+ volume header
#define HFSPLUS_VH_OFFSET 1024
#define HFSPLUS_VH_SIGNATURE 0x482b //"H+"
#define HFSPLUS_VH_VERSION 4
typedef struct s_HfsPlusVH {
  uint16_t signature; // "H+"
  uint16_t version; // Currently 4 for HFS+
  uint32_t unused01[9];
  uint32_t block_size;
  uint32_t total_blocks;
  uint32_t free_blocks;
  uint32_t unused02[15];
  uint64_t alloc_file_size;
  uint32_t alloc_file_clump_size;
  uint32_t alloc_file_total_blocks;
  ts_HfsPlusExtend alloc_file_extends[8];
} __attribute__ ((packed)) ts_HfsPlusVH, *pts_HfsPlusVH;

int ReadHfsPlusHeader(pts_UnallocatedHandle p_unallocated_handle);
int ReadHfsPlusAllocFile(pts_UnallocatedHandle p_unallocated_handle,
                         uint8_t **pp_alloc_file);
int BuildHfsPlusBlockMap(pts_UnallocatedHandle p_unallocated_handle,
                         uint8_t *p_alloc_file);

#endif // HFS_FUNCTIONS_H

