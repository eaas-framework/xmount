/*******************************************************************************
* xmount Copyright (c) 2008,2009 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

/*
 * Virtual image types
 */
typedef enum TXVirtImageType {
  /** Virtual image is a DD file */
  TVirtImageType_DD,
  /** Virtual image is a VDI file */
  TVirtImageType_VDI,
  /** Virtual image is a VMDK file (IDE bus)*/
  TVirtImageType_VMDK,
  /** Virtual image is a VMDK file (SCSI bus)*/
  TVirtImageType_VMDKS
} TXVirtImageType;

/*
 * Input image types
 */
typedef enum TXOrigImageType {
  /** Input image is a DD file */
  TOrigImageType_DD,
  /** Input image is an EWF file */
  TOrigImageType_EWF,
  /** Input image is an AFF file */
  TOrigImageType_AFF
} TXOrigImageType;

/*
 * Various xmount runtime options
 */
typedef struct TXMOptions {
  /** Input image type */
  TXOrigImageType OrigImageType;
  /** Size of input image */
  uint64_t OrigImageSize;
  /** Amount of data to use for following hash */
  uint64_t OrigImageHashSize;
  /** MD5 hash of partial input image (16 byte) */
  char *pOrigImageHash;

  /** Virtual image type */
  TXVirtImageType VirtImageType;
  /** Size of virtual image */
  uint64_t VirtImageSize;
  
  /** Enable debug output */
  unsigned char Debug;
  /** Enable virtual write support */
  unsigned char Writable;
  /** Overwrite existing cache */
  unsigned char OverwriteCache;
  
  /** Cache file to save changes to */
  char *pCacheFile;
  
  
  
  
  
  /** Path of virtual image file */
  char *pVirtualImagePath;
  /** Path of virtual VMDK file */
  char *pVirtualVmdkPath;
  /** Path of virtual image info file */
  char *pVirtualImageInfoPath;
} TXMOptions;

TXMOptions XMOptions;

#endif // #ifndef XMOUNT_OPTIONS_H

