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

#ifndef LIBXMOUNT_MORPHING_RAID_H
#define LIBXMOUNT_MORPHING_RAID_H

/*******************************************************************************
 * Enums, type defs, etc...
 ******************************************************************************/
enum {
  RAID_OK=0,
  RAID_MEMALLOC_FAILED,
  RAID_CANNOT_GET_IMAGECOUNT,
  RAID_CANNOT_GET_IMAGESIZE,
  RAID_READ_BEYOND_END_OF_IMAGE,
  RAID_CANNOT_READ_DATA,
  RAID_CANNOT_PARSE_OPTION
};

#define RAID_DEFAULT_CHUNKSIZE 512*1024
typedef struct s_RaidHandle {
  uint8_t debug;
  uint64_t input_images_count;
  uint32_t chunk_size;
  uint64_t chunks_per_image;
  pts_LibXmountMorphingInputFunctions p_input_functions;
  uint64_t morphed_image_size;
} ts_RaidHandle, *pts_RaidHandle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int RaidCreateHandle(void **pp_handle,
                            const char *p_format,
                            uint8_t debug);
static int RaidDestroyHandle(void **pp_handle);
static int RaidMorph(void *p_handle,
                     pts_LibXmountMorphingInputFunctions p_input_functions);
static int RaidSize(void *p_handle,
                    uint64_t *p_size);
static int RaidRead(void *p_handle,
                    char *p_buf,
                    off_t offset,
                    size_t count,
                    size_t *p_read);
static int RaidOptionsHelp(const char **pp_help);
static int RaidOptionsParse(void *p_handle,
                            uint32_t options_count,
                            const pts_LibXmountOptions *pp_options,
                            const char **pp_error);
static int RaidGetInfofileContent(void *p_handle,
                                  const char **pp_info_buf);
static const char* RaidGetErrorMessage(int err_num);
static void RaidFreeBuffer(void *p_buf);

#endif // LIBXMOUNT_MORPHING_RAID_H

