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

#ifndef LIBXMOUNT_MORPHING_H
#define LIBXMOUNT_MORPHING_H

#define LIBXMOUNT_MORPHING_API_VERSION 1

#include <config.h>

#include <stdint.h> // For int*_t and uint*_t
#include <inttypes.h> // For PRI*

typedef struct s_LibXmountMorphingInputImage {
  void *p_image_handle;
  uint64_t size;
  int (*Read)(void *p_handle, char *p_buf, off_t offset, size_t count);
} ts_LibXmountMorphingInputImage, *pts_LibXmountMorphingInputImage;

//! Structure containing pointers to the lib's functions
typedef struct s_LibXmountMorphingFunctions {
  //! Function to initialize handle
  /*!
   * This function is called once to allow the lib to alloc any needed
   * structures before other functions that rely upon a valid handle are called
   * (for ex. OptionsParse or Morph).
   *
   * The p_format parameter specifies one of the morphing functions returned by
   * LibXmount_Morphing_GetSupportedMorphFunctions() which should be used for
   * this handle.
   *
   * \param pp_handle Pointer to store handle to
   * \param p_type Morph type to use
   * \return 0 on success or error code
   */
  int (*CreateHandle)(void **pp_handle, char *p_type);

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

  //! Function to start morphing
  /*!
   * Begins to morph input image
   *
   * \param p_handle Handle
   * \param p_input_functions ts_LibXmountInputFunctions structure
   * \return 0 on success or error code
   */
  int (*Morph)(void *p_handle,
               uint64_t input_images,
               const pts_LibXmountMorphingInputImage *pp_input_images);

  //! Function to get the size of the morphed data
  /*!
   * \param p_handle Handle to the opened image
   * \param p_size Pointer to store input image's size to
   * \return 0 on success or error code
   */
  int (*Size)(void *p_handle,
              uint64_t *p_size);

  //! Function to read data from an input image
  /*!
   * Reads count bytes at offset from input image and copies them into memory
   * starting at the address of p_buf. Memory is pre-allocated to as much bytes
   * as should be read.
   *
   * \param p_handle Handle to the opened image
   * \param p_buf Buffer to store read data to
   * \param offset Position at which to start reading
   * \param count Amount of bytes to read
   * \return Read bytes on success or negated error code on error
   */
  int (*Read)(void *p_handle,
              char *p_buf,
              off_t offset,
              size_t count);

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
} ts_LibXmountMorphingFunctions, *pts_LibXmountMorphingFunctions;

//! Get library API version
/*!
 * This function should return the value of LIBXMOUNT_MORPHING_API_VERSION
 *
 * \return Supported version
 */
uint8_t LibXmount_Morphing_GetApiVersion();
typedef uint8_t (*t_LibXmount_Morphing_GetApiVersion)();

//! Get a list of supported morphing functions
/*!
 * Gets a list of supported morphing functions. These is the string
 * specified with xmount's --morph <string> command line option. The returned
 * string must be a constant vector of morphing functions split by \0 chars. To
 * mark the end of the vector, a single \0 must be used.
 *
 * As an example, "first\0second\0\0" would be a correct string to return for
 * a lib supporting two morphing functions.
 *
 * \return Vector containing supported morphing functions
 */
const char* LibXmount_Morphing_GetSupportedTypes();
typedef const char* (*t_LibXmount_Morphing_GetSupportedTypes)();

//! Get the lib's s_LibXmountMorphingFunctions structure
/*!
 * This function should set the members of the given
 * s_LibXmountMorphingFunctions structure to the internal lib functions. All
 * members have to be set.
 *
 * \param p_functions s_LibXmountMorphingFunctions structure to fill
 */
void LibXmount_Morphing_GetFunctions(pts_LibXmountMorphingFunctions p_functions);
typedef void (*t_LibXmount_Morphing_GetFunctions)(pts_LibXmountMorphingFunctions);


#endif // LIBXMOUNT_MORPHING_H

