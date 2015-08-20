/*******************************************************************************
* xmount Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#include "../libxmount_input/libxmount_input.h"
#include "../libxmount_morphing/libxmount_morphing.h"

#undef FALSE
#undef TRUE
#define FALSE 0
#define TRUE 1

/*
 * Constants
 */
#define IMAGE_INFO_INPUT_HEADER \
  "------> The following values are supplied by the used input library(ies) " \
    "<------\n"
#define IMAGE_INFO_MORPHING_HEADER \
  "\n------> The following values are supplied by the used morphing library " \
    "<------\n\n"

/*******************************************************************************
 * Structures of output images
 ******************************************************************************/

#define VDI_FILE_COMMENT "<<< This is a virtual VDI image >>>"
#define VDI_HEADER_COMMENT "This VDI was emulated using xmount v" XMOUNT_VERSION
#define VDI_IMAGE_SIGNATURE 0xBEDA107F // 1:1 copy from hp
#define VDI_IMAGE_VERSION 0x00010001 // Vers 1.1
#define VDI_IMAGE_TYPE_FIXED 0x00000002 // Type 2 (fixed size)
#define VDI_IMAGE_FLAGS 0
#define VDI_IMAGE_BLOCK_SIZE (1024*1024) // 1 Megabyte
//! VDI Binary File Header structure
typedef struct s_VdiFileHeader {
// ----- VDIPREHEADER ------
  //! Just text info about image type, for eyes only
  char szFileInfo[64];
  //! The image signature (VDI_IMAGE_SIGNATURE)
  uint32_t u32Signature;
  //! The image version (VDI_IMAGE_VERSION)
  uint32_t u32Version;
  // ----- VDIHEADER1PLUS -----
  //! Size of header structure in bytes.
  uint32_t cbHeader;
  //! The image type (VDI_IMAGE_TYPE_*)
  uint32_t u32Type;
  //! Image flags (VDI_IMAGE_FLAGS_*)
  uint32_t fFlags;
  //! Image comment (UTF-8)
  char szComment[256];
  //! Offset of Blocks array from the begining of image file.
  //  Should be sector-aligned for HDD access optimization.
  uint32_t offBlocks;
  //! Offset of image data from the begining of image file.
  //  Should be sector-aligned for HDD access optimization.
  uint32_t offData;
  //! Legacy image geometry - Cylinders
  uint32_t cCylinders;
  //! Legacy image geometry - Heads
  uint32_t cHeads;
  //! Legacy image geometry - Sectors per track
  uint32_t cSectors;
  //! Legacy image geometry - Sector size (bytes per sector)
  uint32_t cbSector;
  //! Was BIOS HDD translation mode, now unused
  uint32_t u32Dummy;
  //! Size of disk (in bytes)
  uint64_t cbDisk;
  //! Block size (for instance VDI_IMAGE_BLOCK_SIZE). Must be a power of 2!
  uint32_t cbBlock;
  //! Size of additional service information of every data block.
  //  Prepended before block data. May be 0.
  //  Should be a power of 2 and sector-aligned for optimization reasons.
  uint32_t cbBlockExtra;
  //! Number of blocks
  uint32_t cBlocks;
  //! Number of allocated blocks
  uint32_t cBlocksAllocated;
  //! UUID of image (lower 64 bit)
  uint64_t uuidCreate_l;
  //! UUID of image (higher 64 bit)
  uint64_t uuidCreate_h;
  //! UUID of image's last modification (lower 64 bit)
  uint64_t uuidModify_l;
  //! UUID of image's last modification (higher 64 bit)
  uint64_t uuidModify_h;
  //! Only for secondary images - UUID of previous image (lower 64 bit)
  uint64_t uuidLinkage_l;
  //! Only for secondary images - UUID of previous image (higher 64 bit)
  uint64_t uuidLinkage_h;
  //! Only for secondary images - UUID of prev image's last mod (lower 64 bit)
  uint64_t uuidParentModify_l;
  //! Only for secondary images - UUID of prev image's last mod (higher 64 bit)
  uint64_t uuidParentModify_h;
  //! Padding to get 512 byte alignment
  char padding[56];
} __attribute__ ((packed)) ts_VdiFileHeader, *pts_VdiFileHeader;

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
 * VHD Binary File footer structure
 *
 * At the time of writing, the specs could be found here:
 *   http://www.microsoft.com/downloads/details.aspx?
 *     FamilyID=C2D03242-2FFB-48EF-A211-F0C44741109E
 *
 * Warning: All values are big-endian!
 */
