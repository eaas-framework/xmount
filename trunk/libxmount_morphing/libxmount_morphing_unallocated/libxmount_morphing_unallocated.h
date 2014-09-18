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

#ifndef LIBXMOUNT_MORPHING_UNALLOCATED_H
#define LIBXMOUNT_MORPHING_UNALLOCATED_H

#include "hfs_functions.h"
#include "fat_functions.h"

#define LOG_ERROR(...) {                             \
  LibXmount_Morphing_LogMessage("ERROR",             \
                                (char*)__FUNCTION__, \
                                __LINE__,            \
                                __VA_ARGS__);        \
}
#define LOG_WARNING(...) {                           \
  LibXmount_Morphing_LogMessage("WARNING",           \
                                (char*)__FUNCTION__, \
                                __LINE__,            \
                                __VA_ARGS__);        \
}
#define LOG_DEBUG(...) {                               \
  if(p_unallocated_handle->debug==1)                   \
    LibXmount_Morphing_LogMessage("DEBUG",             \
                                  (char*)__FUNCTION__, \
                                  __LINE__,            \
                                  __VA_ARGS__);        \
}

/*******************************************************************************
 * Enums, type defs, etc...
 ******************************************************************************/
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
  UNALLOCATED_CANNOT_READ_HFSPLUS_HEADER,
  UNALLOCATED_INVALID_HFSPLUS_HEADER,
  UNALLOCATED_CANNOT_READ_HFSPLUS_ALLOC_FILE,
  UNALLOCATED_ALLOC_FILE_HAS_TOO_MUCH_EXTENDS,
  UNALLOCATED_INVALID_FAT_HEADER
};

// Supported fs types
typedef enum e_UnallocatedFsType {
  UnallocatedFsType_Unknown=0,
  UnallocatedFsType_HfsPlus,
  UnallocatedFsType_Fat12,
  UnallocatedFsType_Fat16,
  UnallocatedFsType_Fat32
} te_UnallocatedFsType;

// Handle
typedef struct s_UnallocatedHandle {
  uint8_t debug;
  te_UnallocatedFsType fs_type;
  pts_LibXmountMorphingInputFunctions p_input_functions;

  uint64_t block_size;
  uint64_t free_block_map_size;
  uint64_t *p_free_block_map;
  uint64_t morphed_image_size;

  union {
    pts_HfsPlusVH p_hfsplus_vh;
    pts_FatVH p_fat_vh;
  };
} ts_UnallocatedHandle, *pts_UnallocatedHandle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int UnallocatedCreateHandle(void **pp_handle,
                                 const char *p_format,
                                 uint8_t debug);
static int UnallocatedDestroyHandle(void **pp_handle);
static int UnallocatedMorph(
  void *p_handle,
  pts_LibXmountMorphingInputFunctions p_input_functions);
static int UnallocatedSize(void *p_handle,
                         uint64_t *p_size);
static int UnallocatedRead(void *p_handle,
                         char *p_buf,
                         off_t offset,
                         size_t count,
                         size_t *p_read);
static int UnallocatedOptionsHelp(const char **pp_help);
static int UnallocatedOptionsParse(void *p_handle,
                                 uint32_t options_count,
                                 const pts_LibXmountOptions *pp_options,
                                 const char **pp_error);
static int UnallocatedGetInfofileContent(void *p_handle,
                                       const char **pp_info_buf);
static const char* UnallocatedGetErrorMessage(int err_num);
static void UnallocatedFreeBuffer(void *p_buf);

// Helper functions
static int DetectFs(pts_UnallocatedHandle p_unallocated_handle);

#endif // LIBXMOUNT_MORPHING_UNALLOCATED_H

