/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing EWF images created by Guymager and others.                         *
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

#ifndef AEWF_H
#define AEWF_H

typedef struct _t_Aewf *t_pAewf;

// ----------------------
//  Constant definitions
// ----------------------

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))

#define FALSE 0
#define TRUE  1


#define ASPRINTF(...)               \
{                                   \
   if (asprintf(__VA_ARGS__) < 0)   \
      return AEWF_ASPRINTF_FAILED;  \
}

// ---------------------
//  Types and strutures
// ---------------------

typedef struct
{
   unsigned char      Signature[8];
   unsigned char      StartOfFields; // 0x01;
   unsigned short int SegmentNumber;
   unsigned short int EndOfFields;   // 0x0000
} __attribute__ ((packed)) t_AewfFileHeader, *t_AewfpFileHeader;

typedef struct
{
   unsigned char      Type[16];
   uint64_t           OffsetNextSection;
   uint64_t           Size;
   unsigned char      Padding[40];
   unsigned int       Checksum;
   char               Data[];  //lint !e1501 data member has zero size
} __attribute__ ((packed)) t_AewfSection, *t_pAewfSection;

typedef struct
{
   unsigned char      MediaType;
   unsigned char      Unknown1[3];  // contains 0x00
   unsigned int       ChunkCount;
   unsigned int       SectorsPerChunk;
   unsigned int       BytesPerSector;
   uint64_t           SectorCount;
   unsigned int       CHS_Cylinders;
   unsigned int       CHS_Heads;
   unsigned int       CHS_Sectors;
   unsigned char      MediaFlags;
   unsigned char      Unknown2[3];  // contains 0x00
   unsigned int       PalmVolumeStartSector;
   unsigned char      Padding1[4];  // contains 0x00
   unsigned int       SmartLogsStartSector;
   unsigned char      CompressionLevel;
   unsigned char      Unknown3[3];  // contains 0x00
   unsigned int       ErrorBlockSize;
   unsigned char      Unknown4[4];
   unsigned char      AcquirySystemGUID[16];
   unsigned char      Padding2[963];
   unsigned char      Reserved [5];
   unsigned int       Checksum;
} __attribute__ ((packed)) t_AewfSectionVolume, *t_pAewfSectionVolume;

typedef struct
{
   unsigned int       ChunkCount;
   unsigned char      Padding1 [4];
   uint64_t           TableBaseOffset;
   unsigned char      Padding2 [4];
   unsigned int       Checksum;
   unsigned int       OffsetArray[];  //lint !e1501 data member has zero size
} __attribute__ ((packed)) t_AewfSectionTable, *t_pAewfSectionTable;

const unsigned int  AEWF_COMPRESSED = 0x80000000;

typedef struct
{
   unsigned int FirstSector;
   unsigned int NumberOfSectors;
} __attribute__ ((packed)) t_AewfSectionErrorEntry, *t_pAewfSectionErrorEntry;

typedef struct
{
   unsigned int            NumberOfErrors;
   unsigned char           Padding[512];
   unsigned int            Checksum;
   t_AewfSectionErrorEntry ErrorArr[0];  //lint !e1501 data member has zero size
   unsigned int            ChecksumArr;
} __attribute__ ((packed)) t_AewfSectionError, *t_pAewfSectionError;

typedef struct
{
   unsigned char MD5[16];
   unsigned char Unknown[16];
   unsigned int  Checksum;
} __attribute__ ((packed)) t_AewfSectionHash, *t_pAewfSectionHash;


typedef struct
{
   char     *pName;
   unsigned   Number;
   FILE     *pFile;         // NULL if file is not opened (never read or kicked out form cache)
   time_t     LastUsed;
} t_Segment, *t_pSegment;

typedef struct
{
   uint64_t   Nr;                 // The table's position in the pAewf->pTableArr, for debug output only
   uint64_t   ChunkFrom;          // Number of the chunk referred to by the first entry of this table (very first chunk has number 0)
   uint64_t   ChunkTo;            // Number of the chunk referred to by the last entry of this table
   t_pSegment          pSegment;            // The file segment where the table is located
   uint64_t   Offset;             // The offset of the table inside the segment file (start of t_AewfSectionTable, not of the preceding t_AewfSection)
   unsigned long        Size;               // The length of the table (same as allocated length for pEwfTable)
   unsigned int         ChunkCount;         // The number of chunk; this is the same as pTableData->Chunkcount, however, pTableData might not be available (NULL)
   unsigned int         SectionSectorsSize; // Silly EWF format has no clean way of knowing size of the last (possibly compressed) chunk of a table
   time_t               LastUsed;           // Last usage of this table, for cache management
   t_pAewfSectionTable pEwfTable;           // Contains the original EWF table section or NULL, if never read or kicked out from cache
} t_Table, *t_pTable;