// 
#ifdef __LP64__
  #define VHD_IMAGE_HVAL_COOKIE 0x78697463656E6F63 // "conectix"
#else
  #define VHD_IMAGE_HVAL_COOKIE 0x78697463656E6F63LL 
#endif
#define VHD_IMAGE_HVAL_FEATURES 0x02000000
#define VHD_IMAGE_HVAL_FILE_FORMAT_VERSION 0x00000100
#ifdef __LP64__
  #define VHD_IMAGE_HVAL_DATA_OFFSET 0xFFFFFFFFFFFFFFFF
#else
  #define VHD_IMAGE_HVAL_DATA_OFFSET 0xFFFFFFFFFFFFFFFFLL
#endif
#define VHD_IMAGE_HVAL_CREATOR_APPLICATION 0x746E6D78 // "xmnt"
#define VHD_IMAGE_HVAL_CREATOR_VERSION 0x00000500
// This one is funny! According to VHD specs, I can only choose between Windows
// and Macintosh. I'm going to choose the most common one.
#define VHD_IMAGE_HVAL_CREATOR_HOST_OS 0x6B326957 // "Win2k"
#define VHD_IMAGE_HVAL_DISK_TYPE 0x02000000
// Seconds from January 1st, 1970 to January 1st, 2000
#define VHD_IMAGE_TIME_CONVERSION_OFFSET 0x386D97E0
typedef struct s_VhdFileHeader {
  uint64_t cookie;
  uint32_t features;
  uint32_t file_format_version;
  uint64_t data_offset;
  uint32_t creation_time;
  uint32_t creator_app;
  uint32_t creator_ver;
  uint32_t creator_os;
  uint64_t size_original;
  uint64_t size_current;
  uint16_t disk_geometry_c;
  uint8_t disk_geometry_h;
  uint8_t disk_geometry_s;
  uint32_t disk_type;
  uint32_t checksum;
  uint64_t uuid_l;
  uint64_t uuid_h;
  uint8_t saved_state;
  char reserved[427];
} __attribute__ ((packed)) ts_VhdFileHeader, *pts_VhdFileHeader;

/*******************************************************************************
 * Xmount specific structures
 ******************************************************************************/

#ifdef __LP64__
  #define CACHE_BLOCK_FREE 0xFFFFFFFFFFFFFFFF
#else
  #define CACHE_BLOCK_FREE 0xFFFFFFFFFFFFFFFFLL 
#endif
//! Cache file block index array element
typedef struct s_CacheFileBlockIndex {
  //! Set to 1 if block is assigned (this block has data in cache file)
  uint32_t Assigned;
  //! Offset to data in cache file
  uint64_t off_data;
} __attribute__ ((packed)) ts_CacheFileBlockIndex, *pts_CacheFileBlockIndex;

#define CACHE_BLOCK_SIZE (1024*1024) // 1 megabyte
#ifdef __LP64__
  #define CACHE_FILE_SIGNATURE 0xFFFF746E756F6D78 // "xmount\xFF\xFF"
#else
  #define CACHE_FILE_SIGNATURE 0xFFFF746E756F6D78LL 
#endif
#define CUR_CACHE_FILE_VERSION 0x00000002 // Current cache file version
#define HASH_AMOUNT (1024*1024)*10 // Amount of data used to construct a
                                   // "unique" hash for every input image
                                   // (10MByte)
//! Cache file header structure
typedef struct s_CacheFileHeader {
  //! Simple signature to identify cache files
  uint64_t FileSignature;
  //! Cache file version
  uint32_t CacheFileVersion;
  //! Cache block size
  uint64_t BlockSize;
  //! Total amount of cache blocks
  uint64_t BlockCount;
  //! Offset to the first block index array element
  uint64_t pBlockIndex;
  //! Set to 1 if VDI file header is cached
  uint32_t VdiFileHeaderCached;
  //! Offset to cached VDI file header
  uint64_t pVdiFileHeader;
  //! Set to 1 if VMDK file is cached
  uint32_t VmdkFileCached;
  //! Size of VMDK file
  uint64_t VmdkFileSize;
  //! Offset to cached VMDK file
  uint64_t pVmdkFile;
  //! Set to 1 if VHD header is cached
  uint32_t VhdFileHeaderCached;
  //! Offset to cached VHD header
  uint64_t pVhdFileHeader;
  //! Padding to get 512 byte alignment and ease further additions
  char HeaderPadding[432];
} __attribute__ ((packed)) ts_CacheFileHeader, *pts_CacheFileHeader;

