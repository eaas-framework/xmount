/*******************************************************************************
* xmount Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing EWF images created by Guymager and others.                         *
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

// Please don't touch source code formatting!

#ifdef LINTING
//   #define _LARGEFILE_SOURCE
//   #define _FILE_OFFSET_BITS 64
   #define AEWF_STANDALONE
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <limits.h>
#include <time.h>       //lint !e537 !e451  Include file messages
#include <zlib.h>
#include <unistd.h>     //lint !e537
#include <wchar.h>      //lint !e537 !e451
#include <stdarg.h>     //lint !e537 !e451
#include <limits.h>     //lint !e537 !e451
#include <errno.h>
#include <pthread.h>

#include "../libxmount_input.h"

#include "libxmount_input_aewf.h"

#ifdef AEWF_STANDALONE
   #define CREATE_REVERSE_FILE
//   #define REVERSE_FILE_USES_SEPARATE_HANDLE
   #define LOG_STDOUT TRUE
#endif

//#ifdef AEWF_STANDALONE
//  #define _GNU_SOURCE
//#endif


#define AEWF_OPTION_TABLECACHE      "aewfmaxmem"
#define AEWF_OPTION_MAXOPENSEGMENTS "aewfmaxfiles"
#define AEWF_OPTION_STATS           "aewfstats"
#define AEWF_OPTION_STATSREFRESH    "aewfrefresh"
#define AEWF_OPTION_LOG             "aewflog"
#define AEWF_OPTION_THREADS         "aewfthreads"

static int         AewfClose           (void *pHandle);
static const char* AewfGetErrorMessage (int ErrNum);

const uint64_t AEWF_DEFAULT_TABLECACHE      = 10;  // MiB
const uint64_t AEWF_DEFAULT_MAXOPENSEGMENTS = 10;
const uint64_t AEWF_DEFAULT_STATSREFRESH    = 10;
const uint64_t AEWF_DEFAULT_THREADS         =  4;  // There normally is no sense in using higher values, as - according to out statistics - we never get called for reading
                                                   // more than 128k of data (there's only 1 exception: the very 1st read request from xmount itself). With the default EWF
                                                   // chunk size of 32K, 4 threads are enough for running the whole decompression in parallel.

// ----------------------------
//  Logging and error handling
// ----------------------------

#define LOG_HEADER_LEN 80

int LogvEntry (const char *pLogPath, uint8_t LogStdout, const char *pFileName, const char *pFunctionName, int LineNr, const char *pFormat, va_list pArguments)
{
   time_t       NowT;
   struct tm  *pNowTM;
   FILE       *pFile;
   int          wr;
   char       *pFullLogFileName = NULL;
   const char *pBase;
   char         LogLineHeader[1024];
   pid_t        OwnPID;
   va_list     pArguments0;

   if (!LogStdout && (pLogPath==NULL))
      return AEWF_OK;

   time (&NowT);
   pNowTM = localtime (&NowT);
   OwnPID = getpid();  // pthread_self()
   wr  = (int) strftime (&LogLineHeader[0] , sizeof(LogLineHeader)   , "%a %d.%b.%Y %H:%M:%S ", pNowTM); //lint !e713
   wr += snprintf (&LogLineHeader[wr], sizeof(LogLineHeader)-wr, "%5d ", OwnPID);                        //lint !e737

   if (pFileName && pFunctionName)
   {
      pBase = strrchr(pFileName, '/');
      if (pBase)
         pFileName = pBase+1;
      wr += snprintf (&LogLineHeader[wr], sizeof(LogLineHeader)-wr, "%s %s %d ", pFileName, pFunctionName, LineNr); //lint !e737
   }

//   while (wr < LOG_HEADER_LEN)
//      LogLineHeader[wr++] = ' ';

   if (pLogPath)
   {
      wr = asprintf (&pFullLogFileName, "%s/log_%d", pLogPath, OwnPID);
      if ((wr <= 0) || (pFullLogFileName == NULL))
      {
         if (LogStdout)
            printf ("\nLog file error: Can't build filename");
         return AEWF_MEMALLOC_FAILED;
      }
      else
      {
         pFile = fopen64 (pFullLogFileName, "a");
         if (pFile == NULL)
         {
            if (LogStdout)
               printf ("\nLog file error: Can't be opened");
            return AEWF_CANNOT_OPEN_LOGFILE;
         }
         else
         {
            fprintf  (pFile, "%-*s", LOG_HEADER_LEN, &LogLineHeader[0]);
            va_copy (pArguments0, pArguments);
            vfprintf (pFile, pFormat, pArguments0);
            fprintf  (pFile, "\n");
            fclose   (pFile);
         }
         free (pFullLogFileName);
      }
   }
   if (LogStdout)
   {
      printf  ("%s", &LogLineHeader[0]);
      va_copy (pArguments0, pArguments);
      vprintf (pFormat, pArguments0);
      printf  ("\n");
   }
   return AEWF_OK;
}

int LogEntry (const char *pLogPath, uint8_t LogStdout, const char *pFileName, const char *pFunctionName, int LineNr, const char *pFormat, ...)
{
   va_list VaList;
   int     rc;

   if (!LogStdout && (pLogPath==NULL))
      return AEWF_OK;

   va_start (VaList, pFormat); //lint !e530 Symbol 'VaList' not initialized
   rc = LogvEntry (pLogPath, LogStdout, pFileName, pFunctionName, LineNr, pFormat, VaList);
   va_end(VaList);
   return rc;
}

// CHK requires existance of pAewf handle

#ifdef AEWF_STANDALONE
   #define LOG_ERRORS_ON_STDOUT TRUE
#else
   #define LOG_ERRORS_ON_STDOUT pAewf->LogStdout
#endif

#define CHK(ChkVal)                                                             \
{                                                                               \
   int ChkValRc;                                                                \
   if ((ChkValRc=(ChkVal)) != AEWF_OK)                                          \
   {                                                                            \
      const char *pErr = AewfGetErrorMessage (ChkValRc);                        \
      LogEntry (pAewf->pLogPath, LOG_ERRORS_ON_STDOUT, __FILE__, __FUNCTION__, __LINE__, "Error %d (%s) occured", ChkValRc, pErr); \
      return ChkValRc;                                                          \
   }                                                                            \
}

#define LOG(...) \
   LogEntry (pAewf->pLogPath, pAewf->LogStdout, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);


// AewfCheckError is called before exiting AewfRead. It should not
// be called elsewehere or else the statistics would become wrong.
static void AewfCheckError (t_pAewf pAewf, int Ret, int *pErrno)
{
   *pErrno = 0;
   if (Ret != AEWF_OK)
   {
      pAewf->Errors++;
      pAewf->LastError = Ret;

      if      ((Ret >= AEWF_ERROR_ENOMEM_START) && (Ret <= AEWF_ERROR_ENOMEM_END)) *pErrno = ENOMEM;
      else if ((Ret >= AEWF_ERROR_EINVAL_START) && (Ret <= AEWF_ERROR_EINVAL_END)) *pErrno = EINVAL;
      else                                                                         *pErrno = EIO;    // all other errors
   }
}

// ------------------------------------
//         Internal functions
// ------------------------------------

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

#define NO_SEEK ULLONG_MAX

static int ReadFilePos (t_pAewf pAewf, FILE *pFile, void *pMem, unsigned int Size, uint64_t Pos)
{
   if (Size == 0)
      return AEWF_OK;

   if (Pos != NO_SEEK)
   {
      if (fseeko64 (pFile, Pos, SEEK_SET))
         return AEWF_FILE_SEEK_FAILED;
   }
   if (fread (pMem, Size, 1UL, pFile) != 1)
      return AEWF_FILE_READ_FAILED;

   return AEWF_OK;
}

static int ReadFileAllocPos (t_pAewf pAewf, FILE *pFile, void **ppMem, unsigned int Size, uint64_t Pos)
{
   *ppMem = (void*) malloc (Size);
   if (*ppMem == NULL)
      return AEWF_MEMALLOC_FAILED;

   CHK (ReadFilePos (pAewf, pFile, *ppMem, Size, Pos))
   return AEWF_OK;
}

static int ReadFileAlloc (t_pAewf pAewf, FILE *pFile, void **ppMem, unsigned int Size)
{
   CHK (ReadFileAllocPos (pAewf, pFile, ppMem, Size, NO_SEEK))

   return AEWF_OK;
}

static int QsortCompareSegments (const void *pA, const void *pB)
{
   const t_pSegment pSegmentA = ((const t_pSegment)pA); //lint !e1773 Attempt to cast way const
   const t_pSegment pSegmentB = ((const t_pSegment)pB); //lint !e1773 Attempt to cast way const
   return (int)pSegmentA->Number - (int)pSegmentB->Number;
}

static int CreateInfoData (t_pAewf pAewf, t_pAewfSectionVolume pVolume,
                                          char *pHeader , unsigned HeaderLen,
                                          char *pHeader2, unsigned Header2Len,
                                          t_pAewfSectionHash pMD5)
{
   char      *pInfo1 = NULL;
   char      *pInfo2 = NULL;
   char      *pInfo3 = NULL;
   char      *pInfo4 = NULL;
   char      *pInfo5 = NULL;
   char      *pInfo6 = NULL;
   char      *pHdr   = NULL;
   unsigned    HdrLen= 0;
   char      *pText  = NULL;
   char      *pCurrent;
   char      *pDesc  = NULL;
   char      *pData  = NULL;
   char      *pEnd;
   uLongf    DstLen0;
   int       zrc;
   const int MaxTextSize = 65536;
   unsigned  UncompressedLen;
   int       rc = AEWF_OK;

   #define RET_ERR(ErrCode)  \
   {                         \
      rc = ErrCode;          \
      goto CleanUp;          \
   }

   #define ASPRINTF(...)                \
   {                                    \
      if (asprintf(__VA_ARGS__) < 0)    \
         RET_ERR (AEWF_ASPRINTF_FAILED) \
   }

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
             pVolume->AcquirySystemGUID[ 4], pVolume->AcquirySystemGUID[ 4], pVolume->AcquirySystemGUID[ 6], pVolume->AcquirySystemGUID[ 7],
             pVolume->AcquirySystemGUID[ 8], pVolume->AcquirySystemGUID[ 9], pVolume->AcquirySystemGUID[10], pVolume->AcquirySystemGUID[11],
             pVolume->AcquirySystemGUID[12], pVolume->AcquirySystemGUID[13], pVolume->AcquirySystemGUID[14], pVolume->AcquirySystemGUID[15]);

   if      (pHeader2) { pHdr = pHeader2; HdrLen = Header2Len; }
   else if (pHeader ) { pHdr = pHeader;  HdrLen = HeaderLen;  }
   if (pHdr)
   {
      pText   = (char *) malloc (MaxTextSize);
      if (pText == NULL)
         RET_ERR (AEWF_MEMALLOC_FAILED)
      DstLen0 = MaxTextSize;
      zrc = uncompress ((unsigned char *)pText, &DstLen0, (const Bytef*)pHdr, HdrLen);
      UncompressedLen = DstLen0;
      if (zrc != Z_OK)
         RET_ERR (AEWF_UNCOMPRESS_HEADER_FAILED)
      if (pHeader2)                                         // We must convert from silly Windows 2 byte wchar_t to
      {                                                     // correct Unix 4 byte wchar_t, before we can convert to UTF8
         wchar_t *pTemp  = (wchar_t*) malloc ((UncompressedLen/2)*sizeof(wchar_t));
         wchar_t *pStart = pTemp;

         if (pTemp == NULL)
            RET_ERR (AEWF_MEMALLOC_FAILED)
         for (unsigned i=0; i<(UncompressedLen/2); i++)
            pTemp[i] =   (wchar_t) (((unsigned char*)pText)[2*i  ])  |
                       (((wchar_t) (((unsigned char*)pText)[2*i+1])) << 8);

         if (*pStart == 0xFEFF)    // Jump over BOM if it exists, or else
            pStart++;              // conversion to UTF8 might fail.
         (void) wcstombs(pText, pStart, UncompressedLen/2);
         free (pTemp);
      }

      // Extract descriptor and data lines
      // ---------------------------------
      pCurrent = pText;
      while (pCurrent)
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

      if (pCurDesc && pCurData)
      {
         pInfo3 = (char *) malloc (strlen (pCurData) + 4096);
         if (pInfo3 == NULL)
            RET_ERR (AEWF_MEMALLOC_FAILED)

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
        ASPRINTF (&pInfo4, "%"PRIu64" segment file: %s\n",
                  pAewf->Segments,
                  pAewf->pSegmentArr[0].pName)
   else ASPRINTF (&pInfo4, "%"PRIu64" segment files\n   First: %s\n   Last:  %s\n",
                  pAewf->Segments,
                  pAewf->pSegmentArr[0                ].pName,
                  pAewf->pSegmentArr[pAewf->Segments-1].pName);

   ASPRINTF (&pInfo5, "%"PRIu64" tables\n", pAewf->Tables);

   if (pMD5)
      ASPRINTF  (&pInfo6, "MD5 stored in image: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                          pMD5->MD5[0], pMD5->MD5[1], pMD5->MD5[2 ], pMD5->MD5[3 ], pMD5->MD5[4 ], pMD5->MD5[5 ], pMD5->MD5[6 ], pMD5->MD5[7 ],
                          pMD5->MD5[8], pMD5->MD5[9], pMD5->MD5[10], pMD5->MD5[11], pMD5->MD5[12], pMD5->MD5[13], pMD5->MD5[14], pMD5->MD5[15])
   else
      ASPRINTF  (&pInfo6, "MD5 stored in image: none")

   if (pInfo3)
        ASPRINTF (&pAewf->pInfo, "%s%s\n%s\n%s%s\n%s", pInfo1, pInfo2, pInfo3, pInfo4, pInfo5, pInfo6)
   else ASPRINTF (&pAewf->pInfo, "%s%s%s%s\n%s"      , pInfo1, pInfo2,         pInfo4, pInfo5, pInfo6)

   #undef RET_ERR
   #undef ASPRINTF

CleanUp:
   if (pText ) free (pText );
   if (pInfo1) free (pInfo1);
   if (pInfo2) free (pInfo2);
   if (pInfo3) free (pInfo3);
   if (pInfo4) free (pInfo4);
   if (pInfo5) free (pInfo5);
   if (pInfo6) free (pInfo6);

   return rc;
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

      LOG ("Closing %s", pOldestSegment->pName);
      CHK (CloseFile (&pOldestSegment->pFile))
      pAewf->OpenSegments--;
   }

   // Open the desired segment file
   // -----------------------------
   LOG ("Opening %s", pTable->pSegment->pName);
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
      LOG ("Releasing table %" PRIu64 " (%lu bytes)", pOldestTable->Nr, pOldestTable->Size);
   }

   // Read the desired table into RAM
   // -------------------------------
   LOG ("Loading table %" PRIu64 " (%lu bytes)", pTable->Nr, pTable->Size);
   CHK (AewfOpenSegment (pAewf, pTable));
   CHK (ReadFileAllocPos (pAewf, pTable->pSegment->pFile, (void**) &pTable->pEwfTable, pTable->Size, pTable->Offset))
   pAewf->TableCache += pTable->Size;
   pAewf->TablesReadFromImage += pTable->Size;

   return AEWF_OK;
}

static int UpdateStats (t_pAewf pAewf, int Force)
{
   time_t   NowT;
   pid_t    pid;
   FILE   *pFile;
   char   *pFilename       = NULL;
   char   *pCurrentWorkDir = NULL;

   if (pAewf->pStatsPath)
   {
      time (&NowT);
      if (((NowT - pAewf->LastStatsUpdate) >= (int)pAewf->StatsRefresh) || Force)
      {
         pAewf->LastStatsUpdate = NowT;

         pid = getpid ();
         if (asprintf (&pFilename, "%s/stats_%d", pAewf->pStatsPath, pid) < 0)
            return AEWF_MEMALLOC_FAILED;
         pFile = fopen (pFilename, "w");
         if (pFile == NULL) // May be the file is locked by someone else, let's retry in 1 second
         {
            pAewf->LastStatsUpdate = NowT - pAewf->StatsRefresh + 1;
            return AEWF_OK;
         }

         fprintf (pFile, "Image segment files     %6"PRIu64"\n" , pAewf->Segments);
         fprintf (pFile, "Image tables            %6"PRIu64"\n" , pAewf->Tables);
         fprintf (pFile, "\n");
         fprintf (pFile, "Cache         hits      misses  ratio\n");
         fprintf (pFile, "--------------------------------------\n");
         fprintf (pFile, "Segment %10" PRIu64 "  %10" PRIu64 "  %5.1f%%\n", pAewf->SegmentCacheHits, pAewf->SegmentCacheMisses, (100.0*pAewf->SegmentCacheHits)/(pAewf->SegmentCacheHits+pAewf->SegmentCacheMisses));
         fprintf (pFile, "Table   %10" PRIu64 "  %10" PRIu64 "  %5.1f%%\n", pAewf->TableCacheHits  , pAewf->TableCacheMisses  , (100.0*pAewf->TableCacheHits)  /(pAewf->TableCacheHits  +pAewf->TableCacheMisses  ));
         fprintf (pFile, "Chunk   %10" PRIu64 "  %10" PRIu64 "  %5.1f%%\n", pAewf->ChunkCacheHits  , pAewf->ChunkCacheMisses  , (100.0*pAewf->ChunkCacheHits)  /(pAewf->ChunkCacheHits  +pAewf->ChunkCacheMisses  ));
         fprintf (pFile, "\n");
         fprintf (pFile, "Read operations          %10" PRIu64 "\n", pAewf->ReadOperations);
         fprintf (pFile, "Errors                   %10" PRIu64 "\n", pAewf->Errors);
         fprintf (pFile, "Open segment files       %10" PRIu64"\n" , pAewf->OpenSegments);
         fprintf (pFile, "Last error               %10d (%s)\n"         , pAewf->LastError, AewfGetErrorMessage (pAewf->LastError));
         fprintf (pFile, "Data read from image     %10.1f MiB (compressed)\n", pAewf->DataReadFromImage    / (1024.0*1024.0));
         fprintf (pFile, "Data read from image     %10.1f MiB (raw)\n"       , pAewf->DataReadFromImageRaw / (1024.0*1024.0));
         fprintf (pFile, "Data requested by caller %10.1f MiB\n"             , pAewf->DataRequestedByCaller/ (1024.0*1024.0));
         fprintf (pFile, "Tables read from image   %10.1f MiB\n"             , pAewf->TablesReadFromImage  / (1024.0*1024.0));
         fprintf (pFile, "RAM used as table cache  %10.1f MiB\n"             , pAewf->TableCache           / (1024.0*1024.0));
         fprintf (pFile, "Size of all image tables %10.1f MiB\n"             , pAewf->TotalTableSize       / (1024.0*1024.0));
         fprintf (pFile, "\n");
         fprintf (pFile, "Histogram of read request sizes\n");
         fprintf (pFile, "-------------------------------\n");
         fprintf (pFile, "  0  < Size <= 32K  %10" PRIu64"\n", pAewf->ReadSizesArr[READSIZE_32K]);
         fprintf (pFile, " 32K < Size <= 64K  %10" PRIu64"\n", pAewf->ReadSizesArr[READSIZE_64K]);
         fprintf (pFile, " 64K < Size <= 128K %10" PRIu64"\n", pAewf->ReadSizesArr[READSIZE_128K]);
         fprintf (pFile, "128K < Size <= 256K %10" PRIu64"\n", pAewf->ReadSizesArr[READSIZE_256K]);
         fprintf (pFile, "256K < Size <= 512K %10" PRIu64"\n", pAewf->ReadSizesArr[READSIZE_512K]);
         fprintf (pFile, "512K < Size <= 1M   %10" PRIu64"\n", pAewf->ReadSizesArr[READSIZE_1M]);
         fprintf (pFile, "       Size >  1M   %10" PRIu64"\n", pAewf->ReadSizesArr[READSIZE_ABOVE_1M]);

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

// -----------------------------------------------------------------------------
//  Legacy functions - Single threaded read function from former xmount version
// -----------------------------------------------------------------------------

// AewfReadChunkLegacy0 reads exactly one chunk. It expects the EWF table be present
// in memory and the required segment be opened.

static int AewfReadChunkLegacy0 (t_pAewf pAewf, t_pTable pTable, uint64_t AbsoluteChunk, unsigned TableChunk)
{
   int                  Compressed;
   uint64_t             SeekPos;
   t_pAewfSectionTable pEwfTable;
   unsigned int         Offset;
   unsigned int         ReadLen;
   uLongf               DstLen0;
   int                  zrc;
   uint                 CalcCRC;
   uint               *pStoredCRC;
   uint64_t             ChunkSize;
   int                  Ret = AEWF_OK;

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
   {
      LOG ("Chunk too big %u / %u", ReadLen, pAewf->ChunkBuffSize);
      return AEWF_CHUNK_TOO_BIG;
   }

   ChunkSize = pAewf->ChunkSize;
   if (AbsoluteChunk == (pAewf->Chunks-1))   // The very last chunk of the image may be smaller than the default
   {                                         // chunk size if the image isn't a multiple of the chunk size.
      ChunkSize = pAewf->ImageSize % pAewf->ChunkSize;
      if (ChunkSize == 0)
         ChunkSize = pAewf->ChunkSize;
   }

   if (Compressed)
   {
      CHK (ReadFilePos (pAewf, pTable->pSegment->pFile, pAewf->pChunkBuffCompressed, ReadLen, SeekPos))
      DstLen0 = pAewf->ChunkBuffSize;
      zrc = uncompress ((unsigned char*)pAewf->pChunkBuffUncompressed, &DstLen0, (const Bytef*)pAewf->pChunkBuffCompressed, ReadLen);
      if (zrc != Z_OK)
         Ret = AEWF_UNCOMPRESS_FAILED;
      if (DstLen0 != ChunkSize)
         Ret = AEWF_BAD_UNCOMPRESSED_LENGTH;
   }
   else
   {
      CHK (ReadFilePos (pAewf, pTable->pSegment->pFile, pAewf->pChunkBuffUncompressed, ReadLen, SeekPos))
      CalcCRC    =  adler32 (1, (const Bytef *) pAewf->pChunkBuffUncompressed, ChunkSize);
      pStoredCRC = (uint *) (pAewf->pChunkBuffUncompressed + ChunkSize);  //lint !e826 Suspicious pointer-to-pointer conversion (area too small)
      if (CalcCRC != *pStoredCRC)
         Ret = AEWF_CHUNK_CRC_ERROR;
   }

   pAewf->DataReadFromImage    += ReadLen;
   pAewf->DataReadFromImageRaw += ChunkSize;
   if (Ret == AEWF_OK)
   {
      pAewf->ChunkInBuff                  = AbsoluteChunk;
      pAewf->ChunkBuffUncompressedDataLen = ChunkSize;
   }
   else
   {
      pAewf->ChunkInBuff                  = AEWF_NONE;
      pAewf->ChunkBuffUncompressedDataLen = 0;
   }
   return Ret;
}

static int AewfReadChunkLegacy (t_pAewf pAewf, uint64_t AbsoluteChunk, char **ppBuffer, unsigned int *pLen)
{
   t_pTable  pTable;
   int        Found=FALSE;
   unsigned   TableChunk;
   unsigned   TableNr;

   *ppBuffer = pAewf->pChunkBuffUncompressed;
   *pLen     = 0;

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
      CHK (AEWF_CHUNK_NOT_FOUND)

   // Load corresponding table and get chunk
   // --------------------------------------
   pTable->LastUsed = time(NULL);                  //lint !e771 pTable' (line 640) conceivably not initialized
   pTable->pSegment->LastUsed = pTable->LastUsed;  // Update LastUsed here, in order not to remove the required data from cache

   CHK (AewfLoadEwfTable (pAewf, pTable))
   CHK (AewfOpenSegment  (pAewf, pTable));
   if ((AbsoluteChunk - pTable->ChunkFrom) > UINT_MAX)
      CHK (AEWF_ERROR_IN_CHUNK_NUMBER)
   TableChunk = AbsoluteChunk - pTable->ChunkFrom;
//   LOG ("table %d / entry %" PRIu64 " (%s)", TableNr, TableChunk, pTable->pSegment->pName)
   CHK (AewfReadChunkLegacy0 (pAewf, pTable, AbsoluteChunk, TableChunk))
   *pLen = pAewf->ChunkBuffUncompressedDataLen;

   return AEWF_OK;
}

static int AewfReadLegacy (t_pAewf pAewf, char *pBuf, uint64_t Seek64, size_t Count, size_t *pRead, int *pErrno)
{
   char         *pChunkBuffer;
   uint64_t       Chunk;
   uint64_t       Remaining;
   unsigned int   ChunkLen, Ofs, ToCopy;

   Ofs       = Seek64 % pAewf->ChunkSize;
   Chunk     = Seek64 / pAewf->ChunkSize;
   Remaining = Count;

   while (Remaining)
   {
      CHK (AewfReadChunkLegacy (pAewf, Chunk, &pChunkBuffer, &ChunkLen))
      if (ChunkLen == 0)
         return AEWF_CHUNK_LENGTH_ZERO;

      ToCopy = GETMIN (ChunkLen-Ofs, Remaining);
      memcpy (pBuf, pChunkBuffer+Ofs, ToCopy);
      Remaining -= ToCopy;
      pBuf      += ToCopy;
      *pRead    += ToCopy;
      Ofs        = 0;
      Chunk++;
   }

   return AEWF_OK;
}

// ------------------------------------------------------------------------------------
//  MT functions - Read function with multi-threaded decompression and CRC calculation
// ------------------------------------------------------------------------------------

// AewfThreadUncompress is run whenever compressed data chunk are encountered. It uncompresses
// the data and copies it to the correct destination.
static void* AewfThreadUncompress (void *pArg)
{
   t_pAewfThread pThread = (t_pAewfThread) pArg;
   uLongf         DstLen0;
   int            zrc;

   pThread->ReturnCode = AEWF_OK;
   DstLen0 = pThread->pAewf->ChunkBuffSize;
   zrc = uncompress ((unsigned char*)pThread->pChunkBuffUncompressed, &DstLen0,
                       (const Bytef*)pThread->pChunkBuffCompressed  ,
                                     pThread->ChunkBuffCompressedDataLen);
   if (zrc != Z_OK)
      pThread->ReturnCode = AEWF_UNCOMPRESS_FAILED;
   else if (DstLen0 != pThread->ChunkBuffUncompressedDataLen)
      pThread->ReturnCode = AEWF_BAD_UNCOMPRESSED_LENGTH;
   else
      memcpy (pThread->pBuf, pThread->pChunkBuffUncompressed+pThread->Ofs, pThread->Len);

   return NULL;
}

// AewfThreadCRC is called for uncompressed data chunks. It verifies the CRC and
// copies the data to the correct destination.
static void* AewfThreadCRC (void *pArg)
{
   t_pAewfThread  pThread = (t_pAewfThread) pArg;
   uint          *pStoredCRC;
   uint            CalcCRC;

   pThread->ReturnCode = AEWF_OK;
   CalcCRC    =  adler32 (1, (const Bytef *) pThread->pChunkBuffUncompressed, pThread->ChunkBuffUncompressedDataLen);
   pStoredCRC = (uint *) (pThread->pChunkBuffUncompressed + pThread->ChunkBuffUncompressedDataLen);  //lint !e826 Suspicious pointer-to-pointer conversion (area too small)
   if (CalcCRC != *pStoredCRC)
      pThread->ReturnCode = AEWF_CHUNK_CRC_ERROR;
   memcpy (pThread->pBuf, pThread->pChunkBuffUncompressed+pThread->Ofs, pThread->Len);

   return NULL;
}

// AewfThreadCopy is used in case of a cache hit. It simply copies the data from the
// uncompressed buffer to the destination.
static void* AewfThreadCopy (void *pArg)
{
   t_pAewfThread pThread = (t_pAewfThread) pArg;

   memcpy (pThread->pBuf, pThread->pChunkBuffUncompressed+pThread->Ofs, pThread->Len);
   pThread->ReturnCode = AEWF_OK;

   return NULL;
}


// AewfReadChunkMT0 reads exactly one chunk. It expects the EWF table be present
// in memory and the required segment be opened.
static int AewfReadChunkMT0 (t_pAewf pAewf, t_pTable pTable, uint64_t AbsoluteChunk, unsigned TableChunk, char *pBuf, unsigned int Ofs, unsigned int Len)
{
   int                  Compressed;
   uint64_t             SeekPos;
   t_pAewfSectionTable pEwfTable;
   unsigned int         Offset;
   unsigned int         ReadLen;
   int                  prc;
   uint64_t             ChunkSize;
   int                  Ret = AEWF_OK;

//   LOG ("Called - AbsoluteChunk=%'" PRIu64, AbsoluteChunk);

   pEwfTable = pTable->pEwfTable;
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
   {
      LOG ("Chunk too big %u / %u", ReadLen, pAewf->ChunkBuffSize);
      CHK (AEWF_CHUNK_TOO_BIG)
   }

   ChunkSize = pAewf->ChunkSize;
   if (AbsoluteChunk == (pAewf->Chunks-1))   // The very last chunk of the image may be smaller than the default
   {                                         // chunk size if the image isn't a multiple of the chunk size.
      ChunkSize = pAewf->ImageSize % pAewf->ChunkSize;
      if (ChunkSize == 0)
         ChunkSize = pAewf->ChunkSize;
   }

   for (int i=0; i<pAewf->Threads; i++)
   {
      t_pAewfThread pThread = &(pAewf->pThreadArr[i]);
      if (pThread->State == AEWF_IDLE)
      {
         pThread->State                        = AEWF_LAUNCHED;
         pThread->ChunkBuffCompressedDataLen   = ReadLen;
         pThread->ChunkBuffUncompressedDataLen = ChunkSize;  // uncompress should return this size (if it's a compressed chunk)
         pThread->ChunkInBuff                  = AbsoluteChunk;

         pThread->pBuf                         = pBuf; // These 3 parameters specify which part
         pThread->Ofs                          = Ofs;  // of the resulting chunk data should be
         pThread->Len                          = Len;  // copied to which location.
         if (Compressed)
         {
            CHK (ReadFilePos (pAewf, pTable->pSegment->pFile, pThread->pChunkBuffCompressed, ReadLen, SeekPos))
            prc = pthread_create (&pThread->ID, NULL, AewfThreadUncompress, pThread);
         }
         else
         {
            CHK (ReadFilePos (pAewf, pTable->pSegment->pFile, pThread->pChunkBuffUncompressed, ReadLen, SeekPos))
            prc = pthread_create (&pThread->ID, NULL, AewfThreadCRC, pThread);
         }
         if (prc != 0)
            Ret = AEWF_ERROR_PTHREAD;
         break;
      }
   }

   pAewf->DataReadFromImage    += ReadLen;
   pAewf->DataReadFromImageRaw += ChunkSize;

   return Ret;
}

static int AewfReadChunkMT (t_pAewf pAewf, uint64_t AbsoluteChunk, char *pBuf, unsigned int Ofs, unsigned int Len)
{
   t_pTable  pTable;
   int        Found=FALSE;
   unsigned   TableChunk;
   unsigned   TableNr;
   int        rc;

//   LOG ("Called - AbsoluteChunk=%'" PRIu64, AbsoluteChunk);

   // Check if chunk already is in cache
   // ----------------------------------
   for (int i=0; i<pAewf->Threads; i++)
   {
      t_pAewfThread pThread = &(pAewf->pThreadArr[i]);
      if (pThread->ChunkInBuff == AbsoluteChunk)
      {
         pThread->State = AEWF_LAUNCHED;
         pThread->pBuf  = pBuf;
         pThread->Ofs   = Ofs;
         pThread->Len   = Len;
         rc = pthread_create (&pThread->ID, NULL, AewfThreadCopy, pThread);
         if (rc != 0)
            return AEWF_ERROR_PTHREAD;
         pAewf->ChunkCacheHits++;

         return AEWF_OK;
      }
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
      CHK (AEWF_CHUNK_NOT_FOUND)

   // Load corresponding table and get chunk
   // --------------------------------------
   pTable->LastUsed = time(NULL);                  //lint !e771 pTable' (line 640) conceivably not initialized
   pTable->pSegment->LastUsed = pTable->LastUsed;  // Update LastUsed here, in order not to remove the required data from cache

   CHK (AewfLoadEwfTable (pAewf, pTable))
   CHK (AewfOpenSegment  (pAewf, pTable));
   if ((AbsoluteChunk - pTable->ChunkFrom) > UINT_MAX)
      CHK (AEWF_ERROR_IN_CHUNK_NUMBER)
   TableChunk = AbsoluteChunk - pTable->ChunkFrom;
//   LOG ("table %d / entry %" PRIu64 " (%s)", TableNr, TableChunk, pTable->pSegment->pName)
   CHK (AewfReadChunkMT0 (pAewf, pTable, AbsoluteChunk, TableChunk, pBuf, Ofs, Len))

   return AEWF_OK;
}

static int AewfReadMT0 (t_pAewf pAewf, char *pBuf, uint64_t Seek64, size_t Count, size_t *pRead, int *pErrno)
{
   uint64_t       AbsoluteChunk;
   uint64_t       Remaining;
   uint64_t       Len, Ofs;
   t_pAewfThread pThread;

   Ofs           = Seek64 % pAewf->ChunkSize;
   AbsoluteChunk = Seek64 / pAewf->ChunkSize;
   Remaining     = Count;
   *pRead        = 0;

   // Launch all read/decompress jobs
   // -------------------------------
   while (Remaining)
   {
      Len = GETMIN (pAewf->ChunkSize - Ofs, Remaining);
      CHK (AewfReadChunkMT (pAewf, AbsoluteChunk, pBuf, Ofs, Len))
      Remaining -= Len;
      pBuf      += Len;
      Ofs        = 0;
      AbsoluteChunk++;
   }

   // Wait for threads
   // ----------------
   for (int i=0; i<pAewf->Threads; i++)
   {
      pThread = &(pAewf->pThreadArr[i]);
//      LOG ("Checking thread %d -> %d", i, pThread->State);
      if (pThread->State == AEWF_LAUNCHED)
      {
         pthread_join (pThread->ID, NULL);
         pThread->State = AEWF_IDLE;
         CHK (pThread->ReturnCode)
         *pRead += pThread->Len;
      }
   }

   return AEWF_OK;
}

static int AewfReadMT (t_pAewf pAewf, char *pBuf, uint64_t Seek64, size_t Count, size_t *pRead, int *pErrno)
{
   uint64_t ToRead;
   uint64_t MaxPerLoop;
   size_t   Read;

   MaxPerLoop = pAewf->Threads * pAewf->ChunkSize;
   while (Count)
   {
      ToRead = GETMIN (MaxPerLoop, Count);
      Read   = 0;
      CHK (AewfReadMT0 (pAewf, pBuf, Seek64, ToRead, &Read, pErrno))
      *pRead += Read;
      pBuf   += Read;
      Seek64 += Read;
      Count  -= Read;
   }

   return AEWF_OK;
}


// ---------------
//  API functions
// ---------------

static int AewfCreateHandle (void **ppHandle, const char *pFormat, uint8_t Debug)
{
   t_pAewf pAewf;

   *ppHandle = NULL;

   // Create handle and clear it
   // --------------------------
   pAewf = (t_pAewf) malloc (sizeof(t_Aewf));
   if (pAewf == NULL)
      return AEWF_MEMALLOC_FAILED;

   memset(pAewf,0,sizeof(t_Aewf));
   pAewf->ChunkInBuff           = AEWF_NONE;
   pAewf->pErrorText            = NULL;
   pAewf->StatsRefresh          = 10;
   pAewf->SegmentCacheHits      = 0;
   pAewf->SegmentCacheMisses    = 0;
   pAewf->TableCacheHits        = 0;
   pAewf->TableCacheMisses      = 0;
   pAewf->ChunkCacheHits        = 0;
   pAewf->ChunkCacheMisses      = 0;
   pAewf->ReadOperations        = 0;
   pAewf->DataReadFromImage     = 0;
   pAewf->DataReadFromImageRaw  = 0;
   pAewf->DataRequestedByCaller = 0;
   pAewf->TablesReadFromImage   = 0;
   pAewf->ChunksRead            = 0;
   pAewf->BytesRead             = 0;
   memset (pAewf->ReadSizesArr, 0, sizeof (pAewf->ReadSizesArr));
   pAewf->Errors                = 0;
   pAewf->LastError             = AEWF_OK;
   pAewf->MaxTableCache         = 0;
   pAewf->MaxOpenSegments       = 0;
   pAewf->pStatsPath            = NULL;
   pAewf->StatsRefresh          = 0;
   pAewf->pLogPath              = NULL;
   pAewf->LogStdout             = Debug;
   pAewf->pThreadArr            = NULL;

   pAewf->MaxTableCache   = AEWF_DEFAULT_TABLECACHE * 1024*1024;
   pAewf->MaxOpenSegments = AEWF_DEFAULT_MAXOPENSEGMENTS;
   pAewf->StatsRefresh    = AEWF_DEFAULT_STATSREFRESH;
   pAewf->Threads         = AEWF_DEFAULT_THREADS;

   *ppHandle = (void*) pAewf;

   return AEWF_OK;
}

int AewfDestroyHandle(void **ppHandle)
{
   t_pAewf pAewf = (t_pAewf) *ppHandle;

   LOG ("Called");
   LOG ("Remark: 'Ret' won't be logged"); // Handle gets destroyed, 'ret' logging not possible

   if (pAewf->pLogPath  )   free(pAewf->pLogPath  );
   if (pAewf->pStatsPath)   free(pAewf->pStatsPath);

   memset (pAewf, 0, sizeof(t_Aewf));
   free (pAewf);
   *ppHandle = NULL;

   return AEWF_OK;
}

int AewfOpen (void *pHandle, const char **ppFilenameArr, uint64_t FilenameArrLen)
{
   t_pAewf                 pAewf = (t_pAewf) pHandle;
   t_AewfFileHeader         FileHeader;
   t_AewfSection            Section;
   FILE                   *pFile;
   t_pSegment              pSegment;
   t_pSegment              pPrevSegment;
   t_pTable                pTable;
   uint64_t                 Pos;
   t_pAewfSectionTable     pEwfTable   = NULL;
   t_pAewfSectionVolume    pVolume     = NULL;
   t_pAewfSectionHash      pMD5        = NULL;
   char                   *pHeader     = NULL;
   char                   *pHeader2    = NULL;
   int                      LastSection;
   unsigned int             SectionSectorsSize;
   unsigned                 HeaderLen  = 0;
   unsigned                 Header2Len = 0;

   LOG ("Called - Files=%" PRIu64, FilenameArrLen);

   // Create pSegmentArr and put the segment files in it
   // --------------------------------------------------
   int SegmentArrLen  = FilenameArrLen * sizeof(t_Segment);
   pAewf->pSegmentArr = (t_pSegment) malloc (SegmentArrLen);
   pAewf->Segments    = FilenameArrLen;
   if (pAewf->pSegmentArr == NULL)
      return AEWF_MEMALLOC_FAILED;
   memset (pAewf->pSegmentArr, 0, SegmentArrLen);

   for (unsigned i=0; i<FilenameArrLen; i++)
   {
      pSegment = &pAewf->pSegmentArr[i];
      pSegment->pName = realpath (ppFilenameArr[i], NULL); // realpath allocates a buffer of the necessary length

      LOG ("Opening segment %s", ppFilenameArr[i]);
      CHK (OpenFile (&pFile, pSegment->pName))
      CHK (ReadFilePos (pAewf, pFile, (void*)&FileHeader, sizeof(FileHeader), 0))

      pSegment->Number   = FileHeader.SegmentNumber;
      pSegment->LastUsed = 0;
      pSegment->pFile    = NULL;

      CHK (CloseFile (&pFile))
   }

   // Put segment array into correct sequence and check if segment numbers are correct
   // --------------------------------------------------------------------------------
   qsort (pAewf->pSegmentArr, pAewf->Segments, sizeof (t_Segment), &QsortCompareSegments);
   pPrevSegment = NULL;
   for (unsigned i=0; i<pAewf->Segments; i++)
   {
      pSegment = &(pAewf->pSegmentArr[i]);
      if (pPrevSegment)
      {
         if (pSegment->Number == pPrevSegment->Number)
         {
            LOG ("Error: Duplicate segment numbers");
            LOG ("Segment files %s and %s have both segment number %u", pPrevSegment->pName, pSegment->pName, pSegment->Number);
            return AEWF_DUPLICATE_SEGMENT_NUMBER;
         }
      }
      if (pSegment->Number != (i+1))
      {
         LOG ("Error: Missing segment number(s)");
         LOG ("Previous  segment file %s has segment number %u", pPrevSegment->pName, pPrevSegment->Number);
         LOG ("Following segment file %s has segment number %u", pSegment->pName    , pSegment->Number    );
         return AEWF_MISSING_SEGMENT_NUMBER;
      }
      pPrevSegment = pSegment;
   }

   // Find all tables in the segment files
   // ------------------------------------
   pAewf->pTableArr      = NULL;
   pAewf->Tables         = 0;
   pAewf->Chunks         = 0;
   pAewf->TotalTableSize = 0;
   SectionSectorsSize    = 0;

   LOG ("Reading tables");
   for (unsigned i=0; i<pAewf->Segments; i++)
   {
      pSegment = &pAewf->pSegmentArr[i];
      CHK (OpenFile (&pFile, pSegment->pName))
      CHK (ReadFilePos (pAewf, pFile, &FileHeader, sizeof(FileHeader), 0))
      Pos = sizeof (FileHeader);
      LOG ("Segment %s ", pSegment->pName);
      // Search for the important sections
      do
      {
         CHK (ReadFilePos (pAewf, pFile, &Section, sizeof (t_AewfSection), Pos))

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
            CHK (ReadFileAlloc (pAewf, pFile, (void**) &pEwfTable, sizeof(t_AewfSectionTable))) // No need to read the actual offset table
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
            pAewf->TotalTableSize     += pTable->Size;
            pAewf->Chunks             += pTable->ChunkCount;
            pTable->ChunkTo            = pAewf->Chunks-1;
            free (pEwfTable);
            pEwfTable = NULL;
            SectionSectorsSize = 0;
         }
         else if ((strcasecmp ((char *)Section.Type, "header") == 0) && (pHeader==NULL))
         {
            HeaderLen = Section.Size - sizeof(t_AewfSection);
            CHK (ReadFileAlloc (pAewf, pFile, (void**) &pHeader, HeaderLen))
         }
         else if ((strcasecmp ((char *)Section.Type, "header2") == 0) && (pHeader2==NULL))
         {
            Header2Len = Section.Size - sizeof(t_AewfSection);
            CHK (ReadFileAlloc (pAewf, pFile, (void**) &pHeader2, Header2Len))
         }
         else if ( ((strcasecmp ((char *)Section.Type, "volume") == 0) || // Guymager works with the volume section. Others use different names
                    (strcasecmp ((char *)Section.Type, "disk"  ) == 0) || // for it, but it all is the same. See Joachim Metz' EWF documentation
                    (strcasecmp ((char *)Section.Type, "data"  ) == 0))
                 && (pVolume==NULL))
         {
            CHK (ReadFileAlloc (pAewf, pFile, (void**) &pVolume, sizeof(t_AewfSectionVolume)))
            pAewf->Sectors    = pVolume->SectorCount;
            pAewf->SectorSize = pVolume->BytesPerSector;
            pAewf->ChunkSize  = pVolume->SectorsPerChunk * pVolume->BytesPerSector; //lint !e647 Suspicious truncation
            pAewf->ImageSize  = pAewf->Sectors * pAewf->SectorSize;
         }
         if (strcasecmp ((char *)Section.Type, "hash") == 0)
         {
            CHK (ReadFileAlloc (pAewf, pFile, (void**) &pMD5, sizeof(t_AewfSectionHash)))
         }
//         LOG ("Section %s", Section.Type)

         LastSection = (Pos == Section.OffsetNextSection);
         Pos = Section.OffsetNextSection;
      } while (!LastSection);
      CHK (CloseFile (&pFile))
   }
   if (pVolume == NULL)
      return AEWF_VOLUME_MISSING;

   if (pAewf->Chunks != pVolume->ChunkCount)
   {
      LOG ("Error: Wrong chunk count: %"PRIu64" / %"PRIu64, pAewf->Chunks, pVolume->ChunkCount);
      LOG ("Maybe some segment files are missing. Perhaps you specified E01 instead of E?? or the segments continue beyond extension .EZZ.");
      return AEWF_WRONG_CHUNK_COUNT;
   }
   pAewf->ChunkBuffSize = pAewf->ChunkSize + 4096; // reserve some extra space (for CRC and as compressed data might be slightly larger than uncompressed data with some imagers)
   pAewf->pChunkBuffCompressed   = (char *) malloc (pAewf->ChunkBuffSize);
   pAewf->pChunkBuffUncompressed = (char *) malloc (pAewf->ChunkBuffSize);
   if ((pAewf->pChunkBuffCompressed   == NULL) ||
       (pAewf->pChunkBuffUncompressed == NULL))
      return AEWF_MEMALLOC_FAILED;

   pAewf->TableCache      = 0;
   pAewf->OpenSegments    = 0;

   CHK (CreateInfoData (pAewf, pVolume, pHeader, HeaderLen, pHeader2, Header2Len, pMD5))
   free (pVolume);
   free (pHeader);
   free (pHeader2);

   // Allocate thread structures
   // --------------------------
   if (pAewf->Threads > 1)
   {
      pAewf->pThreadArr = (t_pAewfThread) malloc (pAewf->Threads * sizeof (t_AewfThread));
      for (int i=0; i<pAewf->Threads; i++)
      {
         t_pAewfThread pThread = &pAewf->pThreadArr[i];
         memset (pThread, 0, sizeof(t_AewfThread));
         pThread->pAewf                  = pAewf;
         pThread->pChunkBuffCompressed   = (char *) malloc (pAewf->ChunkBuffSize);
         pThread->pChunkBuffUncompressed = (char *) malloc (pAewf->ChunkBuffSize);
         pThread->ChunkInBuff            = AEWF_NONE;
         pThread->State                  = AEWF_IDLE;
      }
   }

   LOG ("Ret");
   return AEWF_OK;
}

static int AewfClose (void *pHandle)
{
   t_pAewf    pAewf = (t_pAewf) pHandle;
   t_pTable   pTable;
   t_pSegment pSegment;

   LOG ("Called");
   CHK (UpdateStats (pAewf,TRUE))

   for (unsigned i=0; i<pAewf->Tables; i++)
   {
      pTable = &pAewf->pTableArr[i];
      if (pTable->pEwfTable)
         free (pTable->pEwfTable);
   }

   for (unsigned i=0;i<pAewf->Segments;i++)
   {
      pSegment = &pAewf->pSegmentArr[i];
      if (pSegment->pFile)
         CHK (CloseFile (&pSegment->pFile));
      free (pSegment->pName);
   }

   free (pAewf->pTableArr);
   free (pAewf->pSegmentArr);
   free (pAewf->pChunkBuffCompressed);
   free (pAewf->pChunkBuffUncompressed);

   // Free thread structures
   // ----------------------
   if (pAewf->pThreadArr)
   {
      for (int i=0; i<pAewf->Threads; i++)
      {
         t_pAewfThread pThread = &pAewf->pThreadArr[i];
         free (pThread->pChunkBuffCompressed);
         free (pThread->pChunkBuffUncompressed);
      }
      free (pAewf->pThreadArr);
      pAewf->pThreadArr = NULL;
   }

   LOG ("Ret");
   return AEWF_OK;
}

static int AewfSize (void *pHandle, uint64_t *pSize)
{
   t_pAewf pAewf = (t_pAewf) pHandle;

   LOG ("Called");
   *pSize = pAewf->ImageSize;

   LOG ("Ret - Size=%" PRIu64, *pSize);
   return AEWF_OK;
}

static int AewfRead (void *pHandle, char *pBuf, off_t Seek, size_t Count, size_t *pRead, int *pErrno)
{
   t_pAewf       pAewf = (t_pAewf) pHandle;
   uint64_t       Seek64;
   int            Ret = AEWF_OK;

   LOG ("Called - Seek=%'" PRIu64 ",Count=%'" PRIu64, Seek, Count);
   *pRead  = 0;
   *pErrno = 0;

   if (Seek < 0)
   {
      Ret = AEWF_NEGATIVE_SEEK;
      goto Leave;
   }
   Seek64 = Seek;

   pAewf->ReadOperations++;
   pAewf->DataRequestedByCaller+=Count;
   if       (Count <=   32*1024) pAewf->ReadSizesArr[READSIZE_32K]++;
   else  if (Count <=   64*1024) pAewf->ReadSizesArr[READSIZE_64K]++;
   else  if (Count <=  128*1024) pAewf->ReadSizesArr[READSIZE_128K]++;
   else  if (Count <=  256*1024) pAewf->ReadSizesArr[READSIZE_256K]++;
   else  if (Count <=  512*1024) pAewf->ReadSizesArr[READSIZE_512K]++;
   else  if (Count <= 1024*1024) pAewf->ReadSizesArr[READSIZE_1M]++;
   else                          pAewf->ReadSizesArr[READSIZE_ABOVE_1M]++;

   if (Seek64 >= pAewf->ImageSize)        // If calling function asks
      goto Leave;                         // for data beyond end of
   if ((Seek64+Count) > pAewf->ImageSize) // image simply return what
      Count = pAewf->ImageSize - Seek64;  // is possible.

   if (pAewf->Threads == 1)
        Ret = AewfReadLegacy (pAewf, pBuf, Seek64, Count, pRead, pErrno);
   else Ret = AewfReadMT     (pAewf, pBuf, Seek64, Count, pRead, pErrno);

Leave:
   AewfCheckError (pAewf, Ret, pErrno);
   CHK (UpdateStats (pAewf, (Ret != AEWF_OK)))
   LOG ("Ret %d - Read=%" PRIu32, Ret, *pRead);
   return Ret;
}

static int AewfOptionsHelp (const char **ppHelp)
{
   char *pHelp=NULL;
   int    wr;

   wr = asprintf (&pHelp, "    %-12s : Maximum amount of RAM cache, in MiB, for image offset tables. Default: %"PRIu64" MiB\n"
                          "    %-12s : Maximum number of concurrently opened image segment files. Default: %"PRIu64"\n"
                          "    %-12s : Output statistics at regular intervals to this directory (must exist).\n"
                          "                   The files created in this directory will be named stats_<pid>.\n"
                          "    %-12s : The update interval, in seconds, for the statistics (%s must be set). Default: %"PRIu64"s.\n"
                          "    %-12s : Path for writing log file (must exist).\n"
                          "                   The files created in this directory will be named log_<pid>.\n"
                          "    %-12s : Max. number of threads for parallelized decompression. Default: %"PRIu64"\n"
                          "                   A value of 1 switches back to old, single-threaded legacy functions.\n",
                          AEWF_OPTION_TABLECACHE,      AEWF_DEFAULT_TABLECACHE,
                          AEWF_OPTION_MAXOPENSEGMENTS, AEWF_DEFAULT_MAXOPENSEGMENTS,
                          AEWF_OPTION_STATS,
                          AEWF_OPTION_STATSREFRESH, AEWF_OPTION_STATS, AEWF_DEFAULT_STATSREFRESH,
                          AEWF_OPTION_LOG,
                          AEWF_OPTION_THREADS, AEWF_DEFAULT_THREADS);
   if ((pHelp == NULL) || (wr<=0))
      return AEWF_MEMALLOC_FAILED;

   *ppHelp = pHelp;
   return AEWF_OK;
}

static int AewfOptionsParse (void *pHandle, uint32_t OptionCount, const pts_LibXmountOptions *ppOptions, const char **ppError)
{
   pts_LibXmountOptions pOption;
   t_pAewf              pAewf  = (t_pAewf) pHandle;
   const char          *pError = NULL;
   int                   rc    = AEWF_OK;
   int                   Ok;

   LOG ("Called - OptionCount=%" PRIu32, OptionCount);
   *ppError = NULL;

   #define TEST_OPTION_UINT64(Opt,DestField)                      \
      if (strcmp (pOption->p_key, Opt) == 0)                      \
      {                                                           \
         pAewf->DestField = StrToUint64 (pOption->p_value, &Ok);  \
         if (!Ok)                                                 \
         {                                                        \
            pError = "Error in option %s: Invalid value";         \
            break;                                                \
         }                                                        \
         LOG ("Option %s set to %" PRIu64, Opt, pAewf->DestField) \
      }

   for (uint32_t i=0; i<OptionCount; i++)
   {
      pOption = ppOptions[i];
      if (strcmp (pOption->p_key, AEWF_OPTION_LOG) == 0)
      {
         pAewf->pLogPath = realpath (pOption->p_value, NULL);
         if (pAewf->pLogPath == NULL)
         {
            pError = "The given log path does not exist";
            LOG ("Log path %s not found", pOption->p_value);
            break;
         }
         rc = LOG ("Logging for libxmount_input_aewf started")
         if (rc != AEWF_OK)
         {
            pError = "Write test to log file failed";
            break;
         }
         pOption->valid = TRUE;
         LOG ("Option %s set to %s (full path %s)", AEWF_OPTION_LOG, pOption->p_value, pAewf->pLogPath);
      }
      if (strcmp (pOption->p_key, AEWF_OPTION_STATS) == 0)
      {
         pAewf->pStatsPath = realpath (pOption->p_value, NULL);
         if (pAewf->pStatsPath == NULL)
         {
            pError = "The given stats path does not exist";
            LOG ("Stats path %s not found", pOption->p_value);
            break;
         }
         pOption->valid = TRUE;
         LOG ("Option %s set to %s (full path %s)", AEWF_OPTION_STATS, pOption->p_value, pAewf->pLogPath);
      }

      else TEST_OPTION_UINT64 (AEWF_OPTION_MAXOPENSEGMENTS, MaxOpenSegments)
      else TEST_OPTION_UINT64 (AEWF_OPTION_TABLECACHE     , MaxTableCache)
      else TEST_OPTION_UINT64 (AEWF_OPTION_STATSREFRESH   , StatsRefresh)
      else TEST_OPTION_UINT64 (AEWF_OPTION_THREADS        , Threads)
   }
   #undef TEST_OPTION_UINT64

   if (pError)
   {
      *ppError = strdup (pError);
      rc = AEWF_OPTIONS_ERROR;
   }
   LOG ("Ret - rc=%d, error=%s", rc, *ppError);
   return rc;
}

static int AewfGetInfofileContent (void *pHandle, const char **ppInfoBuf)
{
   t_pAewf  pAewf  = (t_pAewf) pHandle;
   char    *pInfo;

   LOG ("Called");
   pInfo = strdup (pAewf->pInfo);
   if (pInfo == NULL)
      return AEWF_MEMALLOC_FAILED;
   *ppInfoBuf = pInfo;

   LOG ("Ret - %d bytes of info", strlen(pInfo)+1);
   return AEWF_OK;
}

static const char* AewfGetErrorMessage (int ErrNum)
{
   const char *pMsg;
   #define ADD_ERR(AewfErrCode)              \
      case AewfErrCode: pMsg = #AewfErrCode; \
      break;

   switch (ErrNum)
   {
      ADD_ERR (AEWF_OK)
      ADD_ERR (AEWF_MEMALLOC_FAILED)
      ADD_ERR (AEWF_READ_BEYOND_END_OF_IMAGE)
      ADD_ERR (AEWF_OPTIONS_ERROR)
      ADD_ERR (AEWF_CANNOT_OPEN_LOGFILE)
      ADD_ERR (AEWF_FILE_OPEN_FAILED)
      ADD_ERR (AEWF_FILE_CLOSE_FAILED)
      ADD_ERR (AEWF_FILE_SEEK_FAILED)
      ADD_ERR (AEWF_FILE_READ_FAILED)
      ADD_ERR (AEWF_READFILE_BAD_MEM)
//      ADD_ERR (AEWF_MISSING_SEGMENT_NUMBER)
//      ADD_ERR (AEWF_DUPLICATE_SEGMENT_NUMBER)
      case AEWF_MISSING_SEGMENT_NUMBER:
         pMsg = "Missing segment number. The list of EWF segment files is incomplete. One or "
                "more segment numbers are missing.";
         break;
      case AEWF_DUPLICATE_SEGMENT_NUMBER:
         pMsg = "Duplicate segment number. The list of EWF segment files contains duplicate segment "
                "numbers. Maybe you accidentally specified the segment files of more than just one EWF image.";
         break;
      ADD_ERR (AEWF_WRONG_SEGMENT_FILE_COUNT)
      ADD_ERR (AEWF_VOLUME_MUST_PRECEDE_TABLES)
      ADD_ERR (AEWF_SECTORS_MUST_PRECEDE_TABLES)
//      ADD_ERR (AEWF_WRONG_CHUNK_COUNT)
      case AEWF_WRONG_CHUNK_COUNT:
         pMsg = "Wrong chunk count. Some segment files seem to be missing. Perhaps you specified .E01 "
                "instead of .E?? or the segment files continue beyond extension .EZZ.";
         break;
      ADD_ERR (AEWF_CHUNK_NOT_FOUND)
      ADD_ERR (AEWF_VOLUME_MISSING)
      ADD_ERR (AEWF_ERROR_EWF_TABLE_NOT_READY)
      ADD_ERR (AEWF_ERROR_EWF_SEGMENT_NOT_READY)
      ADD_ERR (AEWF_CHUNK_TOO_BIG)
      ADD_ERR (AEWF_UNCOMPRESS_FAILED)
      ADD_ERR (AEWF_BAD_UNCOMPRESSED_LENGTH)
      ADD_ERR (AEWF_CHUNK_CRC_ERROR)
      ADD_ERR (AEWF_ERROR_IN_CHUNK_NUMBER)
      ADD_ERR (AEWF_VASPRINTF_FAILED)
      ADD_ERR (AEWF_UNCOMPRESS_HEADER_FAILED)
      ADD_ERR (AEWF_ASPRINTF_FAILED)
      ADD_ERR (AEWF_CHUNK_LENGTH_ZERO)
      ADD_ERR (AEWF_NEGATIVE_SEEK)
      ADD_ERR (AEWF_ERROR_EIO_END)
      ADD_ERR (AEWF_ERROR_PTHREAD)
      ADD_ERR (AEWF_WRONG_CHUNK_CALCULATION)

      default:
         pMsg = "Unknown error";
   }
   #undef ARR_ERR
   return pMsg;
}

static int AewfFreeBuffer (void *pBuf)
{
   free (pBuf);

   return AEWF_OK;
}


// ------------------------------------
//  LibXmount_Input API implementation
// ------------------------------------

uint8_t LibXmount_Input_GetApiVersion ()
{
   return LIBXMOUNT_INPUT_API_VERSION;
}

const char* LibXmount_Input_GetSupportedFormats ()
{
  return "aewf\0\0"; //lint !e840 Use of nul character in a string literal
}

void LibXmount_Input_GetFunctions (ts_LibXmountInputFunctions *pFunctions)
{
   pFunctions->CreateHandle       = &AewfCreateHandle;
   pFunctions->DestroyHandle      = &AewfDestroyHandle;
   pFunctions->Open               = &AewfOpen;
   pFunctions->Close              = &AewfClose;
   pFunctions->Size               = &AewfSize;
   pFunctions->Read               = &AewfRead;
   pFunctions->OptionsHelp        = &AewfOptionsHelp;
   pFunctions->OptionsParse       = &AewfOptionsParse;
   pFunctions->GetInfofileContent = &AewfGetInfofileContent;
   pFunctions->GetErrorMessage    = &AewfGetErrorMessage;
   pFunctions->FreeBuffer         = &AewfFreeBuffer;
}

// -----------------------------------------------------
//            Small main routine for testing
//            It converts an EWF file into dd
// -----------------------------------------------------

#ifdef AEWF_STANDALONE

#define PRINT_ERROR_AND_EXIT(...) \
{                                 \
   printf (__VA_ARGS__);          \
   exit (1);                      \
}

int ParseOptions (t_pAewf pAewf, char *pOptions)
{
   pts_LibXmountOptions   pOptionArr;
   pts_LibXmountOptions *ppOptionArr;
   int                     OptionCount;
   char                  *pSep;
   char                  *pEqual;
   char                  *pTmp;
   const char            *pError;
   char                  *pOpt;
   int                     rc;

   if (pOptions == NULL)
      return AEWF_OK;

   if (*pOptions == '\0')
      return AEWF_OK;

   if (*pOptions == ',')
      return AEWF_OPTIONS_ERROR;

   if (pOptions[strlen(pOptions)-1] == ',')
      return AEWF_OPTIONS_ERROR;

   pOpt = strdup (pOptions);

   // Count number of comma separated options
   OptionCount = 1;
   pTmp        = pOpt;
   while ((pTmp = strchr (pTmp, ',')) != NULL)
   {
      OptionCount++;
      pTmp++;
   }

   // Create and fill option array
   pOptionArr  = (pts_LibXmountOptions) malloc (OptionCount * sizeof(ts_LibXmountOptions));
   if (pOptionArr == NULL)
      PRINT_ERROR_AND_EXIT ("Cannot allocate pOptionArr");
   memset (pOptionArr, 0, OptionCount * sizeof(ts_LibXmountOptions));
   pTmp = pOpt;
   for (int i=0; i<OptionCount; i++)
   {
      pOptionArr[i].p_key = pTmp;
      pSep = strchr (pTmp, ',');
      if (pSep)
         *pSep ='\0';
      pEqual = strchr (pTmp, '=');
      if (pEqual)
      {
         *pEqual = '\0';
         pOptionArr[i].p_value = pEqual+1;
      }
      if (pSep != NULL)
         pTmp = pSep+1;
   }

   // Create pointer array and call AEWF parse function
   ppOptionArr = (pts_LibXmountOptions *)malloc (OptionCount*sizeof (pts_LibXmountOptions));
   if (ppOptionArr == NULL)
      PRINT_ERROR_AND_EXIT ("Cannot allocate ppOptionArr");
   for (int i=0; i<OptionCount; i++)
      ppOptionArr[i] = &pOptionArr[i];

   rc = AewfOptionsParse ((void*) pAewf, OptionCount, ppOptionArr, &pError);
   free (ppOptionArr);
   free ( pOptionArr);
   free ( pOpt);
   if (pError)
      PRINT_ERROR_AND_EXIT ("Error while setting options: %s", pError);
   CHK (rc)

   return AEWF_OK;
}

int main(int argc, const char *argv[])
{
   t_pAewf       pAewf=NULL;
   uint64_t       TotalSize;
   uint64_t       Remaining;
   uint64_t       ToRead;
   uint64_t       Read;
   uint64_t       Pos;
   unsigned int   BuffSize = 13*65536; // A multiple of chunk size for good performance
   char           Buff[BuffSize];
   FILE         *pFile;
   int            Percent;
   int            PercentOld;
   int            rc;
   int            Errno;
   char         *pOptions = NULL;
   const char   *pHelp;
   const char   *pInfoBuff;

   #ifdef CREATE_REVERSE_FILE
      FILE      *pFileRev;
      uint64_t    PosRev;
      #ifdef REVERSE_FILE_USES_SEPARATE_HANDLE
         t_pAewf pAewfRev;
      #else
         #define pAewfRev pAewf
      #endif
   #endif

   setbuf(stdout, NULL);
   setbuf(stderr, NULL);
   (void) setlocale (LC_ALL, "");

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
      (void) AewfOptionsHelp (&pHelp);
      printf ("Usage: %s <EWF segment file 1> <EWF segment file 2> <...> [-comma_separated_options]\n", argv[0]);
      printf ("Possible options:\n%s\n", pHelp);
      printf ("The output file will be named dd.\n");
      CHK (AewfFreeBuffer ((void*) pHelp))
      exit (1);
   }

   if (argv[argc-1][0] == '-')
   {
      pOptions = strdup (&(argv[argc-1][1]));
      argc--;
   }
   rc = AewfCreateHandle ((void**) &pAewf, "aewf", LOG_STDOUT);
   if (rc != AEWF_OK)
      PRINT_ERROR_AND_EXIT ("Cannot create handle, rc=%d\n", rc)

   if (pOptions)
      CHK (ParseOptions(pAewf, pOptions))
   rc = AewfOpen (pAewf, &argv[1], argc-1);
   if (rc != AEWF_OK)
      PRINT_ERROR_AND_EXIT ("Cannot open EWF files, rc=%d\n", rc)

   #if defined(CREATE_REVERSE_FILE) && defined(REVERSE_FILE_USES_SEPARATE_HANDLE)
      rc = AewfCreateHandle ((void**) &pAewfRev, "aewf", LOG_STDOUT);
      if (rc != AEWF_OK)
         PRINT_ERROR_AND_EXIT ("Cannot create reverse handle, rc=%d\n", rc)
      if (pOptions)
         CHK (ParseOptions (pAewfRev, pOptions))
      rc = AewfOpen (pAewfRev, &argv[1], argc-1);
      if (rc != AEWF_OK)
         PRINT_ERROR_AND_EXIT ("Cannot open EWF files, rc=%d\n", rc)
   #endif

   CHK (AewfGetInfofileContent ((void*) pAewf, &pInfoBuff))
   if (pInfoBuff)
      printf ("Contents of info buffer:\n%s\n", pInfoBuff);
   CHK (AewfFreeBuffer ((void*) pInfoBuff))

   CHK (AewfSize (pAewf, &TotalSize))
   printf ("Total size: %" PRIu64 " bytes\n", TotalSize);

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
   Errno      = 0;
   while (Remaining)
   {
//      LOG ("Pos %" PRIu64 " -- Remaining %" PRIu64 " ", Pos, Remaining);
      ToRead = GETMIN (Remaining, BuffSize);

     rc = AewfRead ((void*) pAewf, &Buff[0], Pos, ToRead, &Read, &Errno);
      if ((rc != AEWF_OK) || (Errno != 0))
         PRINT_ERROR_AND_EXIT("Error %d while calling AewfRead (Errno %d)\n", rc, Errno);
      if (Read != ToRead)
         PRINT_ERROR_AND_EXIT("Only %" PRIu64 " out of %" PRIu64 " bytes read\n", Read, ToRead);

      if (fwrite (Buff, Read, 1, pFile) != 1)
         PRINT_ERROR_AND_EXIT("Could not write to destination file\n");

      Remaining -= ToRead;
      Pos       += ToRead;

      #ifdef CREATE_REVERSE_FILE
         PosRev -= ToRead;
         rc = AewfRead ((void*) pAewfRev, &Buff[0], PosRev, ToRead, &Read, &Errno);
         if ((rc != AEWF_OK) || (Errno != 0))
            PRINT_ERROR_AND_EXIT("Error %d while reverse calling AewfRead (Errno %d)\n", rc, Errno);
         if (Read != ToRead)
            PRINT_ERROR_AND_EXIT("Only %" PRIu64 " out of %" PRIu64 " bytes read from rev file\n", Read, ToRead);

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
   if (AewfClose (pAewf))
      PRINT_ERROR_AND_EXIT("Error while closing AEWF files\n");
   if (AewfDestroyHandle ((void**)&pAewf))
      PRINT_ERROR_AND_EXIT("Error while destroying AEWF handle\n");
   if (fclose (pFile))
      PRINT_ERROR_AND_EXIT ("Error while closing destination file\n");

   #ifdef CREATE_REVERSE_FILE
      #ifdef REVERSE_FILE_USES_SEPARATE_HANDLE
         if (AewfClose (pAewfRev))
            PRINT_ERROR_AND_EXIT("Error while closing reverse AEWF files\n");
         if (AewfDestroyHandle ((void**)&pAewfRev))
            PRINT_ERROR_AND_EXIT("Error while destroying reverse AEWF handle\n");
      #endif
      if (fclose (pFileRev))
         PRINT_ERROR_AND_EXIT ("Error while closing reverse destination file\n");
   #endif

   printf ("\n");
   return 0;
}

#endif

