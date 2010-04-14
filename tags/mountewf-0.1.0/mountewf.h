/*******************************************************************************
 * mountewf (c) 2008 by Gillen Daniel <Daniel.GILLEN@police.etat.lu>           *
 *                                                                             *
 * mountewf is a small tool to "fuse mount" ewf images as dd or vdi files      *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation, either version 3 of the License, or           *
 * (at your option) any later version.                                         *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.        *
 ******************************************************************************/

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <sys/types.h>
#include <inttypes.h>

/*
 * Mount methods
 */
typedef enum MOUNTMETHODS {
  MOUNT_AS_DD,
  MOUNT_AS_VDI
} MOUNTMETHODS;

//    /** The way the UUID is declared by the DCE specification. */
//    struct
//    {
//        uint32_t    u32TimeLow;
//        uint16_t    u16TimeMid;
//        uint16_t    u16TimeHiAndVersion;
//        uint8_t     u8ClockSeqHiAndReserved;
//        uint8_t     u8ClockSeqLow;
//        uint8_t     au8Node[6];
//    } Gen;

/*
 * VDI Binary File Header structure
 */
typedef struct VDIFILEHEADER {
// ----- VDIPREHEADER ------
  /** Just text info about image type, for eyes only. */
  char szFileInfo[64];
  /** The image signature (VDI_IMAGE_SIGNATURE). */
  uint32_t u32Signature;
  /** The image version (VDI_IMAGE_VERSION). */
  uint32_t u32Version;
// ----- VDIHEADER1PLUS -----
  /** Size of header structure in bytes. */
  uint32_t cbHeader;
  /** The image type (VDI_IMAGE_TYPE_*). */
  uint32_t u32Type;
  /** Image flags (VDI_IMAGE_FLAGS_*). */
  uint32_t fFlags;
  /** Image comment. (UTF-8) */
  char szComment[256];
  /** Offset of Blocks array from the begining of image file.
   * Should be sector-aligned for HDD access optimization. */
  uint32_t offBlocks;
  /** Offset of image data from the begining of image file.
   * Should be sector-aligned for HDD access optimization. */
  uint32_t offData;
  /** Legacy image geometry (previous code stored PCHS there). */
  /** Cylinders. */
  uint32_t cCylinders;
  /** Heads. */
  uint32_t cHeads;
  /** Sectors per track. */
  uint32_t cSectors;
  /** Sector size. (bytes per sector) */
  uint32_t cbSector;
  /** Was BIOS HDD translation mode, now unused. */
  uint32_t u32Dummy;
  /** Size of disk (in bytes). */
  uint64_t cbDisk;
  /** Block size. (For instance VDI_IMAGE_BLOCK_SIZE.) Should be a power of 2! */
  uint32_t cbBlock;
  /** Size of additional service information of every data block.
   * Prepended before block data. May be 0.
   * Should be a power of 2 and sector-aligned for optimization reasons. */
  uint32_t cbBlockExtra;
  /** Number of blocks. */
  uint32_t cBlocks;
  /** Number of allocated blocks. */
  uint32_t cBlocksAllocated;
  /** UUID of image. */
  uint64_t uuidCreate_l;
  uint64_t uuidCreate_h;
  /** UUID of image's last modification. */
  uint64_t uuidModify_l;
  uint64_t uuidModify_h;
  /** Only for secondary images - UUID of previous image. */
  uint64_t uuidLinkage_l;
  uint64_t uuidLinkage_h;
  /** Only for secondary images - UUID of previous image's last modification. */
  uint64_t uuidParentModify_l;
  uint64_t uuidParentModify_h;
  /** Padding to get 512 alignment*/
  uint64_t padding0;
  uint64_t padding1;
  uint64_t padding2;
  uint64_t padding3;
  uint64_t padding4;
  uint64_t padding5;
  uint64_t padding6;
} VDIFILEHEADER;

