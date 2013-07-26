/*******************************************************************************
* xmount Copyright (c) 2008,2009, 2010, 2011, 2012                             *
*                      by Gillen Daniel <gillen.dan@pinguin.lu>                *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing simple AFF images created by Guymager.                             *
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


#include <netinet/in.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <limits.h>
#include <zlib.h>

#include "aaff.h"

// ----------------------
//  Constant definitions
// ----------------------

//#define AAFF_DEBUG

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
   unsigned long long     Sectors;
   unsigned long long     ImageSize;
   unsigned long long     TotalPages;

   char                *pNameBuff;     // Buffers
   unsigned char       *pDataBuff;
   unsigned int          NameBuffLen;
   unsigned int          DataBuffLen;

   unsigned long long    CurrentPage;
   unsigned char       *pPageBuff;        // Length is PageSize, contains data of CurrentPage
   unsigned int          PageBuffDataLen; // Length of current data in PageBuff (the same for all pages, but the last one might contain less data)

   char                *pInfoBuff;
   char                *pInfoBuffConst;

   unsigned long long  *pPageSeekArr;
   unsigned long long    PageSeekArrLen;
   unsigned long long    Interleave;  // The number of pages lying between 2 entries in the PageSeekArr
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

// ---------------------------
//  Internal static functions
// ---------------------------

static int AaffCreateHandle (t_pAaff *ppAaff)
{
   t_pAaff pAaff;

   pAaff = (t_pAaff) malloc (sizeof(t_Aaff));
   if (pAaff == NULL)
      return AAFF_MEMALLOC_FAILED;

   memset (pAaff, 0, sizeof(t_Aaff));
   *ppAaff = pAaff;

   return AAFF_OK;
}

static int AaffDestroyHandle (t_pAaff *ppAaff)
{
   t_pAaff pAaff = *ppAaff;

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
   *ppAaff = NULL;

   return AAFF_OK;
}

unsigned long long AaffU64 (unsigned char *pData)
{
   unsigned long long Val=0;
   int                i;

   for (i=4; i<8; i++)  Val = (Val << 8) | pData[i];
   for (i=0; i<4; i++)  Val = (Val << 8) | pData[i];

   return Val;
}

static int AaffPageNumberFromSegmentName (char *pSegmentName, unsigned long long *pPageNumber)
{
   char *pSegmentNamePageNumber;
   char *pTail;

   pSegmentNamePageNumber = &pSegmentName[strlen(AFF_SEGNAME_PAGE)];
   *pPageNumber = strtoull (pSegmentNamePageNumber, &pTail, 10);
   if (*pTail != '\0')
      return AAFF_INVALID_PAGE_NUMBER;  // There should be no extra chars after the number

   return AAFF_OK;
}

static inline unsigned long long AaffGetCurrentSeekPos (t_Aaff *pAaff)
{
   return ftello (pAaff->pFile);
}

static inline unsigned long long AaffSetCurrentSeekPos (t_Aaff *pAaff, unsigned long long Val, int Whence)
{
   if (fseeko (pAaff->pFile, Val, Whence) != 0)
      return AAFF_CANNOT_SEEK;
   return AAFF_OK;
}


static int AaffReadFile (t_Aaff *pAaff, void *pData, unsigned int DataLen)
{
   if (fread (pData, DataLen, 1, pAaff->pFile) != 1)
      return AAFF_CANNOT_READ_DATA;

   return AAFF_OK;
}

static int AaffRealloc (void **ppBuff, unsigned int *pCurrentLen, unsigned int NewLen)
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

static int AaffReadSegment (t_pAaff pAaff, char **ppName, unsigned int *pArg, unsigned char **ppData, unsigned int *pDataLen)
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

static int AaffReadSegmentPage (t_pAaff pAaff, unsigned long long SearchPage, unsigned long long *pFoundPage, unsigned char **ppData, unsigned int *pDataLen)
{
   t_AffSegmentHeader Header;
   t_AffSegmentFooter Footer;
   char               SearchPageStr[128];
   int                rc = AAFF_OK;

   *ppData   = NULL;
   *pDataLen = 0;
   sprintf (SearchPageStr, "page%llu", SearchPage);

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
            DEBUG_PRINTF ("\nuncompressed");
            CHK (AaffReadFile (pAaff, pAaff->pPageBuff, Header.DataLen))
            pAaff->PageBuffDataLen = Header.DataLen;
            break;
         case AFF_PAGEFLAGS_COMPRESSED_ZERO:
            DEBUG_PRINTF ("\nzero");
            CHK (AaffReadFile (pAaff, &Len, sizeof(Len)))
            Len = ntohl (Len);
            memset (pAaff->pPageBuff, 0, Len);
            pAaff->PageBuffDataLen = Len;
            break;
         case AFF_PAGEFLAGS_COMPRESSED_ZLIB:
            DEBUG_PRINTF ("\ncompressed");
            CHK (AaffRealloc ((void**)&pAaff->pDataBuff, &pAaff->DataBuffLen, Header.DataLen));
            CHK (AaffReadFile (pAaff, pAaff->pDataBuff, Header.DataLen))                     // read into pDataBuff
            ZLen = pAaff->PageSize;                                                          // size of pPageBuff
            zrc = uncompress (pAaff->pPageBuff, &ZLen, pAaff->pDataBuff, Header.DataLen);    // uncompress into pPageBuff
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

static int AaffReadPage (t_pAaff pAaff, unsigned long long Page, unsigned char **ppBuffer, unsigned int *pLen)
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

   // Set the seek position for startig the search
   // --------------------------------------------
   int MaxHops;

   if ((pAaff->CurrentPage   != AAFF_CURRENTPAGE_NOTSET) &&
       (pAaff->CurrentPage+1 == Page))             // The current seek pos already is the correct one
   {
      MaxHops = 1;
   }
   else                                            // Find the closest entry in PageSeekArr
   {
      long long Entry;

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
   unsigned long long Seek;
   unsigned long long FoundPage;
   int                rc;

   DEBUG_PRINTF ("\nSearching for page %llu, MaxHops=%d -- ", Page, MaxHops);
   while (MaxHops--)
   {
      Seek = AaffGetCurrentSeekPos (pAaff);
      rc = AaffReadSegmentPage (pAaff, Page, &FoundPage, ppBuffer, pLen);
      DEBUG_PRINTF ("  %llu (%d)", FoundPage, rc);
      if ((FoundPage % pAaff->Interleave) == 0)
         pAaff->pPageSeekArr[FoundPage/pAaff->Interleave] = Seek;
      if (rc == AAFF_FOUND)
         break;
   }
   DEBUG_PRINTF ("\n");
   if (MaxHops<0)
      return AAFF_PAGE_NOT_FOUND;

   return AAFF_OK;
}

// ---------------
//  API functions
// ---------------

int AaffOpen (t_pAaff *ppAaff, const char *pFilename, unsigned long long MaxPageArrMem)
{
   t_pAaff            pAaff;
   char                Signature[strlen(AFF_HEADER)+1];
   unsigned long long  Seek;

   *ppAaff = NULL;
   CHK (AaffCreateHandle (&pAaff))

   pAaff->pFilename = strdup (pFilename);
   pAaff->pFile = fopen (pFilename, "r");
   if (pAaff->pFile == NULL)
   {
      AaffDestroyHandle (&pAaff);
      return AAFF_FILE_OPEN_FAILED;
   }

   // Check signature
   // ---------------
   CHK (AaffReadFile (pAaff, &Signature, sizeof(Signature)))
   if (memcmp (Signature, AFF_HEADER, sizeof(Signature)) != 0)
   {
      (void) AaffClose (&pAaff);
      return AAFF_INVALID_SIGNATURE;
   }

   // Read header segments
   // --------------------
   char          *pName;
   unsigned int    Arg;
   unsigned char *pData;
   unsigned int    DataLen;
   const int       MAX_HEADER_SEGMENTS = 100;
   int             Seg;
   unsigned int    i;
   int             wr;
   int             Pos = 0;
   const unsigned  HexStrLen = 32;
   char            HexStr[HexStrLen+1];

   #define REM (AaffInfoBuffLen-Pos)

   pAaff->pInfoBuffConst = malloc (AaffInfoBuffLen);
   pAaff->pInfoBuff      = malloc (AaffInfoBuffLen*2);
   for (Seg=0; Seg<MAX_HEADER_SEGMENTS; Seg++)   // Search for known segments at the image start
   {
      Seek = AaffGetCurrentSeekPos (pAaff);
      CHK (AaffReadSegment (pAaff, &pName, &Arg, &pData, &DataLen))

      if       (strcmp  (pName, AFF_SEGNAME_PAGESIZE      ) == 0) pAaff->PageSize    = Arg;
      else if  (strcmp  (pName, AFF_SEGNAME_SECTORSIZE    ) == 0) pAaff->SectorSize  = Arg;
      else if  (strcmp  (pName, AFF_SEGNAME_SECTORS       ) == 0) pAaff->Sectors     = AaffU64 (pData);
      else if  (strcmp  (pName, AFF_SEGNAME_IMAGESIZE     ) == 0) pAaff->ImageSize   = AaffU64 (pData);
      else if  (strcmp  (pName, AFF_SEGNAME_AFFLIB_VERSION) == 0) pAaff->pLibVersion = strdup  ((char*)pData);
      else if  (strcmp  (pName, AFF_SEGNAME_FILETYPE      ) == 0) pAaff->pFileType   = strdup  ((char*)pData);
      else if ((strcmp  (pName, AFF_SEGNAME_GID           ) == 0) ||
               (strcmp  (pName, AFF_SEGNAME_BADFLAG       ) == 0))
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
      else if  (strncmp (pName, AFF_SEGNAME_PAGE, strlen(AFF_SEGNAME_PAGE)) == 0) break;
      else
      {
         if ((Arg == 0) && DataLen)
            Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"%-25s %s\n", pName, pData);
      }
   }
   #undef REM

   if (Seg >= MAX_HEADER_SEGMENTS)
   {
      (void) AaffClose (&pAaff);
      return AAFF_TOO_MANY_HEADER_SEGEMENTS;
   }

   if (strstr (pAaff->pLibVersion, "Guymager") == NULL)
   {
      (void) AaffClose (&pAaff);
      return AAFF_NOT_CREATED_BY_GUYMAGER;
   }

   // Prepare page seek array
   // -----------------------
   unsigned long long MaxEntries;
   int                ArrBytes;

   pAaff->TotalPages = pAaff->ImageSize / pAaff->PageSize;
   if (pAaff->ImageSize % pAaff->PageSize)
      pAaff->TotalPages++;

   if (MaxPageArrMem)
        MaxEntries = (MaxPageArrMem / sizeof (unsigned long long *)) + 1;  // +1 in order not to risk a result of 0
   else MaxEntries = AAFF_DEFAULT_PAGE_SEEK_MAX_ENTRIES;

   MaxEntries = GETMIN (MaxEntries, pAaff->TotalPages);
   pAaff->Interleave = pAaff->TotalPages / MaxEntries;
   if (pAaff->TotalPages % MaxEntries)
      pAaff->Interleave++;

   pAaff->PageSeekArrLen = pAaff->TotalPages / pAaff->Interleave;
   ArrBytes = pAaff->PageSeekArrLen * sizeof(unsigned long long *);
   pAaff->pPageSeekArr = (unsigned long long *)malloc (ArrBytes);
   memset (pAaff->pPageSeekArr, 0, ArrBytes);
   CHK (AaffPageNumberFromSegmentName (pName, &pAaff->CurrentPage));
   if (pAaff->CurrentPage != 0)
   {
      (void) AaffClose (&pAaff);
      return AAFF_UNEXPECTED_PAGE_NUMBER;
   }
   pAaff->pPageSeekArr[0] = Seek;

   // Alloc Buffers
   // -------------
   pAaff->pPageBuff   = malloc (pAaff->PageSize);
   pAaff->CurrentPage = AAFF_CURRENTPAGE_NOTSET;

   *ppAaff = pAaff;

   return AAFF_OK;
}

int AaffClose (t_pAaff *ppAaff)
{
   int rc = AAFF_OK;

   if (fclose ((*ppAaff)->pFile))
      rc = AAFF_CANNOT_CLOSE_FILE;

   CHK (AaffDestroyHandle (ppAaff))

   return rc;
}

int AaffInfo (t_pAaff pAaff, char **ppInfoBuff)
{
   unsigned long long i;
   unsigned long long Entries;
   int                Pos = 0;
   #define REM (AaffInfoBuffLen-Pos)

   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM,   "AFF IMAGE INFORMATION");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n---------------------");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nAFF file    %s"  , pAaff->pFilename  );

   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nPage size   %u"  , pAaff->PageSize   );
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSector size %d"  , pAaff->SectorSize );
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSectors     %llu", pAaff->Sectors    );
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nImage size  %llu (%0.1f GiB)", pAaff->ImageSize, pAaff->ImageSize/(1024.0*1024.0*1024.0));
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nTotal pages %llu", pAaff->TotalPages );
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n%s", pAaff->pInfoBuffConst);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n");
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nCurrent page       ");
   if (pAaff->CurrentPage == AAFF_CURRENTPAGE_NOTSET)
        Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "not set");
   else Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "%llu", pAaff->CurrentPage);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSeek array length  %llu", pAaff->PageSeekArrLen);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSeek interleave    %llu", pAaff->Interleave);

   for (i=0; i<pAaff->PageSeekArrLen; i++)
      if (pAaff->pPageSeekArr[i])
         Entries++;
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\nSeek array entries %llu", Entries);
   Pos += snprintf (&pAaff->pInfoBuff[Pos], REM, "\n");
   #undef REM

   *ppInfoBuff = pAaff->pInfoBuff;

   return AAFF_OK;
}

int AaffSize (t_pAaff pAaff, unsigned long long *pSize)
{
   *pSize = pAaff->ImageSize;
   return AAFF_OK;
}

int AaffRead (t_pAaff pAaff, unsigned long long Seek, unsigned char *pBuffer, unsigned int Count)
{
   unsigned long long   Page;
   unsigned char      *pPageBuffer;
   unsigned int         PageLen;
   unsigned int         Ofs;
   unsigned int         ToCopy;

   if ((Seek+Count) > pAaff->ImageSize)
      return AAFF_READ_BEYOND_IMAGE_LENGTH;

   Page = Seek / pAaff->PageSize;
   Ofs  = Seek % pAaff->PageSize;

   while (Count)
   {
      CHK (AaffReadPage (pAaff, Page, &pPageBuffer, &PageLen))
      ToCopy = GETMIN (PageLen-Ofs, Count);
      memcpy (pBuffer, pPageBuffer+Ofs, ToCopy);
      Count   -= ToCopy;
      pBuffer += ToCopy;
      Ofs      = 0;
      Page++;
   }

   return AAFF_OK;
}

// -----------------------------------------------------
//              Small main routine for testing
//              It converts an aff file to dd
// -----------------------------------------------------

#ifdef AAFF_MAIN_FOR_TESTING

int main(int argc, char *argv[])
{
   t_pAaff             pAaff;
   char               *pInfoBuff;
   unsigned long long   Remaining;
   unsigned long long   CurrentPos=0;
   int                  rc;
   int                  Percent;
   int                  PercentOld;

   setbuf (stdout, NULL);
   setbuf (stderr, NULL);
   setlocale (LC_ALL, "");

   printf ("AFF to DD converter\n");
   if (argc != 3)
   {
      printf ("Usage: %s <aff source file> <dd destination file>\n", argv[0]);
      exit (1);
   }

//   rc = AaffOpen (&pAaff, argv[1], 1024); // weird seek array size for testing
   rc = AaffOpen (&pAaff, argv[1], 0);
   if (rc)
   {
      printf ("Error %d while opening file %s\n", rc, argv[1]);
      exit (2);
   }

   rc = AaffInfo (pAaff, &pInfoBuff);
   if (rc)
   {
      printf ("Could not retrieve info\n");
      exit (2);
   }
   printf ("%s", pInfoBuff);

   // Create destination file and fill it with data from aff
   // ------------------------------------------------------
   FILE *pFile;
   pFile = fopen (argv[2], "w");
//   const unsigned BuffSize = 13;  // weird Buffsize for testing
   const unsigned BuffSize = 65536;
   unsigned char *pBuff;
   unsigned int   Bytes;
   Remaining = pAaff->ImageSize;

   pBuff = malloc (BuffSize);
   CurrentPos=0;

   PercentOld = -1;
   while (Remaining)
   {
      Bytes = GETMIN (Remaining, BuffSize);
      rc = AaffRead (pAaff, CurrentPos, pBuff, Bytes);
      if (rc != AAFF_OK)
      {
         printf ("Could not read data from aff file, error %d\n", rc);
         exit (2);
      }
      if (fwrite (pBuff, Bytes, 1, pFile) != 1)
      {
         printf ("Could not write to destinationfile\n");
         exit (2);
      }
      CurrentPos += Bytes;
      Remaining  -= Bytes;
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
