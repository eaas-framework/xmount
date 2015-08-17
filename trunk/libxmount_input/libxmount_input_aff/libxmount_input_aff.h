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

#ifndef LIBXMOUNT_INPUT_AFF_H
#define LIBXMOUNT_INPUT_AFF_H

/*******************************************************************************
 * Enums, Typedefs, etc...
 ******************************************************************************/
//! Possible error return codes
enum {
  AFF_OK=0,
  AFF_MEMALLOC_FAILED,
  AFF_NO_INPUT_FILES,
  AFF_TOO_MANY_INPUT_FILES,
  AFF_OPEN_FAILED,
  AFF_CLOSE_FAILED,
  AFF_ENCRYPTION_UNSUPPORTED,
  AFF_SEEK_FAILED,
  AFF_READ_FAILED
};

//! Library handle
typedef struct s_AffHandle {
  //! AFF handle
  AFFILE *h_aff;
} ts_AffHandle, *pts_AffHandle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int AffCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug);
static int AffDestroyHandle(void **pp_handle);
static int AffOpen(void *p_handle,
                   const char **pp_filename_arr,
                   uint64_t filename_arr_len);
static int AffClose(void *p_handle);
static int AffSize(void *p_handle,
                   uint64_t *p_size);
static int AffRead(void *p_handle,
                   char *p_buf,
                   off_t seek,
                   size_t count,
                   size_t *p_read,
                   int *p_errno);
static int AffOptionsHelp(const char **pp_help);
static int AffOptionsParse(void *p_handle,
                           uint32_t options_count,
                           const pts_LibXmountOptions *pp_options,
                           const char **pp_error);
static int AffGetInfofileContent(void *p_handle,
                                 const char **pp_info_buf);
static const char* AffGetErrorMessage(int err_num);
static int AffFreeBuffer(void *p_buf);

#endif // LIBXMOUNT_INPUT_AFF_H

