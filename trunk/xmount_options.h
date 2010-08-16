/*******************************************************************************
* xmount Copyright (c) 2008-2010 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* xmount is a small tool to "fuse mount" various image formats as dd or vdi    *
* files and enable virtual write access.                                       *
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

#ifndef XMOUNT_OPTIONS_H
#define XMOUNT_OPTIONS_H

#include <inttypes.h>

/*!
  @brief Input image types
  Specifies the input image type.
*/
typedef enum eInputImageType {
  //! Input image is a DD file
  eInputImageType_DD,
  //! Input image is an EWF file
  eInputImageType_EWF,
  //! Input image is an AFF file
  eInputImageType_AFF
} tInputImageType;

/*!
  @brief Output (virtual) image types
  Specifies the output image type.
*/
typedef enum eOutputImageType {
  //! Virtual image is a DD file
  eOutputImageType_DD,
  //! Virtual image is a VDI file
  eOutputImageType_VDI,
  //! Virtual image is a VMDK file (IDE bus)
  eOutputImageType_VMDK,
  //! Virtual image is a VMDK file (SCSI bus)
  eOutputImageType_VMDKS
} tOutputImageType;

/*!
  @brief xmount runtime options
  Structure to save various xmount runtime options. They are accessed trough
  the global variable xmount_options defined in this header too.
*/
typedef struct sXmountOptions {
  /* Input image related */
  //! Input image type
  tInputImageType input_image_type;
  //! Size of input image
  uint64_t input_image_size;
  //! Amount of data to use for following hash
  uint64_t input_image_hash_amount;
  //! MD5 hash of partial input image (16 byte + '\0')
  char orig_image_hash[17];

  /* Output image related (general) */
  //! Output image type
  tOutputImageType output_image_type;
  //! Size of virtual image
  uint64_t output_image_size;
  //! Path and name of the output image file
  char *p_output_image_path;
  //! Cache file to save changes to
  char *p_cache_file;
  
  /* Output image related (VDI specific) */


  /* Output image related (VMDK(S) specific) */
  //! Path of virtual VMDK file
  char *pVirtualVmdkPath;
  //! Path of virtual image info file
  char *pVirtualImageInfoPath;

  /* "Real" options */
  //! Enable debug output
  uint8_t debug;
  //! Enable virtual write support
  uint8_t writable;
  //! Overwrite existing cache
  uint8_t overwrite_cache;
} tXmountOptions;

/* Global xmount options var */
tXmountOptions xmount_options;

/* Functions to deal with the above defined global xmount option variable */
/*!
  @brief Init xmount options structure
  Simply set everything to zero.
*/
inline void InitXmountOptions();

/*!
  @brief Parse command line options
  Parse any options given on command line.
  @param[in] argc Number of command line options
  @param[in] pp_argv Array holding all command line options
  @param[out] p_fuse_argc Number of command line options to pass to fuse
  @param[out] pp_fuse_argv Array holding all command line options to pass to fuse
  @return 1 on success, 0 on error
*/
int ParseCommandLine(int argc,
                     char **pp_argv,
                     int *p_fuse_argc,
                     char **pp_fuse_argv);

#endif // #ifndef XMOUNT_OPTIONS_H