//! Cache file header structure - Old v1 header
typedef struct s_CacheFileHeader_v1 {
  //! Simple signature to identify cache files
  uint64_t FileSignature;
  //! Cache file version
  uint32_t CacheFileVersion;
  //! Total amount of cache blocks
  uint64_t BlockCount;
  //! Offset to the first block index array element
  uint64_t pBlockIndex;
  //! Set to 1 if VDI file header is cached
  uint32_t VdiFileHeaderCached;
  //! Offset to cached VDI file header
  uint64_t pVdiFileHeader;
  //! Set to 1 if VMDK file is cached
} ts_CacheFileHeader_v1, *pts_CacheFileHeader_v1;

//! Virtual image types
typedef enum e_VirtImageType {
  //! Virtual image is a DD file
  VirtImageType_DD,
  //! Virtual image is a DMG file
  VirtImageType_DMG,
  //! Virtual image is a VDI file
  VirtImageType_VDI,
  //! Virtual image is a VMDK file (IDE bus)
  VirtImageType_VMDK,
  //! Virtual image is a VMDK file (SCSI bus)
  VirtImageType_VMDKS,
  //! Virtual image is a VHD file
  VirtImageType_VHD
} te_VirtImageType;

//! Structure containing infos about input libs
typedef struct s_InputLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported input types
  char *p_supported_input_types;
  //! Struct containing lib functions
  ts_LibXmountInputFunctions lib_functions;
} ts_InputLib, *pts_InputLib;

//! Structure containing infos about input images
typedef struct s_InputImage {
  //! Image type
  char *p_type;
  //! Image source file count
  uint64_t files_count;
  //! Image source files
  char **pp_files;
  //! Input lib functions for this image
  pts_LibXmountInputFunctions p_functions;
  //! Image handle
  void *p_handle;
  //! Image size
  uint64_t size;
} ts_InputImage, *pts_InputImage;

typedef struct s_InputData {
  //! Loaded input lib count
  uint32_t libs_count;
  //! Array containing infos about loaded input libs
  pts_InputLib *pp_libs;
  //! Amount of input lib params (--inopts)
  uint32_t lib_params_count;
  //! Input lib params (--inopts)
  pts_LibXmountOptions *pp_lib_params;
  //! Input image count
  uint64_t images_count;
  //! Input images
  pts_InputImage *pp_images;
  //! Input image offset (--offset)
  uint64_t image_offset;
  //! Input image size limit (--sizelimit)
  uint64_t image_size_limit;
  //! MD5 hash of partial input image (lower 64 bit) (after morph)
  uint64_t image_hash_lo;
  //! MD5 hash of partial input image (higher 64 bit) (after morph)
  uint64_t image_hash_hi;
} ts_InputData;

//! Structure containing infos about morphing libs
typedef struct s_MorphingLib {
  //! Filename of lib (without path)
  char *p_name;
  //! Handle to the loaded lib
  void *p_lib;
  //! Array of supported morphing types
  char *p_supported_morphing_types;
  //! Struct containing lib functions
  ts_LibXmountMorphingFunctions lib_functions;
} ts_MorphingLib, *pts_MorphingLib;

//! Structures and vars needed for morph support
typedef struct s_MorphingData {
  //! Loaded morphing lib count
  uint32_t libs_count;
  //! Array containing infos about loaded morphing libs
  pts_MorphingLib *pp_libs;
  //! Specified morphing type (--morph)
  char *p_morph_type;
  //! Amount of specified morphing lib params (--morphopts)
  uint32_t lib_params_count;
  //! Specified morphing lib params (--morphopts)
  pts_LibXmountOptions *pp_lib_params;
  //! Handle to initialized morphing lib
  void *p_handle;
  //! Morphing functions of initialized lib
  pts_LibXmountMorphingFunctions p_functions;
  //! Input image functions passed to morphing lib
  ts_LibXmountMorphingInputFunctions input_image_functions;
} ts_MorphingData;

//! Structures and vars needed for write access
typedef struct s_CacheData {
  //! Cache file to save changes to
  char *p_cache_file;
  //! Handle to cache file
  FILE *h_cache_file;
  //! Overwrite existing cache
  uint8_t overwrite_cache;
  //! Cache header
  pts_CacheFileHeader p_cache_header;
  //! Cache block index
  pts_CacheFileBlockIndex p_cache_blkidx;
} ts_CacheData;

//! Structures and vars needed for VDI support
typedef struct s_OutputImageVdiData {
  //! VDI header size
  uint32_t vdi_header_size;
  //! VDI header
  pts_VdiFileHeader p_vdi_header;
  //! VDI blockmap size
  uint32_t vdi_block_map_size;
  //! VDI block map
  char *p_vdi_block_map;
} ts_OutputImageVdiData;

