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

#ifndef NTFS_FUNCTIONS_H
#define NTFS_FUNCTIONS_H

#include "../libxmount_morphing.h"

// Needed parts of the FAT volume header
typedef struct s_NtfsVH {
  uint8_t jump_inst[3];
  uint8_t oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t null01;
  uint8_t null02;
  uint8_t null03;
  uint16_t unused01;
  uint8_t media_type;
  uint16_t null04;
  uint8_t unused02[14];
  uint64_t total_sectors;
  uint64_t mft_cluster;
  uint64_t mftmirr_cluster;
  int16_t clusters_per_file_rec;
  int8_t clusters_per_index_buf;
} __attribute__ ((packed)) ts_NtfsVH, *pts_NtfsVH;

// NTFS handle
typedef struct s_NtfsHandle {
  pts_NtfsVH p_ntfs_vh;
  uint8_t debug;
} ts_NtfsHandle, *pts_NtfsHandle;

int ReadNtfsHeader(pts_NtfsHandle p_ntfs_handle,
                   pts_LibXmountMorphingInputFunctions p_input_functions,
                   uint8_t debug);

#endif // NTFS_FUNCTIONS_H

