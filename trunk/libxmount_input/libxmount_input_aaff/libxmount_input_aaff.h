/*******************************************************************************
* xmount Copyright (c) 2008-2013 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing simple AFF images created by Guymager.                             *
*                                                                              *
* xmount is a small tool to "fuse mount" various harddisk image formats as dd, *
* vdi, vhd or vmdk files and enable virtual write access to them.              *
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

#ifndef AAFF_H
#define AAFF_H

typedef struct _t_Aaff *t_pAaff;

// ----------------------
//  Constant definitions
// ----------------------

#define AAFF_DEFAULT_PAGE_SEEK_MAX_ENTRIES 1000000  // Default max. number of cached seek points for fast page access

#define AAFF_CURRENTPAGE_NOTSET ULONG_LONG_MAX

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))

// -----------------
//  AFF definitions
// -----------------

#define AFF_GID_LENGTH   16
#define AFF_SEGARG_U64   2  // Used as argument for segments that contain a 64 bit unsigned in the data field

#define AFF_HEADER                    "AFF10\r\n"
#define AFF_SEGMENT_HEADER_MAGIC      "AFF"
#define AFF_SEGMENT_FOOTER_MAGIC      "ATT"
#define AFF_BADSECTOR_HEADER          "BAD SECTOR"
#define AFF_FILE_TYPE                 "AFF"

#define AFF_SEGNAME_BADFLAG           "badflag"
#define AFF_SEGNAME_AFFLIB_VERSION    "afflib_version"
#define AFF_SEGNAME_FILETYPE          "aff_file_type"
#define AFF_SEGNAME_GID               "image_gid"
#define AFF_SEGNAME_SECTORS           "devicesectors"
#define AFF_SEGNAME_SECTORSIZE        "sectorsize"
#define AFF_SEGNAME_IMAGESIZE         "imagesize"
#define AFF_SEGNAME_PAGESIZE          "pagesize"
#define AFF_SEGNAME_BADSECTORS        "badsectors"
#define AFF_SEGNAME_MD5               "md5"
#define AFF_SEGNAME_SHA256            "sha256"
#define AFF_SEGNAME_DURATION          "acquisition_seconds"
#define AFF_SEGNAME_PAGE              "page"

#define AAFF_SEGNAME_COMMAND_LINE "acquisition_commandline"
#define AAFF_SEGNAME_MACADDR      "acquisition_macaddr"
#define AAFF_SEGNAME_DATE         "acquisition_date"        // Format: YYYY-MM-DD HH:MM:SS TZT
#define AAFF_SEGNAME_DEVICE       "acquisition_device"
#define AAFF_SEGNAME_MODEL        "device_model"
#define AAFF_SEGNAME_SN           "device_sn"

#define AFF_PAGEFLAGS_UNCOMPRESSED    0x0000
#define AFF_PAGEFLAGS_COMPRESSED_ZLIB 0x0001
#define AFF_PAGEFLAGS_COMPRESSED_ZERO 0x0033

#define AAFF_MD5_LEN                16
#define AAFF_SHA256_LEN             32
#define AAFF_BADSECTORMARKER_MAXLEN 65536

typedef struct
{
   char         Magic[4];
   unsigned int NameLen;
   unsigned int DataLen;
   unsigned int Argument;          // Named "flags" in original aff source, named "arg" in afinfo output.
   char         Name[];            //lint !e1501
} __attribute__ ((packed)) t_AffSegmentHeader;
typedef t_AffSegmentHeader *t_pAffSegmentHeader;

// Between header and footer lie the segment name and the data

typedef struct
{
   char         Magic[4];
   unsigned int SegmentLen;
} __attribute__ ((packed)) t_AffSegmentFooter;

const int AaffInfoBuffLen = 1024*1024;

typedef struct _t_Aaff
{
   char                *pFilename;
   FILE                *pFile;

   char                 *pLibVersion;  // AFF File Header info
   char                 *pFileType;
   unsigned int           PageSize;
   unsigned int           SectorSize;
   uint64_t     Sectors;
   uint64_t     ImageSize;
   uint64_t     TotalPages;

   char                *pNameBuff;     // Buffers
   char       *pDataBuff;
   unsigned int          NameBuffLen;
   unsigned int          DataBuffLen;

   uint64_t    CurrentPage;
   char       *pPageBuff;        // Length is PageSize, contains data of CurrentPage
   unsigned int          PageBuffDataLen; // Length of current data in PageBuff (the same for all pages, but the last one might contain less data)

   char                *pInfoBuff;
   char                *pInfoBuffConst;

   uint64_t  *pPageSeekArr;
   uint64_t    PageSeekArrLen;
   uint64_t    Interleave;  // The number of pages lying between 2 entries in the PageSeekArr
} t_Aaff;


// ----------------
//  Error handling
// ----------------

#ifdef AAFF_DEBUG
   #define CHK(ChkVal)    \
   {                                                                  \
      int ChkValRc;                                                   \
      if ((ChkValRc=(ChkVal)) != AAFF_OK)                             \
      {                                                               \
         printf ("Err %d in %s, %d\n", ChkValRc, __FILE__, __LINE__); \
         return ChkValRc;                                             \
      }                                                               \
   }
   #define DEBUG_PRINTF(pFormat, ...) \
      printf (pFormat, ##__VA_ARGS__);
#else
   #define CHK(ChkVal)                      \
   {                                        \
      int ChkValRc;                         \
      if ((ChkValRc=(ChkVal)) != AAFF_OK)   \
         return ChkValRc;                   \
   }
   #define DEBUG_PRINTF(...)
#endif

// Possible error codes
enum
{
    AAFF_OK = 0,
    AAFF_FOUND,

    AAFF_MEMALLOC_FAILED=100,
    AAFF_FILE_OPEN_FAILED,
    AAFF_INVALID_SIGNATURE,
    AAFF_CANNOT_READ_DATA,
    AAFF_INVALID_HEADER,
    AAFF_INVALID_FOOTER,                // 105
    AAFF_TOO_MANY_HEADER_SEGEMENTS,
    AAFF_NOT_CREATED_BY_GUYMAGER,
    AAFF_INVALID_PAGE_NUMBER,
    AAFF_UNEXPECTED_PAGE_NUMBER,
    AAFF_CANNOT_CLOSE_FILE,             // 110
    AAFF_CANNOT_SEEK,
    AAFF_WRONG_SEGMENT,
    AAFF_UNCOMPRESS_FAILED,
    AAFF_INVALID_PAGE_ARGUMENT,
    AAFF_SEEKARR_CORRUPT,               // 115
    AAFF_PAGE_NOT_FOUND,
    AAFF_READ_BEYOND_IMAGE_LENGTH,
    AAFF_READ_BEYOND_LAST_PAGE
};


#endif
