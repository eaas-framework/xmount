/*******************************************************************************
* xmount Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing simple AFF images created by Guymager.                             *
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

// Please don't touch source code formatting!

#ifdef LINTING
//   #define _LARGEFILE_SOURCE
//   #define _FILE_OFFSET_BITS 64
   #define AAFF_STANDALONE
#endif

#ifdef AAFF_STANDALONE
   #define LOG_STDOUT TRUE
#endif

#include <netinet/in.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include <limits.h>
#include <zlib.h>
#include <time.h>
#include <errno.h>

#include "../libxmount_input.h"

#include "libxmount_input_aaff.h"

static int         AaffClose           (void *pHandle);
static const char* AaffGetErrorMessage (int ErrNum);

#define AAFF_OPTION_MAXPAGEARRMEM   "aaffmaxmem"
#define AAFF_OPTION_LOG             "aafflog"

// ----------------------------
//  Logging and error handling
// ----------------------------

#define LOG_HEADER_LEN 80

int LogvEntry (const char *pLogFileName, uint8_t LogStdout, const char *pFileName, const char *pFunctionName, int LineNr, const char *pFormat, va_list pArguments)
{
   time_t       NowT;
   struct tm  *pNowTM;
   FILE       *pFile;
   int          wr;
   char       *pFullLogFileName = NULL;
   const char *pBase;
   char         LogLineHeader[1024];
   pid_t        OwnPID;

   if (!LogStdout && (pLogFileName==NULL))
      return AAFF_OK;

   time (&NowT);
   pNowTM = localtime (&NowT);
   OwnPID = getpid();  // pthread_self()
   wr  = (int) strftime (&LogLineHeader[0] , sizeof(LogLineHeader)   , "%a %d.%b.%Y %H:%M:%S ", pNowTM); //lint !e713
   wr += snprintf (&LogLineHeader[wr], sizeof(LogLineHeader)-wr, "%5d ", OwnPID);                  //lint !e737

   if (pFileName && pFunctionName)
   {
      pBase = strrchr(pFileName, '/');
      if (pBase)
         pFileName = pBase+1;
      wr += snprintf (&LogLineHeader[wr], sizeof(LogLineHeader)-wr, "%s %s %d ", pFileName, pFunctionName, LineNr); //lint !e737
   }

//   while (wr < LOG_HEADER_LEN)
//      LogLineHeader[wr++] = ' ';

   if (pLogFileName)
   {
      wr = asprintf (&pFullLogFileName, "%s_%d", pLogFileName, OwnPID);
      if ((wr <= 0) || (pFullLogFileName == NULL))
      {
         if (LogStdout)
            printf ("\nLog file error: Can't build filename");
         return AAFF_MEMALLOC_FAILED;
      }
      else
      {
         pFile = fopen64 (pFullLogFileName, "a");
         if (pFile == NULL)
         {
            if (LogStdout)
               printf ("\nLog file error: Can't be opened");
            return AAFF_CANNOT_OPEN_LOGFILE;
         }
         else
         {
            fprintf  (pFile, "%-*s", LOG_HEADER_LEN, &LogLineHeader[0]);
            vfprintf (pFile, pFormat, pArguments);
            fprintf  (pFile, "\n");
            fclose   (pFile);
         }
         free (pFullLogFileName);
      }
   }
   if (LogStdout)
   {
      printf  ("%s", &LogLineHeader[0]);
      vprintf (pFormat, pArguments);
      printf  ("\n");
   }
   return AAFF_OK;
}

int LogEntry (const char *pLogFileName, uint8_t LogStdout, const char *pFileName, const char *pFunctionName, int LineNr, const char *pFormat, ...)
{
   va_list VaList;
   int     rc;

   if (!LogStdout && (pLogFileName==NULL))
      return AAFF_OK;

   va_start (VaList, pFormat); //lint !e530 Symbol 'VaList' not initialized
   rc = LogvEntry (pLogFileName, LogStdout, pFileName, pFunctionName, LineNr, pFormat, VaList);
   va_end(VaList);
   return rc;
}

// CHK requires existance of pAaff handle

#ifdef AAFF_STANDALONE
   #define LOG_ERRORS_ON_STDOUT TRUE
#else
   #define LOG_ERRORS_ON_STDOUT pAaff->LogStdout
#endif

#define CHK(ChkVal)                                                             \
{                                                                               \
   int ChkValRc;                                                                \
   if ((ChkValRc=(ChkVal)) != AAFF_OK)                                          \
   {                                                                            \
      const char *pErr = AaffGetErrorMessage (ChkValRc);                        \
      LogEntry (pAaff->pLogFilename, LOG_ERRORS_ON_STDOUT, __FILE__, __FUNCTION__, __LINE__, "Error %d (%s) occured", ChkValRc, pErr); \
      return ChkValRc;                                                          \
   }                                                                            \
}

#define LOG(...) \
   LogEntry (pAaff->pLogFilename, pAaff->LogStdout, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);


// AaffCheckError is called before exiting AaffRead. It should not
// be called elsewehere or else the statistics would become wrong.
static void AaffCheckError (t_pAaff pAaff, int Ret, int *pErrno)
{
   *pErrno = 0;
   if (Ret != AAFF_OK)
   {
      if      ((Ret >= AAFF_ERROR_ENOMEM_START) && (Ret <= AAFF_ERROR_ENOMEM_END)) *pErrno = ENOMEM;
      else if ((Ret >= AAFF_ERROR_EINVAL_START) && (Ret <= AAFF_ERROR_EINVAL_END)) *pErrno = EINVAL;
      else                                                                         *pErrno = EIO;    // all other errors
   }
}

// ------------------------------------
//         Internal functions
// ------------------------------------

uint64_t AaffU64 (char *pData)
{
   uint64_t Val=0;
   int      i;

   for (i=4; i<8; i++)  Val = (Val << 8) | pData[i];
   for (i=0; i<4; i++)  Val = (Val << 8) | pData[i];

   return Val;
}

static int AaffPageNumberFromSegmentName (char *pSegmentName, uint64_t *pPageNumber)
{
   char *pSegmentNamePageNumber;
   char *pTail;

   pSegmentNamePageNumber = &pSegmentName[strlen(AFF_SEGNAME_PAGE)];
   *pPageNumber = strtoull (pSegmentNamePageNumber, &pTail, 10);
   if (*pTail != '\0')
      return AAFF_INVALID_PAGE_NUMBER;  // There should be no extra chars after the number

   return AAFF_OK;
}

static inline uint64_t AaffGetCurrentSeekPos (t_Aaff *pAaff)
{
   return ftello (pAaff->pFile);
}

static inline uint64_t AaffSetCurrentSeekPos (t_Aaff *pAaff, uint64_t Val, int Whence)
{
   if (fseeko (pAaff->pFile, Val, Whence) != 0)
      return AAFF_CANNOT_SEEK;
   return AAFF_OK;
}


static int AaffReadFile (t_Aaff *pAaff, void *pData, uint32_t DataLen)
{
   if (fread (pData, DataLen, 1, pAaff->pFile) != 1)
      return AAFF_CANNOT_READ_DATA;

   return AAFF_OK;
}

static int AaffRealloc (void **ppBuff, uint32_t *pCurrentLen, uint32_t NewLen)
{
   if (NewLen > *pCurrentLen)
   {
      *ppBuff  = realloc (*ppBuff, NewLen);
      if (*ppBuff == NULL)
         return AAFF_MEMALLOC_FAILED;
      *pCurrentLen = NewLen;
   }
   return AAFF_OK;
}

static int AaffReadSegment (t_pAaff pAaff, char **ppName, uint32_t *pArg, char **ppData, uint32_t *pDataLen)
{
   t_AffSegmentHeader Header;
   t_AffSegmentFooter Footer;

   CHK (AaffReadFile (pAaff, &Header, offsetof(t_AffSegmentHeader, Name)))
   if (strcmp (&Header.Magic[0], AFF_SEGMENT_HEADER_MAGIC) != 0)
      return AAFF_INVALID_HEADER;
   Header.NameLen  = ntohl (Header.NameLen );
   Header.DataLen  = ntohl (Header.DataLen );
   Header.Argument = ntohl (Header.Argument);
   CHK (AaffRealloc ((void**)&pAaff->pNameBuff, &pAaff->NameBuffLen, Header.NameLen+1)) // alloc +1, as is might be a string which can be more
   CHK (AaffRealloc ((void**)&pAaff->pDataBuff, &pAaff->DataBuffLen, Header.DataLen+1)) // easily handled by the calling fn when adding a \0
   CHK (AaffReadFile (pAaff, pAaff->pNameBuff, Header.NameLen))
   if (Header.DataLen)
      CHK (AaffReadFile (pAaff, pAaff->pDataBuff, Header.DataLen))

   pAaff->pNameBuff[Header.NameLen] = '\0';
   pAaff->pDataBuff[Header.DataLen] = '\0';

   if (ppName)   *ppName   = pAaff->pNameBuff;
   if (pArg  )   *pArg     = Header.Argument;
   if (ppData)   *ppData   = pAaff->pDataBuff;
   if (pDataLen) *pDataLen = Header.DataLen;

   // Read footer and position file pointer to next segemnt at the same time
   // ----------------------------------------------------------------------
   CHK (AaffReadFile (pAaff, &Footer, sizeof(Footer)))
   if (strcmp (&Footer.Magic[0], AFF_SEGMENT_FOOTER_MAGIC) != 0)
      return AAFF_INVALID_FOOTER;

   return AAFF_OK;
}

static int AaffReadSegmentPage (t_pAaff pAaff, uint64_t SearchPage, uint64_t *pFoundPage, char **ppData, uint32_t *pDataLen)
{
   t_AffSegmentHeader Header;
   t_AffSegmentFooter Footer;
   char               SearchPageStr[128];
   int                rc = AAFF_OK;

   *ppData   = NULL;
   *pDataLen = 0;
   sprintf (SearchPageStr, "page%" PRIu64, SearchPage);

   CHK (AaffReadFile (pAaff, &Header, offsetof(t_AffSegmentHeader, Name)))
   if (strcmp (&Header.Magic[0], AFF_SEGMENT_HEADER_MAGIC) != 0)
      return AAFF_INVALID_HEADER;
   Header.NameLen  = ntohl (Header.NameLen );
   Header.DataLen  = ntohl (Header.DataLen );
   Header.Argument = ntohl (Header.Argument);
   CHK (AaffRealloc ((void**)&pAaff->pNameBuff, &pAaff->NameBuffLen, Header.NameLen+1))
   CHK (AaffReadFile (pAaff, pAaff->pNameBuff, Header.NameLen))
   pAaff->pNameBuff[Header.NameLen] = '\0';

   if (strncmp (pAaff->pNameBuff, AFF_SEGNAME_PAGE, strlen(AFF_SEGNAME_PAGE)) != 0)
      return AAFF_WRONG_SEGMENT;

   CHK (AaffPageNumberFromSegmentName (pAaff->pNameBuff, pFoundPage))
   if (*pFoundPage == SearchPage)
   {
      unsigned int Len;
      uLongf       ZLen;
      int          zrc;

      switch (Header.Argument)
      {
         case AFF_PAGEFLAGS_UNCOMPRESSED:
            CHK (AaffReadFile (pAaff, pAaff->pPageBuff, Header.DataLen))
            pAaff->PageBuffDataLen = Header.DataLen;
            break;
         case AFF_PAGEFLAGS_COMPRESSED_ZERO:
            CHK (AaffReadFile (pAaff, &Len, sizeof(Len)))
            Len = ntohl (Len);
            memset (pAaff->pPageBuff, 0, Len);
            pAaff->PageBuffDataLen = Len;
            break;
         case AFF_PAGEFLAGS_COMPRESSED_ZLIB:
            CHK (AaffRealloc ((void**)&pAaff->pDataBuff, &pAaff->DataBuffLen, Header.DataLen));
            CHK (AaffReadFile (pAaff, pAaff->pDataBuff, Header.DataLen))                     // read into pDataBuff
            ZLen = pAaff->PageSize;                                                          // size of pPageBuff
            zrc = uncompress ((unsigned char*)(pAaff->pPageBuff), &ZLen, (unsigned char*)(pAaff->pDataBuff), Header.DataLen);    // uncompress into pPageBuff
            pAaff->PageBuffDataLen = ZLen;
            if (zrc != Z_OK)
               return AAFF_UNCOMPRESS_FAILED;
            break;
         default:
            return AAFF_INVALID_PAGE_ARGUMENT;
      }
      *ppData   = pAaff->pPageBuff;
      *pDataLen = pAaff->PageBuffDataLen;
      pAaff->CurrentPage = *pFoundPage;
      rc = AAFF_FOUND;
   }
   else
   {
      CHK (AaffSetCurrentSeekPos (pAaff, Header.DataLen, SEEK_CUR))
   }

   // Read footer and position file pointer to next segemnt at the same time
   // ----------------------------------------------------------------------
   CHK (AaffReadFile (pAaff, &Footer, sizeof(Footer)))
   if (strcmp (&Footer.Magic[0], AFF_SEGMENT_FOOTER_MAGIC) != 0)
      return AAFF_INVALID_FOOTER;

   return rc;
}

static int AaffReadPage (t_pAaff pAaff, uint64_t Page, char **ppBuffer, uint32_t *pLen)
{
    if (Page >= pAaff->TotalPages)
       return AAFF_READ_BEYOND_LAST_PAGE;

   // Check if it's the current page
   // ------------------------------
   if (Page == pAaff->CurrentPage)
   {
      *ppBuffer = pAaff->pPageBuff;
      *pLen     = pAaff->PageBuffDataLen;
      return AAFF_OK;
   }

   // Set the seek position for starting the search
   // ---------------------------------------------
   int MaxHops;

   if ((pAaff->CurrentPage   != AAFF_CURRENTPAGE_NOTSET) &&
       (pAaff->CurrentPage+1 == Page))             // The current seek pos already is the correct one
   {
      MaxHops = 1;
   }
   else                                            // Find the closest entry in PageSeekArr
   {
      int64_t Entry;

      Entry = Page / pAaff->Interleave;
      while (pAaff->pPageSeekArr[Entry] == 0)
      {
         Entry--;
         if (Entry<0)
            return AAFF_SEEKARR_CORRUPT;
      }
      AaffSetCurrentSeekPos (pAaff, pAaff->pPageSeekArr[Entry], SEEK_SET);
      MaxHops = Page - (Entry * pAaff->Interleave) +1;
   }

   // Run through segment list until page is found
   // --------------------------------------------
   uint64_t Seek;
   uint64_t FoundPage=0;
   int                rc;

   LOG ("Searching for page %" PRIu64 ", MaxHops=%d", Page, MaxHops);
   while (MaxHops--)
   {
      Seek = AaffGetCurrentSeekPos (pAaff);
      rc   = AaffReadSegmentPage   (pAaff, Page, &FoundPage, ppBuffer, pLen);
      if (rc != AAFF_FOUND)
         CHK (rc)
      LOG ("   %" PRIu64 " (%d)", FoundPage, rc);
      if ((FoundPage % pAaff->Interleave) == 0)
         pAaff->pPageSeekArr[FoundPage/pAaff->Interleave] = Seek;
      if (rc == AAFF_FOUND)
         break;
   }
   if (MaxHops < 0)
      return AAFF_PAGE_NOT_FOUND;

   return AAFF_OK;
}

// ---------------
//  API functions
// ---------------

static int AaffCreateHandle (void **ppHandle, const char *pFormat, uint8_t Debug)
{
   t_pAaff pAaff;

   *ppHandle = NULL;

   pAaff = (t_pAaff) malloc (sizeof(t_Aaff));
   if (pAaff == NULL)
      return AAFF_MEMALLOC_FAILED;

   memset (pAaff, 0, sizeof(t_Aaff));
   pAaff->MaxPageArrMem = AAFF_DEFAULT_MAX_PAGE_ARR_MEM;
   pAaff->LogStdout     = Debug;

   *ppHandle = (void*) pAaff;

   return AAFF_OK;
}

static int AaffDestroyHandle (void **ppHandle)
{
   t_pAaff pAaff = (t_pAaff) *ppHandle;

   if (pAaff->pFilename)       free (pAaff->pFilename);
   if (pAaff->pPageSeekArr)    free (pAaff->pPageSeekArr);
   if (pAaff->pLibVersion)     free (pAaff->pLibVersion);
   if (pAaff->pFileType)       free (pAaff->pFileType);
   if (pAaff->pNameBuff)       free (pAaff->pNameBuff);
   if (pAaff->pDataBuff)       free (pAaff->pDataBuff);
   if (pAaff->pPageBuff)       free (pAaff->pPageBuff);
   if (pAaff->pInfoBuffConst)  free (pAaff->pInfoBuffConst);
   if (pAaff->pInfoBuff)       free (pAaff->pInfoBuff);

   memset (pAaff, 0, sizeof(t_Aaff));
   free (pAaff);
   *ppHandle = NULL;

   return AAFF_OK;
}


int AaffOpen (void *pHandle, const char **ppFilenameArr, uint64_t FilenameArrLen)
{
   t_pAaff  pAaff = (t_pAaff) pHandle;
   char      Signature[strlen(AFF_HEADER)+1];
   uint64_t  Seek;

   LOG ("Called - Files=%" PRIu64, FilenameArrLen);

   if (FilenameArrLen != 1)
      CHK (AAFF_SPLIT_IMAGES_NOT_SUPPORTED)

   pAaff->pFilename = strdup (ppFilenameArr[0]);
   pAaff->pFile     = fopen  (ppFilenameArr[0],"r");
   if(pAaff->pFile==NULL)
   {
      (void) AaffDestroyHandle ((void**) &pAaff);
      CHK (AAFF_FILE_OPEN_FAILED)
   }

   // Check signature
   // ---------------
   CHK (AaffReadFile (pAaff, &Signature, sizeof(Signature)))
   if (memcmp (Signature, AFF_HEADER, sizeof(Signature)) !=0)
   {
      (void)AaffClose((void**)&pAaff);
      CHK (AAFF_INVALID_SIGNATURE)
   }

   // Read header segments
   // --------------------
   char          *pName;
   uint32_t        Arg;
   char          *pData;
   uint32_t        DataLen;
   const int       MAX_HEADER_SEGMENTS = 100;
   int             Seg;
   unsigned int    i;
   int             wr;
   int             Pos = 0;
   const unsigned  HexStrLen = 32;
   char            HexStr[HexStrLen+1];

   #define REM (AaffInfoBuffLen-Pos)

   pAaff->pInfoBuffConst = (char *) malloc (AaffInfoBuffLen);
   pAaff->pInfoBuff      = (char *) malloc (AaffInfoBuffLen*2);

   // Search for known segments at the image start
   for (Seg=0; Seg<MAX_HEADER_SEGMENTS; Seg++)
   {
      Seek = AaffGetCurrentSeekPos (pAaff);
      CHK (AaffReadSegment (pAaff, &pName, &Arg, &pData, &DataLen))

      if      (strcmp (pName, AFF_SEGNAME_PAGESIZE)       == 0 )    pAaff->PageSize    = Arg;
      else if (strcmp (pName, AFF_SEGNAME_SECTORSIZE)     == 0 )    pAaff->SectorSize  = Arg;
      else if (strcmp (pName, AFF_SEGNAME_SECTORS)        == 0 )    pAaff->Sectors     = AaffU64(pData);
      else if (strcmp (pName, AFF_SEGNAME_IMAGESIZE)      == 0 )    pAaff->ImageSize   = AaffU64(pData);
      else if (strcmp (pName, AFF_SEGNAME_AFFLIB_VERSION) == 0 )    pAaff->pLibVersion = strdup((char*)pData);
      else if (strcmp (pName, AFF_SEGNAME_FILETYPE)       == 0 )    pAaff->pFileType   = strdup((char*)pData);
      else if ((strcmp(pName, AFF_SEGNAME_GID)            == 0 ) ||
               (strcmp(pName, AFF_SEGNAME_BADFLAG)        == 0 ))
      {
         wr=0;
         for (i=0; i<GETMIN(DataLen,HexStrLen/2); i++)
            wr += sprintf (&HexStr[wr], "%02X", pData[i]);
         HexStr[i] = '\0';
         Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"%-25s %s", pName, HexStr);
         if (i<DataLen)
            Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"...");
         Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"\n");
      }
      else if (strncmp(pName,AFF_SEGNAME_PAGE,strlen(AFF_SEGNAME_PAGE))==0)
      {
         break;
      }
      else
      {
         if ((Arg == 0) && DataLen)
         Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"%-25s %s\n", pName, pData);
      }
   }
   #undef REM

   if (Seg >= MAX_HEADER_SEGMENTS)
   {
      (void) AaffClose ((void**)&pAaff);
      CHK (AAFF_TOO_MANY_HEADER_SEGEMENTS)
   }

   if (strstr (pAaff->pLibVersion, "Guymager") == NULL)
   {
      (void) AaffClose ((void**)&pAaff);
      CHK (AAFF_NOT_CREATED_BY_GUYMAGER)
   }

   // Prepare page seek array
   // -----------------------
   uint64_t MaxEntries;
   int      ArrBytes;

   pAaff->TotalPages = pAaff->ImageSize / pAaff->PageSize;
   if (pAaff->ImageSize % pAaff->PageSize)
      pAaff->TotalPages++;

   MaxEntries = (pAaff->MaxPageArrMem*1024*1024) / (sizeof (unsigned long long *) + 1); // +1 in order not to risk a result of 0
   MaxEntries = GETMIN (MaxEntries, pAaff->TotalPages);

   pAaff->Interleave = pAaff->TotalPages / MaxEntries;
   if (pAaff->TotalPages % MaxEntries)
      pAaff->Interleave++;

   pAaff->PageSeekArrLen = pAaff->TotalPages / pAaff->Interleave;
   ArrBytes              = pAaff->PageSeekArrLen * sizeof(uint64_t *);
   pAaff->pPageSeekArr   = (uint64_t*)malloc (ArrBytes);
   memset (pAaff->pPageSeekArr, 0, ArrBytes);
   CHK (AaffPageNumberFromSegmentName (pName, &pAaff->CurrentPage));
   if (pAaff->CurrentPage != 0)
   {
      (void) AaffClose ((void**)&pAaff);
      CHK (AAFF_UNEXPECTED_PAGE_NUMBER)
   }
   pAaff->pPageSeekArr[0] = Seek;

   // Alloc Buffers
   // -------------
   pAaff->pPageBuff   = (char *) malloc (pAaff->PageSize);
   pAaff->CurrentPage = AAFF_CURRENTPAGE_NOTSET;

   LOG ("Ret");
   return AAFF_OK;
}

static int AaffClose (void *pHandle)
{
   t_pAaff pAaff = (t_pAaff) pHandle;
   int      rc   = AAFF_OK;

   LOG ("Called");

   if (fclose (pAaff->pFile))
      rc = AAFF_CANNOT_CLOSE_FILE;

   LOG ("Ret");
   return rc;
}

static int AaffSize (void *pHandle, uint64_t *pSize)
{
   t_pAaff pAaff = (t_pAaff) pHandle;

   LOG ("Called");
   *pSize = pAaff->ImageSize;

   LOG ("Ret - Size=%" PRIu64, *pSize);
   return AAFF_OK;
}

static int AaffRead (void *pHandle, char *pBuf, off_t Seek, size_t Count, size_t *pRead, int *pErrno)
{
   t_pAaff   pAaff = (t_pAaff) pHandle;
   char     *pPageBuffer=NULL;
   uint64_t   Page;
   uint64_t   Seek64;
   uint64_t   Remaining;
   uint32_t   PageLen=0, Ofs, ToCopy;
   int        Ret = AAFF_OK;

   LOG ("Called - Seek=%'" PRIu64 ",Count=%'" PRIu64, Seek, Count);
   *pRead  = 0;
   *pErrno = 0;

   if (Seek < 0)
   {
      Ret = AAFF_NEGATIVE_SEEK;
      goto Leave;
   }
   Seek64 = Seek;

   if (Seek64 >= pAaff->ImageSize)        // If calling function asks
      goto Leave;                       // for data beyond end of
   if ((Seek64+Count) > pAaff->ImageSize) // image simply return what
      Count = pAaff->ImageSize - Seek64;  // is possible.

   Page      = Seek64 / pAaff->PageSize;
   Ofs       = Seek64 % pAaff->PageSize;
   Remaining = Count;

   while (Count)
   {
      Ret = AaffReadPage (pAaff, Page, &pPageBuffer, &PageLen);
      if (Ret)
         goto Leave;
      if (PageLen == 0)
      {
         Ret = AAFF_PAGE_LENGTH_ZERO;
         goto Leave;
      }
      ToCopy = GETMIN (PageLen-Ofs, Remaining);
      memcpy (pBuf, pPageBuffer+Ofs, ToCopy);
      Remaining -= ToCopy;
      pBuf      += ToCopy;
      *pRead    += ToCopy;
      Ofs=0;
      Page++;
   }

Leave:
   AaffCheckError (pAaff, Ret, pErrno);
   LOG ("Ret %d - Read=%" PRIu32, Ret, *pRead);
   return Ret;
}

static int AaffOptionsHelp (const char **ppHelp)
{
   char *pHelp=NULL;
   int    wr;

   wr = asprintf (&pHelp, "    %-12s : Maximum amount of RAM cache, in MiB, for image seek offsets. Default: %"PRIu64" MiB\n"
                          "    %-12s : Log file name.\n"
                          "    Specify full path for %s. The given file name is extended by _<pid>.\n",
                          AAFF_OPTION_MAXPAGEARRMEM, AAFF_DEFAULT_MAX_PAGE_ARR_MEM,
                          AAFF_OPTION_LOG,
                          AAFF_OPTION_LOG);
   if ((pHelp == NULL) || (wr<=0))
      return AAFF_MEMALLOC_FAILED;

   *ppHelp = pHelp;
   return AAFF_OK;
}

static int AaffOptionsParse (void *pHandle, uint32_t OptionCount, const pts_LibXmountOptions *ppOptions, const char **ppError)
{
   pts_LibXmountOptions pOption;
   t_pAaff              pAaff  = (t_pAaff) pHandle;
   const char          *pError = NULL;
   int                   rc    = AAFF_OK;
   int                   Ok;

   LOG ("Called - OptionCount=%" PRIu32, OptionCount);
   *ppError = NULL;

   #define TEST_OPTION_UINT64(Opt,DestField)                      \
      if (strcmp (pOption->p_key, Opt) == 0)                      \
      {                                                           \
         pAaff->DestField = StrToUint64 (pOption->p_value, &Ok);  \
         if (!Ok)                                                 \
         {                                                        \
            pError = "Error in option %s: Invalid value";         \
            break;                                                \
         }                                                        \
         LOG ("Option %s set to %" PRIu64, Opt, pAaff->DestField) \
      }

   for (uint32_t i=0; i<OptionCount; i++)
   {
      pOption = ppOptions[i];
      if (strcmp (pOption->p_key, AAFF_OPTION_LOG) == 0)
      {
         pAaff->pLogFilename = strdup (pOption->p_value);
         rc = LOG ("Logging for libxmount_input_aaff started")
         if (rc != AAFF_OK)
         {
            pError = "Write test to log file failed";
            break;
         }
         pOption->valid = TRUE;
         LOG ("Option %s set to %s", AAFF_OPTION_LOG, pAaff->pLogFilename);
      }
      else TEST_OPTION_UINT64 (AAFF_OPTION_MAXPAGEARRMEM, MaxPageArrMem)
   }
   #undef TEST_OPTION_UINT64

   if (pError)
      *ppError = strdup (pError);
   LOG ("Ret - rc=%d,Error=%s", rc, *ppError);
   return rc;
}

static int AaffGetInfofileContent (void *pHandle, const char **ppInfoBuf)
{
   t_pAaff  pAaff  = (t_pAaff) pHandle;
   uint64_t i;
   uint64_t Entries = 0;
   int      Pos     = 0;

   LOG ("Called");

   #define REM (AaffInfoBuffLen-Pos)

   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM,   "AFF IMAGE INFORMATION");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n---------------------");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nAFF file    %s"  , pAaff->pFilename  );

   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nPage size   %u"  , pAaff->PageSize   );
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSector size %d"  , pAaff->SectorSize );
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSectors     %" PRIu64, pAaff->Sectors);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nImage size  %" PRIu64 " (%0.1f GiB)", pAaff->ImageSize, pAaff->ImageSize/(1024.0*1024.0*1024.0));
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nTotal pages %" PRIu64, pAaff->TotalPages);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n%s", pAaff->pInfoBuffConst);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nCurrent page       ");
   if (pAaff->CurrentPage == AAFF_CURRENTPAGE_NOTSET)
        Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "not set");
   else Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "%" PRIu64, pAaff->CurrentPage);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSeek array length  %" PRIu64, pAaff->PageSeekArrLen);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSeek interleave    %" PRIu64, pAaff->Interleave);

   for (i=0; i<pAaff->PageSeekArrLen; i++)
   {
      if (pAaff->pPageSeekArr[i])
         Entries++;
   }
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSeek array entries %" PRIu64, Entries);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n");
   #undef REM

   *ppInfoBuf = strdup (pAaff->pInfoBuff);
   if (*ppInfoBuf == NULL)
      CHK (AAFF_MEMALLOC_FAILED)

   LOG ("Ret - %d bytes of info", strlen(*ppInfoBuf)+1);
   return AAFF_OK;
}

static const char* AaffGetErrorMessage (int ErrNum)
{
   const char *pMsg;
   #define ADD_ERR(ErrCode)              \
      case ErrCode: pMsg = #ErrCode; \
      break;

   switch (ErrNum)
   {
      ADD_ERR (AAFF_OK)
      ADD_ERR (AAFF_FOUND)
      ADD_ERR (AAFF_MEMALLOC_FAILED)
      ADD_ERR (AAFF_OPTIONS_ERROR)
      ADD_ERR (AAFF_SPLIT_IMAGES_NOT_SUPPORTED)
      ADD_ERR (AAFF_INVALID_SIGNATURE)
      ADD_ERR (AAFF_NOT_CREATED_BY_GUYMAGER)
      ADD_ERR (AAFF_CANNOT_OPEN_LOGFILE)
      ADD_ERR (AAFF_FILE_OPEN_FAILED)
      ADD_ERR (AAFF_CANNOT_READ_DATA)
      ADD_ERR (AAFF_INVALID_HEADER)
      ADD_ERR (AAFF_INVALID_FOOTER)
      ADD_ERR (AAFF_TOO_MANY_HEADER_SEGEMENTS)
      ADD_ERR (AAFF_INVALID_PAGE_NUMBER)
      ADD_ERR (AAFF_UNEXPECTED_PAGE_NUMBER)
      ADD_ERR (AAFF_CANNOT_CLOSE_FILE)
      ADD_ERR (AAFF_CANNOT_SEEK)
      ADD_ERR (AAFF_WRONG_SEGMENT)
      ADD_ERR (AAFF_UNCOMPRESS_FAILED)
      ADD_ERR (AAFF_INVALID_PAGE_ARGUMENT)
      ADD_ERR (AAFF_SEEKARR_CORRUPT)
      ADD_ERR (AAFF_PAGE_NOT_FOUND)
      ADD_ERR (AAFF_READ_BEYOND_IMAGE_LENGTH)
      ADD_ERR (AAFF_READ_BEYOND_LAST_PAGE)
      ADD_ERR (AAFF_PAGE_LENGTH_ZERO)
      ADD_ERR (AAFF_NEGATIVE_SEEK)
      default:
         pMsg = "Unknown error";
   }
   #undef ARR_ERR
   return pMsg;
}

static int AaffFreeBuffer (void *pBuf)
{
   free (pBuf);
   return AAFF_OK;
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
   return "aaff\0\0";
}

void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *pFunctions)
{
  pFunctions->CreateHandle       = &AaffCreateHandle;
  pFunctions->DestroyHandle      = &AaffDestroyHandle;
  pFunctions->Open               = &AaffOpen;
  pFunctions->Close              = &AaffClose;
  pFunctions->Size               = &AaffSize;
  pFunctions->Read               = &AaffRead;
  pFunctions->OptionsHelp        = &AaffOptionsHelp;
  pFunctions->OptionsParse       = &AaffOptionsParse;
  pFunctions->GetInfofileContent = &AaffGetInfofileContent;
  pFunctions->GetErrorMessage    = &AaffGetErrorMessage;
  pFunctions->FreeBuffer         = &AaffFreeBuffer;
}

// -----------------------------------------------------
//              Small main routine for testing
//              It converts an aff file to dd
// -----------------------------------------------------

#ifdef AAFF_STANDALONE

#define PRINT_ERROR_AND_EXIT(...) \
{                                 \
   printf (__VA_ARGS__);          \
   exit (1);                      \
}

int ParseOptions (t_pAaff pAaff, char *pOptions)
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
      return AAFF_OK;

   if (*pOptions == '\0')
      return AAFF_OK;

   if (*pOptions == ',')
      return AAFF_OPTIONS_ERROR;

   if (pOptions[strlen(pOptions)-1] == ',')
      return AAFF_OPTIONS_ERROR;

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

   // Create pointer array and call parse function
   ppOptionArr = (pts_LibXmountOptions *)malloc (OptionCount*sizeof (pts_LibXmountOptions));
   if (ppOptionArr == NULL)
      PRINT_ERROR_AND_EXIT ("Cannot allocate ppOptionArr");
   for (int i=0; i<OptionCount; i++)
      ppOptionArr[i] = &pOptionArr[i];

   rc = AaffOptionsParse ((void*) pAaff, OptionCount, ppOptionArr, &pError);
   free (ppOptionArr);
   free ( pOptionArr);
   free ( pOpt);
   if (pError)
      PRINT_ERROR_AND_EXIT ("Error while setting options: %s", pError);
   CHK (rc)

   return AAFF_OK;
}

int main(int argc, const char *argv[])
{
   t_pAaff     pAaff;
   const char *pInfoBuff;
   const char *pHelp;
   uint64_t     Remaining;
   uint64_t     CurrentPos=0;
   int          rc;
   int          Errno;
   int          Percent;
   int          PercentOld;
   char       *pOptions = NULL;

   setbuf (stdout, NULL);
   setbuf (stderr, NULL);
   setlocale (LC_ALL, "");

   printf ("AFF to DD converter\n");
   if (argc != 3)
   {
      (void) AaffOptionsHelp (&pHelp);
      printf ("Usage: %s <EWF segment file 1> <EWF segment file 2> <...> [-comma_separated_options]\n", argv[0]);
      printf ("Possible options:\n%s\n", pHelp);
      printf ("The output file will be named dd.\n");
      CHK (AaffFreeBuffer ((void*) pHelp))
      exit (1);
   }
   if (argv[argc-1][0] == '-')
   {
      pOptions = strdup (&(argv[argc-1][1]));
      argc--;
   }

   rc = AaffCreateHandle ((void**) &pAaff, "aaff", LOG_STDOUT);
   if (rc != AAFF_OK)
      PRINT_ERROR_AND_EXIT ("Cannot create handle, rc=%d\n", rc)
   if (pOptions)
      CHK (ParseOptions(pAaff, pOptions))

   rc = AaffOpen (pAaff, &argv[1], 1);
   if (rc)
   {
      printf ("Error %d while opening file %s\n", rc, argv[1]);
      exit (2);
   }

   CHK (AaffGetInfofileContent ((void*) pAaff, &pInfoBuff))
   if (pInfoBuff)
      printf ("Contents of info buffer:\n%s\n", pInfoBuff);
   CHK (AaffFreeBuffer ((void*)pInfoBuff))

   // Create destination file and fill it with data from aff
   // ------------------------------------------------------
   FILE *pFile;
   pFile = fopen (argv[2], "w");
//   const unsigned BuffSize = 13;  // weird Buffsize for testing
   const unsigned  BuffSize = 65536;
   char          *pBuff;
   uint64_t        ToRead;
   uint64_t        Read;

   Remaining = pAaff->ImageSize;

   pBuff = (char *) malloc (BuffSize);
   CurrentPos=0;

   Errno      =  0;
   PercentOld = -1;

   while (Remaining)
   {
      ToRead = GETMIN (Remaining, BuffSize);
      rc = AaffRead ((void*) pAaff, pBuff, CurrentPos, ToRead, &Read, &Errno);
      if ((rc != AAFF_OK) || (Errno != 0))
         PRINT_ERROR_AND_EXIT("Error %d while calling AewfRead (Errno %d)\n", rc, Errno);
      if (Read != ToRead)
         PRINT_ERROR_AND_EXIT("Only %" PRIu64 " out of %" PRIu64 " bytes read\n", Read, ToRead);

      if (fwrite (pBuff, Read, 1, pFile) != 1)
      {
         printf ("Could not write to destinationfile\n");
         exit (2);
      }
      CurrentPos += Read;
      Remaining  -= Read;
      Percent = (100*CurrentPos) / pAaff->ImageSize;
      if (Percent != PercentOld)
      {
         printf ("\r%d%% done...", Percent);
         PercentOld = Percent;
      }
   }
   free (pBuff);
   fclose (pFile);

   return 0;
}

#endif

