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

#ifndef LIBXMOUNT_MORPHING_FREESPACE_H
#define LIBXMOUNT_MORPHING_FREESPACE_H

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
  if(p_freespace_handle->debug==1)                     \
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
  FREESPACE_OK=0,
  FREESPACE_MEMALLOC_FAILED,
  FREESPACE_NO_FS_SPECIFIED,
  FREESPACE_UNSUPPORTED_FS_SPECIFIED,
  FREESPACE_CANNOT_GET_IMAGECOUNT,
  FREESPACE_WRONG_INPUT_IMAGE_COUNT,
  FREESPACE_CANNOT_GET_IMAGESIZE,
  FREESPACE_READ_BEYOND_END_OF_IMAGE,
  FREESPACE_CANNOT_READ_DATA,
  FREESPACE_CANNOT_PARSE_OPTION,
  FREESPACE_CANNOT_READ_HFSPLUS_HEADER,
  FREESPACE_INVALID_HFSPLUS_HEADER,
  FREESPACE_CANNOT_READ_HFSPLUS_ALLOC_FILE,
  FREESPACE_ALLOC_FILE_HAS_TOO_MUCH_EXTENDS
};

// Supported fs types
typedef enum e_FreespaceFsType {
  FreespaceFsType_Unknown=0,
  FreespaceFsType_HfsPlus
} te_FreespaceFsType;

// HFS+ extend
typedef struct s_FreespaceHfsPlusExtend {
  uint32_t start_block;
  uint32_t block_count;
} __attribute__ ((packed)) ts_FreespaceHfsPlusExtend, *pts_FreespaceHfsPlusExtend;

// Needed parts of the HFS+ volume header
#define FREESPACE_HFSPLUS_VH_OFFSET 1024
#define FREESPACE_HFSPLUS_VH_SIGNATURE 0x482b //"H+"
#define FREESPACE_HFSPLUS_VH_VERSION 4
typedef struct s_FreespaceHfsPlusVH {
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
  ts_FreespaceHfsPlusExtend alloc_file_extends[8];
} __attribute__ ((packed)) ts_FreespaceHfsPlusVH, *pts_FreespaceHfsPlusVH;

// Needed data for HFS+
typedef struct s_FreespaceHfsPlusData {
  pts_FreespaceHfsPlusVH p_vh;
  uint8_t *p_alloc_file;
  uint64_t free_block_map_size;
  uint64_t *p_free_block_map;
} ts_FreespaceHfsPlusData, *pts_FreespaceHfsPlusData;

// Handle
typedef struct s_FreespaceHandle {
  uint8_t debug;
  te_FreespaceFsType fs_type;
  pts_LibXmountMorphingInputFunctions p_input_functions;
  uint64_t morphed_image_size;
  ts_FreespaceHfsPlusData hfsplus;
} ts_FreespaceHandle, *pts_FreespaceHandle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int FreespaceCreateHandle(void **pp_handle,
                                 char *p_format,
                                 uint8_t debug);
static int FreespaceDestroyHandle(void **pp_handle);
static int FreespaceMorph(void *p_handle,
                          pts_LibXmountMorphingInputFunctions p_input_functions);
static int FreespaceSize(void *p_handle,
                         uint64_t *p_size);
static int FreespaceRead(void *p_handle,
                         char *p_buf,
                         off_t offset,
                         size_t count,
                         size_t *p_read);
static const char* FreespaceOptionsHelp();
static int FreespaceOptionsParse(void *p_handle,
                                 uint32_t options_count,
                                 pts_LibXmountOptions *pp_options,
                                 char **pp_error);
static int FreespaceGetInfofileContent(void *p_handle,
                                       char **pp_info_buf);
static const char* FreespaceGetErrorMessage(int err_num);
static void FreespaceFreeBuffer(void *p_buf);

// Helper functions
static int FreespaceReadHfsPlusHeader(pts_FreespaceHandle p_freespace_handle);
static int FreespaceReadHfsPlusAllocFile(pts_FreespaceHandle p_freespace_handle);
static int FreespaceBuildHfsPlusBlockMap(pts_FreespaceHandle p_freespace_handle);
static int FreespaceReadHfsPlusBlock(pts_FreespaceHandle p_freespace_handle,
                                     char *p_buf,
                                     off_t offset,
                                     size_t count,
                                     size_t *p_read);

#endif // LIBXMOUNT_MORPHING_FREESPACE_H
