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

// Aewf has been written in order to reduce xmount's memory footprint when
// operating on large EWF images. Before Aewf, xmount exclusively relied on
// libewf for accessing EWF images, resulting in enormous memory consumption.
//
// Aewf uses 2 main structures for handling image access: pAewf->pSegmentArr
// contains everything about the image files (segments) and pAewf->pTableArr
// handles the EWF chunk offset tables.
//
// At the same time, those structures serve as caches for the two most vital
// ressouces, namely the number of segment files opened in parallel and the
// memory consumed by the chunk offset tables.
//
// The max. values for both are configurable, see pAewf->MaxOpenSegments and
// pAewf->MaxTableCache.

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <limits.h>
#include <time.h>     //lint !e537 !e451 Repeated include
#include <zlib.h>
#include <unistd.h>   //lint !e537 !e451 Repeated include
#include <wchar.h>    //lint !e537 !e451 Repeated include
#include <stdarg.h>   //lint !e537 !e451 Repeated include
#include <limits.h>   //lint !e537 !e451 Repeated include

#include "../libxmount_input.h"

//#define AEWF_DEBUG
#include "libxmount_input_aewf.h"

//#define AEWF_MAIN_FOR_TESTING

#ifdef AEWF_MAIN_FOR_TESTING
   #define CREATE_REVERSE_FILE
//   #define REVERSE_FILE_USES_SEPARATE_HANDLE
#endif

#ifdef AEWF_MAIN_FOR_TESTING
  #define _GNU_SOURCE
#endif

#ifdef LINT_CODE_CHECK
   #define _LARGEFILE_SOURCE
   #define _FILE_OFFSET_BITS 64
#endif

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
int AewfOpen(void **pp_handle,
             const char **pp_filename_arr,
             uint64_t filename_arr_len);
int AewfSize(void *p_handle,
             uint64_t *p_size);
int AewfRead(void *p_handle,
             uint64_t seek,
             char *p_buf,
             uint32_t count);
int AewfClose(void **pp_handle);
int AewfOptionsHelp(const char **pp_help);
int AewfOptionsParse(void *p_handle,
                     char *p_options,
                     char **pp_error);
int AewfGetInfofileContent(void *p_handle,
                           const char **pp_info_buf);
void AewfFreeBuffer(void *p_buf);

/*******************************************************************************
 * LibXmount_Input API implementation
 ******************************************************************************/
/*
 * LibXmount_Input_GetApiVersion
 */
uint8_t LibXmount_Input_GetApiVersion() {
  return LIBXMOUNT_INPUT_API_VERSION;
}

/*
 * LibXmount_Input_GetSupportedFormats
 */
