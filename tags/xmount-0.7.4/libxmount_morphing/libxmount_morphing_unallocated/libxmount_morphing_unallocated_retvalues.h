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

#ifndef LIBXMOUNT_MORPHING_UNALLOCATED_RETVALUES_H
#define LIBXMOUNT_MORPHING_UNALLOCATED_RETVALUES_H

// Error codes
enum {
  UNALLOCATED_OK=0,
  UNALLOCATED_MEMALLOC_FAILED,
  UNALLOCATED_NO_SUPPORTED_FS_DETECTED,
  UNALLOCATED_UNSUPPORTED_FS_SPECIFIED,
  UNALLOCATED_INTERNAL_ERROR,
  UNALLOCATED_CANNOT_GET_IMAGECOUNT,
  UNALLOCATED_WRONG_INPUT_IMAGE_COUNT,
  UNALLOCATED_CANNOT_GET_IMAGESIZE,
  UNALLOCATED_READ_BEYOND_END_OF_IMAGE,
  UNALLOCATED_CANNOT_READ_DATA,
  UNALLOCATED_CANNOT_PARSE_OPTION,
  // HFS return values
  UNALLOCATED_HFS_CANNOT_READ_HEADER,
  UNALLOCATED_HFS_INVALID_HEADER,
  UNALLOCATED_HFS_CANNOT_READ_ALLOC_FILE,
  UNALLOCATED_HFS_ALLOC_FILE_HAS_TOO_MUCH_EXTENDS,
  // FAT return values
  UNALLOCATED_FAT_CANNOT_READ_HEADER,
  UNALLOCATED_FAT_INVALID_HEADER,
  UNALLOCATED_FAT_UNSUPPORTED_FS_TYPE,
  UNALLOCATED_FAT_CANNOT_READ_FAT
};

#endif // LIBXMOUNT_MORPHING_UNALLOCATED_RETVALUES_H