//! Structures and vars needed for VHD support
typedef struct s_OutputImageVhdData {
  //! VHD header
  ts_VhdFileHeader *p_vhd_header;
} ts_OutputImageVhdData;

//! Structures and vars needed for VMDK support
typedef struct s_OutputImageVmdkData {
  //! Path of virtual VMDK file
  char *p_virtual_vmdk_path;
  //! VMDK content
  char *p_vmdk_file;
  //! VMDK content size
  int vmdk_file_size;
  //! Path of 1st lockdir
  char *p_vmdk_lockdir1;
  //! Path of 2nd lockdir
  char *p_vmdk_lockdir2;
  //! Lockfile content
  char *p_vmdk_lockfile_data;
  //! Lockfile content size
  int vmdk_lockfile_size;
  //! Path of lockfile
  char *p_vmdk_lockfile_name;
} ts_OutputImageVmdkData;

//! Structure containing infos about output image
typedef struct s_OutputData {
  //! Virtual image type
  te_VirtImageType VirtImageType;
  //! Size
  uint64_t image_size;
  //! Writable? (Set to 1 if --cache was specified)
  uint8_t writable;
  //! Path of virtual image file
  char *p_virtual_image_path;
  //! Path of virtual image info file
  char *p_info_path;
  //! Pointer to virtual info file
  char *p_info_file;
  //! VDI related data
  ts_OutputImageVdiData vdi;
  //! VHD related data
  ts_OutputImageVhdData vhd;
  //! VMDK related data
  ts_OutputImageVmdkData vmdk;
} ts_OutputData;

//! Structure containing global xmount runtime infos
typedef struct s_XmountData {
  //! Input image related data
  ts_InputData input;
  //! Morphing related data
  ts_MorphingData morphing;
  //! Cache file related data
  ts_CacheData cache;
  //! Output image related data
  ts_OutputData output;
  //! Enable debug output
  uint8_t debug;
  //! Set if we are allowed to set fuse's allow_other option
  uint8_t may_set_fuse_allow_other;
  //! Argv for FUSE
  int fuse_argc;
  //! Argv for FUSE
  char **pp_fuse_argv;
  //! Mount point
  char *p_mountpoint;
  //! Mutex to control concurrent read & write access on output image
  pthread_mutex_t mutex_image_rw;
  //! Mutex to control concurrent read access on info file
  pthread_mutex_t mutex_info_read;
} ts_XmountData;

/*
  ----- Change log -----
  20090226: * Added change history information to this file.
            * Added TVirtImageType enum to identify virtual image type.
            * Added TOrigImageType enum to identify input image type.
            * Added TMountimgConfData struct to hold various mountimg runtime
              options.
            * Renamed VDIFILEHEADER to ts_VdiFileHeader.
  20090228: * Added LOG_ERROR and LOG_DEBUG macros
            * Added defines for various static VDI header values
            * Added defines for TRUE and FALSE
  20090307: * Added defines for various static cache file header values
            * Added VdiFileHeaderCached and pVdiFileHeader values to be able to
              cache the VDI file header separatly.
  20090519: * Added new cache file header structure and moved old one to
              ts_CacheFileHeader_v1.
            * New cache file structure includes VmdkFileCached and pVmdkFile to
              cache virtual VMDK file and makes room for further additions so
              current cache file version 2 cache files can be easily converted
              to newer ones.
  20090814: * Added XMOUNT_MALLOC and XMOUNT_REALLOC macros.
  20090816: * Added XMOUNT_STRSET, XMOUNT_STRNSET, XMOUNT_STRAPP and
              XMOUNT_STRNAPP macros.
  20100324: * Added "__attribute__ ((packed))" to all header structs to prevent
              different sizes on i386 and amd64.
  20111109: * Added TVirtImageType_DMG type.
  20120130: * Added LOG_WARNING macro.
  20120507: * Added ts_VhdFileHeader structure.
  20120511: * Added endianness conversation macros
  20140809: * Moved endianness macros to separate file
  20140810: * Moved convenience macros to separate file
  20140726: * Added ts_InputLib
  20140821: * Added ts_InputImage and ts_InputData
            * Moved data from various places to the above structs.
  20140825: * Added ts_MorphingLib, ts_CacheData, ts_OutputImageVdiData,
              ts_OutputImageVhdData, ts_OutputImageVmdkData and ts_OutputData.
            * Moved data from various places to the above structs.
*/