const char* LibXmount_Input_GetSupportedFormats() {
  return "aewf\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
  p_functions->Open=&AewfOpen;
  p_functions->Size=&AewfSize;
  p_functions->Read=&AewfRead;
  p_functions->Close=&AewfClose;
  p_functions->OptionsHelp=&AewfOptionsHelp;
  p_functions->OptionsParse=&AewfOptionsParse;
  p_functions->GetInfofileContent=&AewfGetInfofileContent;
  p_functions->FreeBuffer=&AewfFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

// ---------------------------
//  Internal static functions
// ---------------------------

static int OpenFile (FILE **ppFile, const char *pFilename)
{
   *ppFile = fopen (pFilename, "r");
   if (*ppFile == NULL)
      return AEWF_FILE_OPEN_FAILED;
   return AEWF_OK;
}

static int CloseFile (FILE **ppFile)
{
   if (fclose (*ppFile))
      return AEWF_FILE_CLOSE_FAILED;
   *ppFile = NULL;

   return AEWF_OK;
}

static int ReadFilePos (FILE *pFile, void *pMem, unsigned int Size, uint64_t Pos)
{
   if (Size == 0)
      return AEWF_OK;

   if (Pos != ULLONG_MAX)
   {
      if (fseeko (pFile, Pos, SEEK_SET))
         return AEWF_FILE_SEEK_FAILED;
   }
   if (fread (pMem, Size, 1, pFile) != 1)
      return AEWF_FILE_READ_FAILED;

   return AEWF_OK;
}

//static int ReadFile (FILE *pFile, void *pMem, unsigned int Size)
//{
//   CHK (ReadFilePos (pFile, pMem, Size, ULLONG_MAX))
//
//   return AEWF_OK;
//}

static int ReadFileAllocPos (FILE *pFile, void **ppMem, unsigned int Size, uint64_t Pos)
{
   *ppMem = (void*) malloc (Size);
   if (*ppMem == NULL)
      return AEWF_MEMALLOC_FAILED;

   CHK (ReadFilePos (pFile, *ppMem, Size, Pos))
   return AEWF_OK;
}

static int ReadFileAlloc (FILE *pFile, void **ppMem, unsigned int Size)
{
   CHK (ReadFileAllocPos (pFile, ppMem, Size, ULLONG_MAX))

   return AEWF_OK;
}

static int QsortCompareSegments (const void *pA, const void *pB)
{
   const t_pSegment pSegmentA = ((const t_pSegment)pA); //lint !e1773 Attempt to cast way const
   const t_pSegment pSegmentB = ((const t_pSegment)pB); //lint !e1773 Attempt to cast way const
   return (int)pSegmentA->Number - (int)pSegmentB->Number;
}

// ---------------
//  API functions
// ---------------

static int CreateInfoData (t_pAewf pAewf, t_pAewfSectionVolume pVolume, char *pHeader , unsigned HeaderLen,
                                                                        char *pHeader2, unsigned Header2Len)
{
   char               *pInfo1;
   char               *pInfo2;
   char               *pInfo3 = NULL;
   char               *pInfo4;
   char               *pInfo5;
   char      *pHdr   = NULL;
   unsigned             HdrLen= 0;
   char               *pText  = NULL;
   char               *pCurrent;
   char               *pDesc  = NULL;
   char               *pData  = NULL;
   char               *pEnd;
   uLongf               DstLen0;
   int                  zrc;
   const int            MaxTextSize = 65536;
   unsigned             UncompressedLen;

   ASPRINTF(&pInfo1, "Image size               %" PRIu64 " (%0.2f GiB)\n"
                     "Bytes per sector         %u\n"
                     "Sector count             %" PRIu64 "\n"
                     "Sectors per chunk        %u\n"
                     "Chunk count              %u\n"
                     "Error block size         %u\n"
                     "Compression level        %u\n"
                     "Media type               %02X\n"
                     "Cylinders/Heads/Sectors  %u/%u/%u\n"
                     "Media flags              %02X\n"
                     "Palm volume start sector %u\n"
                     "Smart logs start sector  %u\n",
                     pAewf->ImageSize, pAewf->ImageSize / (1024.0 * 1024.0* 1024.0),
                     pVolume->BytesPerSector,
                     pVolume->SectorCount,
                     pVolume->SectorsPerChunk,
                     pVolume->ChunkCount,
                     pVolume->ErrorBlockSize,
                     pVolume->CompressionLevel,
                     pVolume->MediaType,
                     pVolume->CHS_Cylinders, pVolume->CHS_Heads, pVolume->CHS_Sectors,
                     pVolume->MediaFlags,
                     pVolume->PalmVolumeStartSector,
                     pVolume->SmartLogsStartSector);

   ASPRINTF (&pInfo2, "AcquirySystemGUID        %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
             pVolume->AcquirySystemGUID[ 0], pVolume->AcquirySystemGUID[ 1], pVolume->AcquirySystemGUID[ 2], pVolume->AcquirySystemGUID[ 3],
             pVolume->AcquirySystemGUID[ 4], pVolume->AcquirySystemGUID[ 5], pVolume->AcquirySystemGUID[ 6], pVolume->AcquirySystemGUID[ 7],
             pVolume->AcquirySystemGUID[ 8], pVolume->AcquirySystemGUID[ 9], pVolume->AcquirySystemGUID[10], pVolume->AcquirySystemGUID[11],
             pVolume->AcquirySystemGUID[12], pVolume->AcquirySystemGUID[13], pVolume->AcquirySystemGUID[14], pVolume->AcquirySystemGUID[15]);

   if      (pHeader2) { pHdr = pHeader2; HdrLen = Header2Len; }
   else if (pHeader ) { pHdr = pHeader;  HdrLen = HeaderLen;  }
// pHdr = pHeader; HdrLen = HeaderLen;
   if (pHdr)
   {
      pText   = (char *) malloc (MaxTextSize);
      if (pText == NULL)
         return AEWF_MEMALLOC_FAILED;

      DstLen0 = MaxTextSize;
      zrc = uncompress ((unsigned char *)pText, &DstLen0, (const Bytef*)pHdr, HdrLen);
      UncompressedLen = DstLen0;
      if (zrc != Z_OK)
         return AEWF_UNCOMPRESS_HEADER_FAILED;
      if (pHeader2)  // We must convert from silly Windows 2 byte wchar_t to
      {              // correct Unix 4 byte wchar_t, before we can convert to UTF8
          wchar_t *pTemp = (wchar_t*) malloc ((UncompressedLen/2)*sizeof(wchar_t));
          if (pTemp == NULL)
             return AEWF_MEMALLOC_FAILED;
          for (unsigned i=0; i<(UncompressedLen/2); i++)
             pTemp[i] =   (wchar_t) (((unsigned char*)pText)[2*i  ])  |
                        (((wchar_t) (((unsigned char*)pText)[2*i+1])) << 8);
          wcstombs(pText, pTemp, UncompressedLen/2);
          free (pTemp);
      }

      // Extract descriptor and data lines
      // ---------------------------------
      pCurrent = pText;    // pText may start with BOM (Header2), but that's no problem as
      while (pCurrent)     // first line anyway never is the "main" line.
      {
          if (strcasestr(pCurrent, "main") == pCurrent)  // The header line is the one that
             break;                                      // follows the line beginning with "main"
          pCurrent = strstr (pCurrent, "\n");
          if (pCurrent)
             pCurrent++;
      }
      if (pCurrent)
      {
         pDesc = strstr (pCurrent, "\n");
         if (pDesc)
         {
            *pDesc++ = '\0';
            pData = strstr (pDesc, "\n");
            if (pData)
            {
               *pData++ = '\0';
               pEnd = strstr (pData, "\n");
               if (pEnd)
                  *pEnd = '\0';
            }
         }
      }

      // Scan descriptor and data lines
      // ------------------------------

      char       *pCurDesc = pDesc;
      char       *pCurData = pData;
      const char *pField;
      char       *pTabDesc;
      char       *pTabData;
      char       *pValue;
      int          wr = 0;
      time_t       Time;
      struct tm  *pTM;
      char         TimeBuff[64];

      if (pDesc && pData)
      {
         pInfo3 = (char *) malloc (strlen (pData) + 4096);
         if (pInfo3 == NULL)
            return AEWF_MEMALLOC_FAILED;

         while (*pCurDesc && *pCurData)
         {
            pTabDesc = strstr (pCurDesc, "\t");
            pTabData = strstr (pCurData, "\t");
            if (pTabDesc) *pTabDesc = '\0';
            if (pTabData) *pTabData = '\0';
            if      (strcasecmp(pCurDesc, "a" ) == 0) pField = "Description";
            else if (strcasecmp(pCurDesc, "c" ) == 0) pField = "Case";
            else if (strcasecmp(pCurDesc, "n" ) == 0) pField = "Evidence";
            else if (strcasecmp(pCurDesc, "e" ) == 0) pField = "Examiner";
            else if (strcasecmp(pCurDesc, "t" ) == 0) pField = "Notes";
            else if (strcasecmp(pCurDesc, "md") == 0) pField = "Model";
            else if (strcasecmp(pCurDesc, "sn") == 0) pField = "Serial number";
            else if (strcasecmp(pCurDesc, "av") == 0) pField = "Imager version";
            else if (strcasecmp(pCurDesc, "ov") == 0) pField = "OS version";
            else if (strcasecmp(pCurDesc, "m" ) == 0) pField = "Acquired time";
            else if (strcasecmp(pCurDesc, "u" ) == 0) pField = "System time";
            else if (strcasecmp(pCurDesc, "p" ) == 0) pField = NULL;
            else if (strcasecmp(pCurDesc, "dc") == 0) pField = NULL;
            else                                      pField = "--";
            if (pField)
            {
               pValue = pCurData;
               if (strstr (pField, "time"))
               {
                  size_t w;

                  Time = atoll (pCurData);
                  pTM = localtime (&Time);
                  pValue = &TimeBuff[0];
                  w = strftime (pValue, sizeof(TimeBuff), "%Y-%m-%d %H:%M:%S (%z)", pTM);
                  sprintf (&pValue[w], " (epoch %s)", pCurData);
               }
               wr += sprintf (&pInfo3[wr], "%-17s %s\n", pField, pValue);
            }
            if (!pTabDesc || !pTabData)
               break;
            pCurDesc = pTabDesc+1;
            pCurData = pTabData+1;
         }
      }
   }

   if (pAewf->Segments == 1)
        ASPRINTF (&pInfo4, "%u segment file: %s\n",
                  pAewf->Segments,
                  pAewf->pSegmentArr[0].pName)
   else ASPRINTF (&pInfo4, "%u segment files\n   First: %s\n   Last:  %s\n",
                  pAewf->Segments,
                  pAewf->pSegmentArr[0                ].pName,
                  pAewf->pSegmentArr[pAewf->Segments-1].pName);

   ASPRINTF (&pInfo5, "%u tables\n", pAewf->Tables);

   if (pInfo3)
        ASPRINTF (&pAewf->pInfo, "%s%s\n%s\n%s%s", pInfo1, pInfo2, pInfo3, pInfo4, pInfo5)
   else ASPRINTF (&pAewf->pInfo, "%s%s%s%s"      , pInfo1, pInfo2,         pInfo4, pInfo5)

   free (pInfo1);
   free (pInfo2);
   free (pInfo4);
   free (pInfo5);
   if (pText ) free (pText );
   if (pInfo3) free (pInfo3);

   return AEWF_OK;
}

/*
 * AewfOpen
 */
int AewfOpen(void **pp_handle,
             const char **pp_filename_arr,
             uint64_t filename_arr_len)
{
   t_pAewf                 pAewf;
   t_AewfFileHeader         FileHeader;
   t_AewfSection            Section;
   FILE                   *pFile;
   t_pSegment              pSegment;
   t_pTable                pTable;
   uint64_t       Pos;
   t_pAewfSectionTable     pEwfTable    = NULL;
   t_pAewfSectionVolume    pVolume      = NULL;
   char          *pHeader      = NULL;
   char          *pHeader2     = NULL;
   int                      LastSection;
   unsigned int             SectionSectorsSize;
   unsigned                 HeaderLen  = 0;
   unsigned                 Header2Len = 0;

   // Create handle and clear it
   // --------------------------
   *pp_handle = NULL;
   pAewf = (t_pAewf) malloc (sizeof(t_Aewf));
   if (pAewf == NULL)
      return AEWF_MEMALLOC_FAILED;
   memset (pAewf, 0, sizeof(t_Aewf));
   pAewf->ChunkInBuff       = ULONG_LONG_MAX;
   pAewf->pErrorText        = NULL;
   pAewf->pStatsFilename    = NULL;
   pAewf->StatsRefresh      = 10;

   pAewf->SegmentCacheHits     = 0;
   pAewf->SegmentCacheMisses   = 0;
   pAewf->TableCacheHits       = 0;
   pAewf->TableCacheMisses     = 0;
   pAewf->ChunkCacheHits       = 0;
   pAewf->ChunkCacheMisses     = 0;
   pAewf->ReadOperations       = 0;
   pAewf->DataReadFromImage    = 0;
   pAewf->DataReadFromImageRaw = 0;
   pAewf->DataRequestedByCaller= 0;
   pAewf->TablesReadFromImage  = 0;

   // Create pSegmentArr and put the segment files in it
   // --------------------------------------------------
   int SegmentArrLen  = filename_arr_len * sizeof(t_Segment);
   pAewf->pSegmentArr = (t_pSegment) malloc (SegmentArrLen);
   pAewf->Segments    = filename_arr_len;
   if (pAewf->pSegmentArr == NULL)
      return AEWF_MEMALLOC_FAILED;
   memset (pAewf->pSegmentArr, 0, SegmentArrLen);

   for (unsigned i=0; i<filename_arr_len; i++)
   {
      pSegment = &pAewf->pSegmentArr[i];
      pSegment->pName = canonicalize_file_name (pp_filename_arr[i]); // canonicalize_file_name allocates a buffer

      CHK (OpenFile (&pFile, pSegment->pName))
      CHK (ReadFilePos (pFile, (void*)&FileHeader, sizeof(FileHeader), 0))
//      DEBUG_PRINTF ("Segment %s - %d \n", pp_filename_arr[i], FileHeader.SegmentNumber);

      pSegment->Number   = FileHeader.SegmentNumber;
      pSegment->LastUsed = 0;
      pSegment->pFile    = NULL;

      CHK (CloseFile (&pFile))
   }

   // Put segment array into correct sequence and check if segment number are correct
   // -------------------------------------------------------------------------------
   qsort (pAewf->pSegmentArr, pAewf->Segments, sizeof (t_Segment), &QsortCompareSegments);
   for (unsigned i=0; i<pAewf->Segments; i++)
   {
      if ((i+1) != pAewf->pSegmentArr[i].Number)
         return AEWF_INVALID_SEGMENT_NUMBER;
   }

   // Find all tables in the segment files
   // ------------------------------------
   pAewf->pTableArr   = NULL;
   pAewf->Tables      = 0;
   pAewf->Chunks      = 0;
   SectionSectorsSize = 0;

   DEBUG_PRINTF ("Reading tables\n");
   for (unsigned i=0; i<pAewf->Segments; i++)
   {
      pSegment = &pAewf->pSegmentArr[i];
      CHK (OpenFile (&pFile, pSegment->pName))
      CHK (ReadFilePos (pFile, &FileHeader, sizeof(FileHeader), 0))
      Pos = sizeof (FileHeader);
      DEBUG_PRINTF ("Segment %s ", pSegment->pName);
      do
      {
         CHK (ReadFilePos (pFile, &Section, sizeof (t_AewfSection), Pos))

         if (strcasecmp ((char *)Section.Type, "sectors") == 0)
         {
            SectionSectorsSize = Section.Size;
         }
         else if (strcasecmp ((char *)Section.Type, "table") == 0)
         {
            if (pVolume == NULL)
               return AEWF_VOLUME_MUST_PRECEDE_TABLES;
            if (SectionSectorsSize == 0)
               return AEWF_SECTORS_MUST_PRECEDE_TABLES;
            pAewf->Tables++;
            pAewf->pTableArr = (t_pTable) realloc (pAewf->pTableArr, pAewf->Tables * sizeof (t_Table));
            CHK (ReadFileAlloc (pFile, (void**) &pEwfTable, sizeof(t_AewfSectionTable))) // No need to read the actual offset table
            pTable = &pAewf->pTableArr[pAewf->Tables-1];
            pTable->Nr                 = pAewf->Tables-1;
            pTable->pSegment           = pSegment;
            pTable->Offset             = Pos + sizeof (t_AewfSection);
            pTable->Size               = Section.Size;
            pTable->ChunkCount         = pEwfTable->ChunkCount;
            pTable->LastUsed           = 0;
            pTable->pEwfTable          = NULL;
            pTable->ChunkFrom          = pAewf->Chunks;
            pTable->SectionSectorsSize = SectionSectorsSize;
            pAewf->Chunks             += pTable->ChunkCount;
            pTable->ChunkTo            = pAewf->Chunks-1;
            DEBUG_PRINTF ("t%d", pTable->ChunkCount)
            free (pEwfTable);
            pEwfTable = NULL;
            SectionSectorsSize = 0;
         }
         else if ((strcasecmp ((char *)Section.Type, "header") == 0) && (pHeader==NULL))
         {
            HeaderLen = Section.Size - sizeof(t_AewfSection);
            CHK (ReadFileAlloc (pFile, (void**) &pHeader, HeaderLen))
         }
         else if ((strcasecmp ((char *)Section.Type, "header2") == 0) && (pHeader2==NULL))
         {
            Header2Len = Section.Size - sizeof(t_AewfSection);
            CHK (ReadFileAlloc (pFile, (void**) &pHeader2, Header2Len))
         }
         else if ((strcasecmp ((char *)Section.Type, "volume") == 0) && (pVolume==NULL))
         {
            CHK (ReadFileAlloc (pFile, (void**) &pVolume, sizeof(t_AewfSectionVolume)))
            pAewf->Sectors    = pVolume->SectorCount;
            pAewf->SectorSize = pVolume->BytesPerSector;
            pAewf->ChunkSize  = pVolume->SectorsPerChunk * pVolume->BytesPerSector;
            pAewf->ImageSize  = pAewf->Sectors * pAewf->SectorSize;
            DEBUG_PRINTF ("%lld sectors à %lld bytes", pAewf->Sectors, pAewf->SectorSize)
         }

         LastSection = (Pos == Section.OffsetNextSection);
         Pos = Section.OffsetNextSection;
      } while (!LastSection);
      DEBUG_PRINTF ("\n");
      CHK (CloseFile (&pFile))
   }
   if (pVolume == NULL)
      return AEWF_VOLUME_MISSING;

   if (pAewf->Chunks != pVolume->ChunkCount)
      return AEWF_WRONG_CHUNK_COUNT;

   pAewf->ChunkBuffSize = pAewf->ChunkSize + 4096; // reserve some extra space (for CRC and as compressed data might be slightly larger than uncompressed data with some imagers)
   pAewf->pChunkBuffCompressed   = (char *) malloc (pAewf->ChunkBuffSize);
   pAewf->pChunkBuffUncompressed = (char *) malloc (pAewf->ChunkBuffSize);
   if ((pAewf->pChunkBuffCompressed   == NULL) ||
       (pAewf->pChunkBuffUncompressed == NULL))
      return AEWF_MEMALLOC_FAILED;

   pAewf->MaxTableCache   = 10*1024*1024;
   pAewf->MaxOpenSegments = 10;
   pAewf->TableCache      = 0;
   pAewf->OpenSegments    = 0;

   *((t_pAewf**)pp_handle)=(void*)pAewf;

   CHK (CreateInfoData (pAewf, pVolume, pHeader, HeaderLen, pHeader2, Header2Len))
   free (pVolume);
   free (pHeader);
   free (pHeader2);

   return AEWF_OK;
}

/*
 * AewfInfo
 */
int AewfGetInfofileContent(void *p_handle, const char **pp_info_buf) {
  *pp_info_buf=((t_pAewf)p_handle)->pInfo;
  return AEWF_OK;
}

/*
 * AewfSize
 */
int AewfSize(void *p_handle, uint64_t *p_size) {
  *p_size = ((t_pAewf)p_handle)->ImageSize;
  return AEWF_OK;
}

static int AewfOpenSegment (t_pAewf pAewf, t_pTable pTable)
{
   t_pSegment pOldestSegment;

   if (pTable->pSegment->pFile != NULL) // is already opened ?
   {
      pAewf->SegmentCacheHits++;
      return AEWF_OK;
   }
   pAewf->SegmentCacheMisses++;

   // Check if another segment file must be closed first
   // --------------------------------------------------
   while (pAewf->OpenSegments >= pAewf->MaxOpenSegments)
   {
      pOldestSegment = NULL;

      for (unsigned i=0; i<pAewf->Segments; i++)
      {
         if (pAewf->pSegmentArr[i].pFile == NULL)
            continue;
         if (pOldestSegment == NULL)
         {
            pOldestSegment = &pAewf->pSegmentArr[i];
         }
         else
         {
            if (pAewf->pSegmentArr[i].LastUsed < pOldestSegment->LastUsed)
               pOldestSegment = &pAewf->pSegmentArr[i];
         }
      }
      if (pOldestSegment == NULL)
         break;

      DEBUG_PRINTF ("Closing %s\n", pOldestSegment->pName);
      CHK (CloseFile (&pOldestSegment->pFile))
      pAewf->OpenSegments--;
   }

   // Read the desired table into RAM
   // -------------------------------
   DEBUG_PRINTF ("Opening %s\n", pTable->pSegment->pName);
   CHK (OpenFile(&pTable->pSegment->pFile, pTable->pSegment->pName))
   pAewf->OpenSegments++;

   return AEWF_OK;
}

static int AewfLoadEwfTable (t_pAewf pAewf, t_pTable pTable)
{
   t_pTable pOldestTable = NULL;

   if (pTable->pEwfTable != NULL) // is already loaded?
   {
      pAewf->TableCacheHits++;
      return AEWF_OK;
   }
   pAewf->TableCacheMisses++;

   // Check if another pEwfTable must be given up first
   // -------------------------------------------------
   while ((pAewf->TableCache + pTable->Size) > pAewf->MaxTableCache)
   {
      pOldestTable = NULL;

      for (unsigned i=0; i<pAewf->Tables; i++)
      {
         if (pAewf->pTableArr[i].pEwfTable == NULL)
            continue;
         if (pOldestTable == NULL)
         {
            pOldestTable = &pAewf->pTableArr[i];
         }
         else
         {
            if (pAewf->pTableArr[i].LastUsed < pOldestTable->LastUsed)
               pOldestTable = &pAewf->pTableArr[i];
         }
      }
      if (pOldestTable == NULL)
         break;
      pAewf->TableCache -= pOldestTable->Size;
      free (pOldestTable->pEwfTable);
      pOldestTable->pEwfTable = NULL;
      DEBUG_PRINTF ("Releasing table %" PRIu64 " (%lu bytes)\n", pOldestTable->Nr, pOldestTable->Size);
   }

   // Read the desired table into RAM
   // -------------------------------
   DEBUG_PRINTF ("Loading table %" PRIu64 " (%lu bytes)\n", pTable->Nr, pTable->Size);
   CHK (AewfOpenSegment (pAewf, pTable));
   CHK (ReadFileAllocPos (pTable->pSegment->pFile, (void**) &pTable->pEwfTable, pTable->Size, pTable->Offset))
   pAewf->TableCache += pTable->Size;
   pAewf->TablesReadFromImage = pTable->Size;

   return AEWF_OK;
}

// AewfReadChunk0 reads one chunk. It expects that the EWF table is present
// in memory and the required segment file is opened.
static int AewfReadChunk0 (t_pAewf pAewf, t_pTable pTable, uint64_t AbsoluteChunk, unsigned TableChunk)
{
   int                  Compressed;
   uint64_t   SeekPos;
   t_pAewfSectionTable pEwfTable;
   unsigned int         Offset;
   unsigned int         ReadLen;
   uLongf               DstLen0;
   int                  zrc;
   uint                 CalcCRC;
   uint               *pStoredCRC;
   uint64_t   ChunkSize;

   pEwfTable  = pTable->pEwfTable;
   if (pEwfTable == NULL)
      return AEWF_ERROR_EWF_TABLE_NOT_READY;
   if (pTable->pSegment->pFile == NULL)
      return AEWF_ERROR_EWF_SEGMENT_NOT_READY;

   Compressed = pEwfTable->OffsetArray[TableChunk] &  AEWF_COMPRESSED;
   Offset     = pEwfTable->OffsetArray[TableChunk] & ~AEWF_COMPRESSED;
   SeekPos    = pEwfTable->TableBaseOffset + Offset;

   if (TableChunk < (pEwfTable->ChunkCount-1))
        ReadLen = (pEwfTable->OffsetArray[TableChunk+1] & ~AEWF_COMPRESSED) - Offset;
   else ReadLen = (pTable->SectionSectorsSize - sizeof(t_AewfSection)) - (Offset - (pEwfTable->OffsetArray[0] & ~AEWF_COMPRESSED));
//   else ReadLen = pAewf->ChunkBuffSize;  // This also  works! It looks as if uncompress is able to find out by itself the real size of the input data.

   if (ReadLen > pAewf->ChunkBuffSize)
      return AEWF_CHUNK_TOO_BIG;

   if (Compressed)
   {
      CHK (ReadFilePos (pTable->pSegment->pFile, pAewf->pChunkBuffCompressed, ReadLen, SeekPos))
      DstLen0 = pAewf->ChunkBuffSize;
      zrc = uncompress ((unsigned char*)pAewf->pChunkBuffUncompressed, &DstLen0, (const Bytef*)pAewf->pChunkBuffCompressed, ReadLen);
      if (zrc != Z_OK)
         return AEWF_UNCOMPRESS_FAILED;
      else if (DstLen0 != pAewf->ChunkSize)
         return AEWF_BAD_UNCOMPRESSED_LENGTH;
      ChunkSize = DstLen0;
   }
   else
   {
      ChunkSize = pAewf->ChunkSize;
      if (AbsoluteChunk == (pAewf->Chunks-1))
      {
         ChunkSize = pAewf->ImageSize % pAewf->ChunkSize;
         if (ChunkSize == 0)
            ChunkSize = pAewf->ChunkSize;
         printf ("Last chunk size %" PRIu64 "\n", ChunkSize);
         printf ("ReadLen         %u\n", ReadLen);
      }
      CHK (ReadFilePos (pTable->pSegment->pFile, pAewf->pChunkBuffUncompressed, ReadLen, SeekPos))
      CalcCRC    =  adler32 (1, (const Bytef *) pAewf->pChunkBuffUncompressed, ChunkSize);
      pStoredCRC = (uint *) (pAewf->pChunkBuffUncompressed + ChunkSize);
      if (CalcCRC != *pStoredCRC)
         return AEWF_CHUNK_CRC_ERROR;
   }
   pAewf->ChunkInBuff                  = AbsoluteChunk;
   pAewf->ChunkBuffUncompressedDataLen = ChunkSize;
   pAewf->DataReadFromImage           += ReadLen;
   pAewf->DataReadFromImageRaw        += ChunkSize;

   return AEWF_OK;
}

static int AewfReadChunk (t_pAewf pAewf, uint64_t AbsoluteChunk, char **ppBuffer, unsigned int *pLen)
{
   t_pTable  pTable;
   int        Found=FALSE;
   unsigned   TableChunk;
   unsigned   TableNr;

   *ppBuffer = pAewf->pChunkBuffUncompressed;

   if (pAewf->ChunkInBuff == AbsoluteChunk)
   {
      *pLen = pAewf->ChunkBuffUncompressedDataLen;
      pAewf->ChunkCacheHits++;
      return AEWF_OK;
   }
   pAewf->ChunkCacheMisses++;

   // Find table containing desired chunk
   // -----------------------------------
   for (TableNr=0; TableNr<pAewf->Tables; TableNr++)
   {
      pTable = &pAewf->pTableArr[TableNr];
      Found  = (AbsoluteChunk >= pTable->ChunkFrom) &&
               (AbsoluteChunk <= pTable->ChunkTo);
      if (Found)
         break;
   }
   if (!Found)
      return AEWF_CHUNK_NOT_FOUND;

   // Load corresponding table and get chunk
   // --------------------------------------
   pTable->LastUsed = time(NULL);                  // Update LastUsed here, in order not to
   pTable->pSegment->LastUsed = pTable->LastUsed;  // remove the required data from cache

   CHK (AewfLoadEwfTable (pAewf, pTable))
   CHK (AewfOpenSegment  (pAewf, pTable));
   if ((AbsoluteChunk - pTable->ChunkFrom) > ULONG_MAX)
      return AEWF_ERROR_IN_CHUNK_NUMBER;
   TableChunk = AbsoluteChunk - pTable->ChunkFrom;
//   DEBUG_PRINTF ("table %d / entry %" PRIu64 " (%s)\n", TableNr, TableChunk, pTable->pSegment->pName)
   CHK (AewfReadChunk0 (pAewf, pTable, AbsoluteChunk, TableChunk))
   *pLen = pAewf->ChunkBuffUncompressedDataLen;

   return AEWF_OK;
}

static int UpdateStats (t_pAewf pAewf, int Force)
{
   time_t   NowT;
   pid_t    pid;
   FILE   *pFile;
   char   *pFilename       = NULL;
   char   *pCurrentWorkDir = NULL;

   time (&NowT);
   if (pAewf->pStatsFilename)
   {
      if (((NowT - pAewf->LastStatsUpdate) >= (int)pAewf->StatsRefresh) || Force)
      {
         pAewf->LastStatsUpdate = NowT;

         pid = getpid ();
         ASPRINTF (&pFilename, "%s_%d", pAewf->pStatsFilename, pid)
         pFile = fopen (pFilename, "w");
         if (pFile == NULL) // May be the file is locked by someone else, let's retry in 1 second
         {
            pAewf->LastStatsUpdate = NowT - pAewf->StatsRefresh + 1;
            return AEWF_OK;
         }

         fprintf (pFile, "Cache         hits      misses  ratio\n");
         fprintf (pFile, "-------------------------------------\n");
         fprintf (pFile, "Segment %10" PRIu64 "  %10" PRIu64 "  %5.1f%%\n", pAewf->SegmentCacheHits, pAewf->SegmentCacheMisses, (100.0*pAewf->SegmentCacheHits)/(pAewf->SegmentCacheHits+pAewf->SegmentCacheMisses));
         fprintf (pFile, "Table   %10" PRIu64 "  %10" PRIu64 "  %5.1f%%\n", pAewf->TableCacheHits  , pAewf->TableCacheMisses  , (100.0*pAewf->TableCacheHits)  /(pAewf->TableCacheHits  +pAewf->TableCacheMisses  ));
         fprintf (pFile, "Chunk   %10" PRIu64 "  %10" PRIu64 "  %5.1f%%\n", pAewf->ChunkCacheHits  , pAewf->ChunkCacheMisses  , (100.0*pAewf->ChunkCacheHits)  /(pAewf->ChunkCacheHits  +pAewf->ChunkCacheMisses  ));
         fprintf (pFile, "\n");
         fprintf (pFile, "Read operations          %10" PRIu64 "\n", pAewf->ReadOperations);
         fprintf (pFile, "Data read from image     %10.1f MiB (compressed)\n", pAewf->DataReadFromImage    / (1024.0*1024.0));
         fprintf (pFile, "Data read from image     %10.1f MiB (raw)\n"       , pAewf->DataReadFromImageRaw / (1024.0*1024.0));
         fprintf (pFile, "Data requested by caller %10.1f MiB\n"             , pAewf->DataRequestedByCaller/ (1024.0*1024.0));
         fprintf (pFile, "Tables read from image   %10.1f MiB\n"             , pAewf->TablesReadFromImage  / (1024.0*1024.0));

         pCurrentWorkDir = getcwd (NULL, 0);
         if (pCurrentWorkDir == NULL)
            return AEWF_MEMALLOC_FAILED;
         fprintf (pFile, "\nCurrent working directory: %s\n", pCurrentWorkDir);
         free (pCurrentWorkDir);

         (void) fclose (pFile);
         free (pFilename);

         return AEWF_OK;
      }
   }
   return AEWF_OK;
}

/*
 * AewfRead
 */
int AewfRead(void *p_handle,
             uint64_t seek,
             char *p_buf,
             uint32_t count)
{
  uint64_t chunk;
  char *p_chunk_buffer;
  unsigned int chunk_len, ofs, to_copy;

  ((t_pAewf)p_handle)->ReadOperations++;
  ((t_pAewf)p_handle)->DataRequestedByCaller+=count;

  if((seek+count)>((t_pAewf)p_handle)->ImageSize) {
    return AEWF_READ_BEYOND_IMAGE_LENGTH;
  }

  chunk=seek/((t_pAewf)p_handle)->ChunkSize;
  ofs=seek%((t_pAewf)p_handle)->ChunkSize;

  while(count) {
    CHK(AewfReadChunk((t_pAewf)p_handle,chunk,&p_chunk_buffer,&chunk_len))
    to_copy=GETMIN(chunk_len-ofs,count);
    memcpy(p_buf,p_chunk_buffer+ofs,to_copy);
    count-=to_copy;
    p_buf+=to_copy;
    ofs=0;
    chunk++;
  }
  CHK(UpdateStats((t_pAewf)p_handle,FALSE))

  return AEWF_OK;
}

/*
 * AewfClose
 */
int AewfClose(void **pp_handle) {
  t_pTable   p_table;
  t_pSegment p_segment;
  t_pAewf    p_aewf=*((t_pAewf*)pp_handle);

  CHK(UpdateStats(p_aewf,TRUE))

  for(unsigned i=0;i<p_aewf->Tables;i++) {
    p_table=&p_aewf->pTableArr[i];
    if(p_table->pEwfTable) free(p_table->pEwfTable);
  }

  for(unsigned i=0;i<p_aewf->Segments;i++) {
    p_segment=&p_aewf->pSegmentArr[i];
    if(p_segment->pFile) CloseFile(&p_segment->pFile);
    free(p_segment->pName);
  }

  free(p_aewf->pTableArr);
  free(p_aewf->pSegmentArr);
  free(p_aewf->pChunkBuffCompressed);
  free(p_aewf->pChunkBuffUncompressed);
  if(p_aewf->pStatsFilename) free(p_aewf->pStatsFilename);
  memset(p_aewf,0,sizeof(t_Aewf));
  free(p_aewf);
  *pp_handle=NULL;

  return AEWF_OK;
}

// Option handling
// ---------------

static const char *pOptionPrefix    = "aewf_";
static const char   OptionSeparator = ',';


static int SetError (t_pAewf pAewf, char **ppError, const char *pFormat, ...)
{
   va_list VaList;

   if (pAewf->pErrorText)
      free (pAewf->pErrorText);

   va_start(VaList, pFormat);
   if (vasprintf (&pAewf->pErrorText, pFormat, VaList) < 0)
      return AEWF_VASPRINTF_FAILED;
   va_end(VaList);

   *ppError = pAewf->pErrorText;

   return AEWF_OK;
}

static int CheckOption (const char *pOption, int OptionLen, const char *pOptionName, const char **ppValue, int *pValueLen)
{
   int Found;

   *ppValue   = NULL;
   *pValueLen = 0;
   Found = (strcasestr (pOption, pOptionName) == pOption);
   if (Found)
   {
      *ppValue   = pOption   + strlen (pOptionName);
      *pValueLen = OptionLen - strlen (pOptionName);
   }
   return Found;
}

static int ValueToInt (t_pAewf pAewf, const char *pValue, int ValueLen, char **ppError)
{
   char *pTail;
   int    Value;

   *ppError = NULL;
   Value = strtoll (pValue, &pTail, 10);
   if (pTail != (pValue + ValueLen))
      CHK (SetError(pAewf, ppError, "Invalid option value %s", pValue))
   return Value;
}

static char *ValueToStr (t_pAewf pAewf, const char *pValue, int ValueLen, char **ppError)
{
   *ppError = NULL;
   return strndup (pValue, ValueLen);
}

static int ReadOption (t_pAewf pAewf, char *pOption, int OptionLen, char **ppError)
{
   const char *pValue;
   int    ValueLen;

   *ppError = NULL;
   if      (CheckOption (pOption, OptionLen, "maxfiles=", &pValue, &ValueLen)) pAewf->MaxOpenSegments = ValueToInt (pAewf, pValue, ValueLen, ppError);
   else if (CheckOption (pOption, OptionLen, "maxmem="  , &pValue, &ValueLen)) pAewf->MaxTableCache   = ValueToInt (pAewf, pValue, ValueLen, ppError)*1024*1024;
   else if (CheckOption (pOption, OptionLen, "stats="   , &pValue, &ValueLen)) pAewf->pStatsFilename  = ValueToStr (pAewf, pValue, ValueLen, ppError);
   else if (CheckOption (pOption, OptionLen, "refresh=" , &pValue, &ValueLen)) pAewf->StatsRefresh    = ValueToInt (pAewf, pValue, ValueLen, ppError);
   else    CHK (SetError(pAewf, ppError, "Unknown option %s%s", pOptionPrefix, pOption))

   return AEWF_OK;
}

/*
 * AewfOptionsParse
 */
int AewfOptionsParse(void *p_handle, char *p_options, char **pp_error) {
   char *pCurrent;
   char *pOption;
   char *pSep;
   int    Found;

   pCurrent = p_options;
   while (*pCurrent)
   {
      pSep = strchr (pCurrent, OptionSeparator);
      if (pSep == NULL)
         pSep = pCurrent + strlen(pCurrent);

      Found = FALSE;
      if ((pSep - pCurrent) >= (int)strlen(pOptionPrefix))   // Check for options starting with our prefix
      {
         Found = (strncasecmp (pCurrent, pOptionPrefix, strlen(pOptionPrefix)) == 0);
         if (Found)
         {
            pOption = pCurrent + strlen(pOptionPrefix);
            CHK (ReadOption ((t_pAewf)p_handle, pOption, pSep-pOption, pp_error))
            if (*pp_error)
               break;
            memmove (pCurrent, pSep+1, strlen(pSep)+1);
         }
      }
      if (!Found)
      {
         if (*pSep)
              pCurrent = pSep+1;
         else pCurrent = pSep;
      }
   }
   if (p_options[strlen(p_options)-1] == OptionSeparator)  // Remove trailing separator if there is one
      p_options[strlen(p_options)-1] = '\0';

   DEBUG_PRINTF ("Max open segment files %" PRIu64 "\n"                  , ((t_pAewf)p_handle)->MaxOpenSegments)
   DEBUG_PRINTF ("Max table cache        %" PRIu64 " bytes (%0.1f MiB)\n", ((t_pAewf)p_handle)->MaxTableCache, ((t_pAewf)p_handle)->MaxTableCache / (1024.0*1024.0))
   DEBUG_PRINTF ("Stats file             %s\n"                    , ((t_pAewf)p_handle)->pStatsFilename ? ((t_pAewf)p_handle)->pStatsFilename : "-none-")
   DEBUG_PRINTF ("Stats refresh          %" PRIu64 "s\n"                 , ((t_pAewf)p_handle)->StatsRefresh);
   DEBUG_PRINTF ("Unused options         %s\n"                    , pOptions);

   return AEWF_OK;
}

int AewfOptionsHelp(const char **pp_help) {
  *pp_help = "   aewf_maxmem   The maximum amount of memory (in MiB) used for caching image offset\n"
             "                 tables.\n"
             "   aewf_maxfiles The maximum number of image segment files opened at the same time.\n"
             "   aewf_stats    A filename that will be used for outputting statistical data at\n"
             "                 regular intervals. The process id is automatically appended to the\n"
             "                 given filename.\n"
             "   aewf_refresh  The update interval, in seconds, for the statistical data output.\n"
             "                 Ignored if aewf_stats is not set. The default value is 10.\n"
             "   Example: aewf_maxmem=64,aewf_stats=mystats,aewf_refresh=2"
             ;
  return AEWF_OK;
}

void AewfFreeBuffer(void *p_buf) {
  free(p_buf);
}

// -----------------------------------------------------
//            Small main routine for testing
//            It converts an EWF file into dd
// -----------------------------------------------------

#ifdef AEWF_MAIN_FOR_TESTING

int main(int argc, const char *argv[])
{
   t_pAewf              pAewf;
   uint64_t  TotalSize;
   uint64_t  Remaining;
   uint64_t  Read;
   uint64_t  Pos;
   unsigned int        BuffSize = 13*65536; // A multiple of chunk size for good performance
   char       Buff[BuffSize];
   FILE              *pFile;
   int                 Percent;
   int                 PercentOld;
   int                 rc;
   char              *pOptions = NULL;
   char              *pError   = NULL;
   const char        *pHelp;
   const char        *pInfoBuff;

   #ifdef CREATE_REVERSE_FILE
      FILE              *pFileRev;
      uint64_t  PosRev;
      #ifdef REVERSE_FILE_USES_SEPARATE_HANDLE
         t_pAewf         pAewfRev;
      #else
         #define pAewfRev pAewf
      #endif
   #endif

   setbuf(stdout, NULL);
   setbuf(stderr, NULL);
   setlocale (LC_ALL, "");

   #define PRINT_ERROR_AND_EXIT(...) \
   {                                 \
      printf (__VA_ARGS__);          \
      exit (1);                      \
   }

   printf ("EWF to DD converter - result file is named dd\n");
   printf ("   Result file is named dd");
   #ifdef CREATE_REVERSE_FILE
      printf ("; Also creates a backwards read file named rev");
      #ifdef REVERSE_FILE_USES_SEPARATE_HANDLE
         printf ("; Uses separate AEWF handle for reverse file");
      #else
         printf ("; Uses the same AEWF handle for reverse file");
      #endif
   #endif
   printf ("\n");


   if (argc < 2)
   {
      (void) AewfOptionHelp (&pHelp);
      printf ("Usage: %s <EWF segment file 1> <EWF segment file 2> <...> [-comma_separated_options]\n", argv[0]);
      printf ("Possible options:\n%s\n", pHelp);
      printf ("The output file will be named dd.\n");
      exit (1);
   }

   if (argv[argc-1][0] == '-')
   {
      pOptions = strdup (&(argv[argc-1][1]));
      argc--;
   }

   rc = AewfOpen (&pAewf, argc-1, &argv[1]);
   if (rc != AEWF_OK)
      PRINT_ERROR_AND_EXIT ("Cannot open EWF files, rc=%d\n", rc)
   if (pOptions)
      CHK (AewfOptions(pAewf, pOptions, &pError))
   if (pError)
      PRINT_ERROR_AND_EXIT ("Error while setting options: %s", pError);

   #if defined(CREATE_REVERSE_FILE) && defined(REVERSE_FILE_USES_SEPARATE_HANDLE)
      rc = AewfOpen (&pAewfRev, argc-1, &argv[1]);
      if (rc != AEWF_OK)
         PRINT_ERROR_AND_EXIT ("Cannot open EWF files, rc=%d\n", rc)
      if (pOptions)
         CHK (AewfOptions(pAewfRev, pOptions, &pError))
      if (pError)
         PRINT_ERROR_AND_EXIT ("Error while setting options: %s", pError);
   #endif

   CHK (AewfInfo (pAewf, &pInfoBuff))
   if (pInfoBuff)
      printf ("Contents of info buffer:\n%s\n", pInfoBuff);

   CHK (AewfSize (pAewf, &TotalSize))
   printf ("Total size: %" PRIu64 " bytes\n", TotalSize);
   Remaining = TotalSize;

   pFile = fopen ("dd", "w");
   if (pFile == NULL)
      PRINT_ERROR_AND_EXIT("Cannot open destination file\n");

   #ifdef CREATE_REVERSE_FILE
      pFileRev = fopen ("rev", "w");
      if (pFileRev == NULL)
         PRINT_ERROR_AND_EXIT("Cannot open reverse destination file\n");
      PosRev = TotalSize;
   #endif

   Remaining  = TotalSize;
   Pos        = 0;
   PercentOld = -1;
   while (Remaining)
   {
//      DEBUG_PRINTF ("Pos %" PRIu64 " -- Remaining %" PRIu64 " ", Pos, Remaining);
      Read = GETMIN (Remaining, BuffSize);
      rc = AewfRead (pAewf, Pos, &Buff[0], Read);
      if (rc != AEWF_OK)
         PRINT_ERROR_AND_EXIT("Error %d while calling AewfRead\n", rc);

      if (fwrite (Buff, Read, 1, pFile) != 1)
         PRINT_ERROR_AND_EXIT("Could not write to destination file\n");

      Remaining -= Read;
      Pos       += Read;

      #ifdef CREATE_REVERSE_FILE
         PosRev -= Read;
         rc = AewfRead (pAewf, PosRev, &Buff[0], Read);
         if (rc != AEWF_OK)
            PRINT_ERROR_AND_EXIT("Error %d while reverse calling AewfRead\n", rc);

         if (fseeko (pFileRev, PosRev, SEEK_SET))
            return AEWF_FILE_SEEK_FAILED;

         if (fwrite (Buff, Read, 1, pFileRev) != 1)
            PRINT_ERROR_AND_EXIT("Could not write to reverse destination file\n");
      #endif


      Percent = (100*Pos) / TotalSize;
      if (Percent != PercentOld)
      {
         printf ("\r%d%% done...", Percent);
         PercentOld = Percent;
      }
   }
   if (AewfClose (&pAewf))
      PRINT_ERROR_AND_EXIT("Error while closing EWF files\n");
   if (fclose (pFile))
      PRINT_ERROR_AND_EXIT ("Error while closing destination file\n");

   #ifdef CREATE_REVERSE_FILE
      #ifdef REVERSE_FILE_USES_SEPARATE_HANDLE
         if (AewfClose (&pAewfRev))
            PRINT_ERROR_AND_EXIT("Error while closing reverse EWF files\n");
      #endif
      if (fclose (pFileRev))
         PRINT_ERROR_AND_EXIT ("Error while closing reverse destination file\n");
   #endif

   printf ("\n");
   return 0;
}

#endif

