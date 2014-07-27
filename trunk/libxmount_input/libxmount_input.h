/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* xmount is a small tool to "fuse mount" various image formats and enable      *
* virtual write access.                                                        *
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

#ifndef LIBXMOUNT_INPUT_H
#define LIBXMOUNT_INPUT_H

#define LIBXMOUNT_INPUT_API_VERSION 1

#include <stdint.h>

//! Structure containing pointers to the lib's functions
typedef struct s_LibXmountInputFunctions {
  /*!
   * Function to open input image
   *
   * \param pp_handle Pointer to store handle of opened image to
   * \param pp_filename_arr Array containing all specified input images
   * \param filename_arr_len Length of pp_filename_arr
   * \return 0 on success or error code
   */
  int (*Open)(void **pp_handle,
              const char **pp_filename_arr,
              uint64_t filename_arr_len);
  /*!
   * Function to get the input image's size
   *
   * \param p_handle Handle to the opened image
   * \param p_size Pointer to store input image's size to
   * \return 0 on success or error code
   */
  int (*Size)(void *p_handle,
              uint64_t *p_size);
  /*!
   * Function to read data from input image
   *
   * \param p_handle Handle to the opened image
   * \param offset Position at which to start reading
   * \param p_buf Buffer to store read data to
   * \param count Amount of bytes to read
   * \return 0 on success or error code
   */
  int (*Read)(void *p_handle,
              uint64_t offset,
              char *p_buf,
              uint32_t count);
  /*!
   * Function to close an opened input image
   *
   * \param pp_handle Pointer to the handle of the opened image
   * \return 0 on success or error code
   */
  int (*Close)(void **pp_handle);
  /*!
   * Function to return a string containing help messages for any supported
   * lib-specific options
   *
   * \param pp_help Pointer to a string to store null-terminated help text
   * \return 0 on success or error code
   */
  int (*OptionsHelp)(const char **pp_help);
  /*!
   * Function to parse any lib-specific options
   *
   * \param p_handle Handle to the opened image
   * \param p_options String with specified options
   * \param pp_error Pointer to a string with error message
   * \return 0 on success or error code and error message
   */
  int (*OptionsParse)(void *p_handle,
                      char *p_options,
                      char **pp_error);
  /*!
   * Function to get content to add to the info file
   *
   * \param p_handle Handle to the opened image
   * \param pp_info_buf Pointer to store the null-terminated content
   * \return 0 on success or error code
   */
  int (*GetInfofileContent)(void *p_handle,
                            const char **pp_info_buf);
  /*!
   * Function to free buffers that were allocated by lib
   *
   * \param p_buf Buffer to free
   */
  void (*FreeBuffer)(void *p_buf);
} ts_LibXmountInputFunctions, *pts_LibXmountInputFunctions;

//! Get library API version
/*!
 * \param p_ver Supported version
 */
uint8_t LibXmount_Input_GetApiVersion();
typedef uint8_t (*t_LibXmount_Input_GetApiVersion)();

//! Get a list of supported formats
/*!
 * Gets a list of supported input image formats. These are the strings
 * specified with xmount's --in <string> command line option.
 *
 * \param ppp_arr Array containing supported format strings
 * \return Length of ppp_arr
 */
const char* LibXmount_Input_GetSupportedFormats();
typedef const char* (*t_LibXmount_Input_GetSupportedFormats)();

//! Get the lib's s_LibXmountInputFunctions structure
/*!
 * \param pp_functions Functions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions);
typedef void (*t_LibXmount_Input_GetFunctions)(ts_LibXmountInputFunctions*);

#endif // LIBXMOUNT_INPUT_H

