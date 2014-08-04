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

#ifndef LIBXMOUNT_INPUT_H
#define LIBXMOUNT_INPUT_H

#define LIBXMOUNT_INPUT_API_VERSION 1

#include <config.h>

#include <stdint.h> // For int*_t and uint*_t
#include <inttypes.h> // For PRI*

//! Structure containing pointers to the lib's functions
typedef struct s_LibXmountInputFunctions {
  //! Function to initialize handle
  /*!
   * This function is called once to allow the lib to alloc any needed
   * structures before other functions that rely upon a valid handle are called
   * (for ex. OptionsParse or Open).
   *
   * \param pp_handle Pointer to store handle to
   * \return 0 on success or error code
   */
  int (*CreateHandle)(void **pp_handle);

  //! Function to destroy handle
  /*!
   * In this function, any structures allocated with CreateHandle should be
   * freed. It is generally the last function called before unloading of lib
   * happens.
   *
   * By convention, after this function has been called, *pp_handle must be
   * NULL.
   *
   * \param pp_handle Pointer to store handle to
   * \return 0 on success or error code
   */
  int (*DestroyHandle)(void **pp_handle);

  //! Function to open input image
  /*!
   * Opens the specified image for reading.
   *
   * \param pp_handle Pointer to store handle of opened image to
   * \param pp_filename_arr Array containing all specified input images
   * \param filename_arr_len Length of pp_filename_arr
   * \return 0 on success or error code
   */
  int (*Open)(void **pp_handle,
              const char **pp_filename_arr,
              uint64_t filename_arr_len);

  //! Function to close an opened input image
  /*!
   * Closes the input image and frees any memory allocaed during opening but
   * does not invalidate the main handle. Further calls to for ex. Open must
   * be possible without first calling CreateHandle again!
   *
   * \param pp_handle Pointer to the handle of the opened image
   * \return 0 on success or error code
   */
  int (*Close)(void **pp_handle);

  //! Function to get the input image's size
  /*!
   * Returns the real size of the input image. Real means the size of the
   * uncompressed or otherwise made available data contained inside the input
   * image.
   *
   * \param p_handle Handle to the opened image
   * \param p_size Pointer to store input image's size to
   * \return 0 on success or error code
   */
  int (*Size)(void *p_handle,
              uint64_t *p_size);

  //! Function to read data from input image
  /*!
   * Reads count bytes at offset from input image and copies them into memory
   * starting at the address of p_buf. Memory is pre-allocated to as much bytes
   * as should be read.
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

  //! Function to get a help message for any supported lib-specific options
  /*!
   * Calling this function should return a string containing help messages for
   * any supported lib-specific options. Every line of this text must be
   * prepended with 6 spaces.
   *
   * Returned string must be constant. It won't be freed!
   *
   * If there is no help text, this function must return NULL.
   *
   * \return Pointer to a null-terminated string containing the help text
   */
  const char* (*OptionsHelp)();

  //! Function to parse any lib-specific options
  /*!
   * This function is called with the options given with the --inopts parameter.
   * All contained options are for the lib. If errors or unknown options are
   * found, this function should fail and return an error message in pp_error.
   * pp_error will be freed by the caller by using FreeBuffer.
   *
   * \param p_handle Handle to the opened image
   * \param p_options String with specified options
   * \param pp_error Pointer to a string with error message
   * \return 0 on success or error code and error message
   */
  int (*OptionsParse)(void *p_handle,
                      char *p_options,
                      char **pp_error);

  //! Function to get content to add to the info file
  /*!
   * The returned string is added to xmount's info file. This function is only
   * called once when the info file is generated. The returned string is then
   * freed with a call to FreeBuffer.
   *
   * \param p_handle Handle to the opened image
   * \param pp_info_buf Pointer to store the null-terminated content
   * \return 0 on success or error code
   */
  int (*GetInfofileContent)(void *p_handle,
                            char **pp_info_buf);

  //! Function to get an error message
  /*!
   * This function should translate an error code that was previously returned
   * by one of the library functions into a human readable error message.
   *
   * By convention, this function must always return a valid pointer to a
   * NULL-terminated string!
   *
   * \param err_num Error code as returned by lib
   */
  const char* (*GetErrorMessage)(int err_num);

  //! Function to free buffers that were allocated by lib
  /*!
   * \param p_buf Buffer to free
   */
  void (*FreeBuffer)(void *p_buf);
} ts_LibXmountInputFunctions, *pts_LibXmountInputFunctions;

//! Get library API version
/*!
 * This function should return the value of LIBXMOUNT_INPUT_API_VERSION
 *
 * \return Supported version
 */
uint8_t LibXmount_Input_GetApiVersion();
typedef uint8_t (*t_LibXmount_Input_GetApiVersion)();

//! Get a list of supported formats
/*!
 * Gets a list of supported input image formats. These are the strings
 * specified with xmount's --in <string> command line option. The returned
 * string must be a constant list of image formats split by \0 chars. To mark
 * the end of the string, a single \0 must be used.
 *
 * As an example, "first\0second\0\0" would be a correct string to return for
 * a lib supporting two input image formats.
 *
 * \return List containing supported format strings
 */
const char* LibXmount_Input_GetSupportedFormats();
typedef const char* (*t_LibXmount_Input_GetSupportedFormats)();

//! Get the lib's s_LibXmountInputFunctions structure
/*!
 * This function should set the members of the given s_LibXmountInputFunctions
 * structure to the internal lib functions. All members have to be set.
 *
 * \param p_functions s_LibXmountInputFunctions structure to fill
 */
void LibXmount_Input_GetFunctions(pts_LibXmountInputFunctions p_functions);
typedef void (*t_LibXmount_Input_GetFunctions)(pts_LibXmountInputFunctions);

#endif // LIBXMOUNT_INPUT_H

