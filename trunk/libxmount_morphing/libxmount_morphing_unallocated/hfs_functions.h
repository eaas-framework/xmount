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

#ifndef HFS_FUNCTIONS_H
#define HFS_FUNCTIONS_H

#include "../libxmount_morphing.h"

// Supported HFS types
typedef enum e_HfsType {
  HfsType_HfsPlus=0
} te_HfsType;

// HFS extend
typedef struct s_HfsExtend {
  uint32_t start_block;
  uint32_t block_count;
} __attribute__ ((packed)) ts_HfsExtend, *pts_HfsExtend;

// Needed parts of the HFS volume header
#define HFS_VH_OFFSET 1024
#define HFS_VH_SIGNATURE 0x482b //"H+"
#define HFS_VH_VERSION 4
typedef struct s_HfsVH {
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
  ts_HfsExtend alloc_file_extends[8];
} __attribute__ ((packed)) ts_HfsVH, *pts_HfsVH;

// HFS handle
typedef struct s_HfsHandle {
  te_HfsType hfs_type;
  pts_HfsVH p_hfs_vh;
  uint8_t *p_alloc_file;
  uint8_t debug;
} ts_HfsHandle, *pts_HfsHandle;

int ReadHfsHeader(pts_HfsHandle p_hfs_handle,
                  pts_LibXmountMorphingInputFunctions p_input_functions,
                  uint8_t debug);
void FreeHfsHeader(pts_HfsHandle p_hfs_handle);
int ReadHfsAllocFile(pts_HfsHandle p_hfs_handle,
                     pts_LibXmountMorphingInputFunctions p_input_functions);
int BuildHfsBlockMap(pts_HfsHandle p_hfs_handle,
                     uint64_t **pp_free_block_map,
                     uint64_t *p_free_block_map_size,
                     uint64_t *p_block_size);
int GetHfsInfos(pts_HfsHandle p_hfs_handle, char **pp_buf);

#endif // HFS_FUNCTIONS_H

