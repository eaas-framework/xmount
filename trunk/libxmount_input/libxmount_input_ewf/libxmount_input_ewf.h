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

#ifndef LIBXMOUNT_INPUT_EWF_H
#define LIBXMOUNT_INPUT_EWF_H

/*******************************************************************************
 * Enums, Typedefs, etc...
 ******************************************************************************/
//! Possible error return codes
enum {
  EWF_OK=0,
  EWF_MEMALLOC_FAILED,
  EWF_HANDLE_CREATION_FAILED,
  EWF_HANDLE_DESTRUCTION_FAILED,
  EWF_NO_INPUT_FILES,
  EWF_INVALID_INPUT_FILES,
  EWF_OPEN_FAILED,
  EWF_OPEN_FAILED_SEEK,
  EWF_OPEN_FAILED_READ,
  EWF_HEADER_PARSING_FAILED,
  EWF_CLOSE_FAILED,
  EWF_GET_SIZE_FAILED,
  EWF_SEEK_FAILED,
  EWF_READ_FAILED
};

//! Library handle
typedef struct s_EwfHandle {
#ifdef HAVE_LIBEWF_V2_API
  //! EWF handle
  libewf_handle_t *h_ewf;
#else
  //! EWF handle
  LIBEWF_HANDLE *h_ewf;
#endif
  //! Debug settings
  uint8_t debug;
} ts_EwfHandle, *pts_EwfHandle;

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int EwfCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug);
static int EwfDestroyHandle(void **pp_handle);
static int EwfOpen(void *p_handle,
                   const char **pp_filename_arr,
                   uint64_t filename_arr_len);
static int EwfClose(void *p_handle);
static int EwfSize(void *p_handle,
                   uint64_t *p_size);
static int EwfRead(void *p_handle,
                   char *p_buf,
                   off_t seek,
                   size_t count,
                   size_t *p_read,
                   int *p_errno);
static int EwfOptionsHelp(const char **pp_help);
static int EwfOptionsParse(void *p_handle,
                           uint32_t options_count,
                           const pts_LibXmountOptions *pp_options,
                           const char **pp_error);
static int EwfGetInfofileContent(void *p_handle,
                                 const char **pp_info_buf);
static const char* EwfGetErrorMessage(int err_num);
static int EwfFreeBuffer(void *p_buf);

#endif // LIBXMOUNT_INPUT_EWF_H