typedef struct _t_Aewf
{
   t_pSegment          pSegmentArr;      // Array of all segment files (in correct order)
   t_pTable            pTableArr;        // Array of all chunk offset tables found in the segment files (in correct order)
   unsigned             Segments;
   unsigned             Tables;
   uint64_t   Chunks;          // Total number of chunks in all tables
   uint64_t   TableCache;      // Current amount RAM used by tables, in bytes
   uint64_t   OpenSegments;    // Current number of open segment files
   uint64_t   SectorSize;
   uint64_t   Sectors;
   uint64_t   ChunkSize;
   uint64_t   ImageSize;       // Equals to Sectors * SectorSize
   char      *pChunkBuffCompressed;
   char      *pChunkBuffUncompressed;
   uint64_t   ChunkBuffUncompressedDataLen;  // This normally always is equal to the chunk size (32K), except maybe for the last chunk, if the image's total size is not a multiple of the chunk size
   unsigned int         ChunkBuffSize;
   uint64_t   ChunkInBuff;     // Chunk currently residing in pChunkBuffUncompressed
   char               *pErrorText;       // Used for assembling error text during option parsing
   time_t               LastStatsUpdate;
   char               *pInfo;

   // Statistics
   uint64_t   SegmentCacheHits;
   uint64_t   SegmentCacheMisses;
   uint64_t   TableCacheHits;
   uint64_t   TableCacheMisses;
   uint64_t   ChunkCacheHits;
   uint64_t   ChunkCacheMisses;
   uint64_t   ReadOperations;        // How many times did xmount call the function AewfRead
   uint64_t   DataReadFromImage;     // The data (in bytes) read from the image
   uint64_t   DataReadFromImageRaw;  // The same data (in bytes), after uncompression (if any)
   uint64_t   DataRequestedByCaller; // How much data was given back to the caller
   uint64_t   TablesReadFromImage;   // The overhead of the table read operations (in bytes)

   uint64_t   ChunksRead;
   uint64_t   BytesRead;

   // Options
   uint64_t   MaxTableCache;    // Max. amount of bytes in pTableArr[x].pTableData, in bytes
   uint64_t   MaxOpenSegments;  // Max. number of open files in pSegmentArr
   char               *pStatsFilename;    // Statistics file
   uint64_t   StatsRefresh;     // The time in seconds between update of the stats file
} t_Aewf;

// ----------------
//  Error handling
// ----------------

#ifdef AEWF_DEBUG
   #define CHK(ChkVal)                                                \
   {                                                                  \
      int ChkValRc;                                                   \
      if ((ChkValRc=(ChkVal)) != AEWF_OK)                             \
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
      if ((ChkValRc=(ChkVal)) != AEWF_OK)   \
         return ChkValRc;                   \
   }
   #define DEBUG_PRINTF(...)
#endif


// Possible error codes for the above functions
enum
{
   AEWF_OK = 0,
   AEWF_FOUND,

   AEWF_MEMALLOC_FAILED=100,           // 100
   AEWF_FILE_OPEN_FAILED,
   AEWF_FILE_CLOSE_FAILED,
   AEWF_FILE_SEEK_FAILED,
   AEWF_FILE_READ_FAILED,
   AEWF_READFILE_BAD_MEM,              // 105
   AEWF_INVALID_SEGMENT_NUMBER,
   AEWF_WRONG_SEGMENT_FILE_COUNT,
   AEWF_VOLUME_MUST_PRECEDE_TABLES,
   AEWF_SECTORS_MUST_PRECEDE_TABLES,
   AEWF_WRONG_CHUNK_COUNT,             // 110
   AEWF_READ_BEYOND_IMAGE_LENGTH,
   AEWF_CHUNK_NOT_FOUND,
   AEWF_VOLUME_MISSING,
   AEWF_ERROR_EWF_TABLE_NOT_READY,
   AEWF_ERROR_EWF_SEGMENT_NOT_READY,   // 115
   AEWF_CHUNK_TOO_BIG,
   AEWF_UNCOMPRESS_FAILED,
   AEWF_BAD_UNCOMPRESSED_LENGTH,
   AEWF_CHUNK_CRC_ERROR,
   AEWF_ERROR_IN_CHUNK_NUMBER,         // 120
   AEWF_VASPRINTF_FAILED,
   AEWF_UNCOMPRESS_HEADER_FAILED,
   AEWF_ASPRINTF_FAILED,
};


#endif
