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

#ifndef LIBXMOUNT_INPUT_EWF_H
#define LIBXMOUNT_INPUT_EWF_H

/*******************************************************************************
 * Error codes
 ******************************************************************************/
enum {
  EWF_OK=0,
  EWF_MEMALLOC_FAILED,
  EWF_HANDLE_CREATION_FAILED,
  EWF_HANDLE_DESTRUCTION_FAILED,
  EWF_NO_INPUT_FILES,
  EWF_INVALID_INPUT_FILES,
  EWF_OPEN_FAILED,
  EWF_HEADER_PARSING_FAILED,
  EWF_CLOSE_FAILED,
  EWF_GET_SIZE_FAILED,
  EWF_SEEK_FAILED,
  EWF_READ_FAILED
};

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int EwfCreateHandle(void **pp_handle, char *p_format);
static int EwfDestroyHandle(void **pp_handle);
static int EwfOpen(void **pp_handle,
                   const char **pp_filename_arr,
                   uint64_t filename_arr_len);
static int EwfSize(void *p_handle,
                   uint64_t *p_size);
static int EwfRead(void *p_handle,
                   uint64_t seek,
                   char *p_buf,
                   uint32_t count);
static int EwfClose(void **pp_handle);
static const char* EwfOptionsHelp();
static int EwfOptionsParse(void *p_handle,
                           char *p_options,
                           char **pp_error);
static int EwfGetInfofileContent(void *p_handle,
                                 char **pp_info_buf);
static const char* EwfGetErrorMessage(int err_num);
static void EwfFreeBuffer(void *p_buf);

#endif // LIBXMOUNT_INPUT_EWF_H

