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

#include "../libxmount_input.h"

//#define AAFF_DEBUG
#include "libxmount_input_aaff.h"

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
int AaffCreateHandle(void **pp_handle);
int AaffDestroyHandle(void **pp_handle);
int AaffOpen(void **pp_handle,
             const char **pp_filename_arr,
             uint64_t filename_arr_len);
int AaffSize(void *p_handle,
             uint64_t *p_size);
int AaffRead(void *p_handle,
             uint64_t seek,
             char *p_buf,
             uint32_t count);
int AaffClose(void **pp_handle);
const char* AaffOptionsHelp();
int AaffOptionsParse(void *p_handle,
                     char *p_options,
                     char **pp_error);
int AaffGetInfofileContent(void *p_handle,
                           char **pp_info_buf);
void AaffFreeBuffer(void *p_buf);

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
  return "aaff\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
  p_functions->CreateHandle=&AaffCreateHandle;
  p_functions->DestroyHandle=&AaffDestroyHandle;
  p_functions->Open=&AaffOpen;
  p_functions->Close=&AaffClose;
  p_functions->Size=&AaffSize;
  p_functions->Read=&AaffRead;
  p_functions->OptionsHelp=&AaffOptionsHelp;
  p_functions->OptionsParse=&AaffOptionsParse;
  p_functions->GetInfofileContent=&AaffGetInfofileContent;
  p_functions->FreeBuffer=&AaffFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

// ---------------------------
//  Internal static functions
// ---------------------------

static int AaffCreateHandle0 (t_pAaff *ppAaff)
{
   t_pAaff pAaff;

   pAaff = (t_pAaff) malloc (sizeof(t_Aaff));
   if (pAaff == NULL)
      return AAFF_MEMALLOC_FAILED;

   memset (pAaff, 0, sizeof(t_Aaff));
   *ppAaff = pAaff;

   return AAFF_OK;
}

static int AaffDestroyHandle0 (t_pAaff *ppAaff)
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
   uint64_t FoundPage;
   int                rc;

   DEBUG_PRINTF ("\nSearching for page %" PRIu64 ", MaxHops=%d -- ", Page, MaxHops);
   while (MaxHops--)
   {
      Seek = AaffGetCurrentSeekPos (pAaff);
      rc = AaffReadSegmentPage (pAaff, Page, &FoundPage, ppBuffer, pLen);
      DEBUG_PRINTF ("  %" PRIu64 " (%d)", FoundPage, rc);
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

/*
 * AaffCreateHandle
 */
int AaffCreateHandle(void **pp_handle) {
  *pp_handle=NULL;
  CHK(AaffCreateHandle0((t_pAaff*)pp_handle))
  return AAFF_OK;
}

/*
 * AaffDestroyHandle
 */
int AaffDestroyHandle(void **pp_handle) {
  // TODO: Implement
/*
  CHK(AaffDestroyHandle0((t_pAaff*)pp_handle))
  *pp_handle=NULL;
*/
  return AAFF_OK;
}

/*
 * AaffOpen
 */
int AaffOpen(void **pp_handle,
             const char **pp_filename_arr,
             uint64_t filename_arr_len)
{
  t_pAaff pAaff=(t_pAaff)*pp_handle;
  char Signature[strlen(AFF_HEADER)+1];
  uint64_t Seek;

  if(filename_arr_len!=1) {
    // Split aff files are not supported
    // TODO: Set correct error
    return 1;
  }

  pAaff->pFilename=strdup(pp_filename_arr[0]);
  pAaff->pFile=fopen(pp_filename_arr[0],"r");
  if(pAaff->pFile==NULL) {
    AaffDestroyHandle0(&pAaff);
    return AAFF_FILE_OPEN_FAILED;
  }

  // Check signature
  // ---------------
  CHK(AaffReadFile(pAaff,&Signature,sizeof(Signature)))
  if(memcmp(Signature,AFF_HEADER,sizeof(Signature))!=0) {
    (void)AaffClose((void**)&pAaff);
    return AAFF_INVALID_SIGNATURE;
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

  pAaff->pInfoBuffConst = malloc (AaffInfoBuffLen);
  pAaff->pInfoBuff      = malloc (AaffInfoBuffLen*2);
  // Search for known segments at the image start
  for (Seg=0; Seg<MAX_HEADER_SEGMENTS; Seg++) {
    Seek = AaffGetCurrentSeekPos (pAaff);
    CHK (AaffReadSegment (pAaff, &pName, &Arg, &pData, &DataLen))

    if(strcmp(pName,AFF_SEGNAME_PAGESIZE)==0) {
      pAaff->PageSize=Arg;
    } else if(strcmp(pName,AFF_SEGNAME_SECTORSIZE)==0) {
      pAaff->SectorSize=Arg;
    } else if(strcmp(pName,AFF_SEGNAME_SECTORS)==0) {
      pAaff->Sectors=AaffU64(pData);
    } else if(strcmp(pName,AFF_SEGNAME_IMAGESIZE)==0) {
      pAaff->ImageSize=AaffU64(pData);
    } else if(strcmp(pName,AFF_SEGNAME_AFFLIB_VERSION)==0) {
      pAaff->pLibVersion=strdup((char*)pData);
    } else if(strcmp(pName,AFF_SEGNAME_FILETYPE)==0) {
      pAaff->pFileType=strdup((char*)pData);
    } else if((strcmp(pName,AFF_SEGNAME_GID)==0) ||
              (strcmp(pName, AFF_SEGNAME_BADFLAG)==0))
    {
      wr=0;
      for (i=0; i<GETMIN(DataLen,HexStrLen/2); i++)
        wr += sprintf (&HexStr[wr], "%02X", pData[i]);
      HexStr[i] = '\0';
      Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"%-25s %s", pName, HexStr);
      if (i<DataLen) Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"...");
      Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"\n");
    } else if(strncmp(pName,AFF_SEGNAME_PAGE,strlen(AFF_SEGNAME_PAGE))==0) {
      break;
    } else {
      if ((Arg == 0) && DataLen)
      Pos += snprintf (&(pAaff->pInfoBuffConst[Pos]), REM,"%-25s %s\n", pName, pData);
    }
  }
  #undef REM

  if (Seg >= MAX_HEADER_SEGMENTS) {
    (void) AaffClose ((void**)&pAaff);
    return AAFF_TOO_MANY_HEADER_SEGEMENTS;
  }

  if (strstr (pAaff->pLibVersion, "Guymager") == NULL) {
    (void) AaffClose ((void**)&pAaff);
    return AAFF_NOT_CREATED_BY_GUYMAGER;
  }

  // Prepare page seek array
  // -----------------------
  uint64_t MaxEntries;
  int      ArrBytes;

  pAaff->TotalPages = pAaff->ImageSize / pAaff->PageSize;
  if (pAaff->ImageSize % pAaff->PageSize) pAaff->TotalPages++;

  // TODO: MaxPageArrMem was a uint64_t parameter of this function
  MaxEntries = AAFF_DEFAULT_PAGE_SEEK_MAX_ENTRIES;
/*
  if (MaxPageArrMem) {
    // +1 in order not to risk a result of 0
    MaxEntries = (MaxPageArrMem / sizeof (unsigned long long *)) + 1;
  } else MaxEntries = AAFF_DEFAULT_PAGE_SEEK_MAX_ENTRIES;
*/

  MaxEntries = GETMIN (MaxEntries, pAaff->TotalPages);
  pAaff->Interleave = pAaff->TotalPages / MaxEntries;
  if (pAaff->TotalPages % MaxEntries) pAaff->Interleave++;

  pAaff->PageSeekArrLen = pAaff->TotalPages / pAaff->Interleave;
  ArrBytes = pAaff->PageSeekArrLen * sizeof(uint64_t *);
  pAaff->pPageSeekArr = (uint64_t*)malloc (ArrBytes);
  memset (pAaff->pPageSeekArr, 0, ArrBytes);
  CHK (AaffPageNumberFromSegmentName (pName, &pAaff->CurrentPage));
  if (pAaff->CurrentPage != 0)
  {
    (void) AaffClose ((void**)&pAaff);
    return AAFF_UNEXPECTED_PAGE_NUMBER;
  }
  pAaff->pPageSeekArr[0] = Seek;

  // Alloc Buffers
  // -------------
  pAaff->pPageBuff   = malloc (pAaff->PageSize);
  pAaff->CurrentPage = AAFF_CURRENTPAGE_NOTSET;

  return AAFF_OK;
}

/*
 * AaffClose
 */
int AaffClose(void **pp_handle) {
  int rc=AAFF_OK;

  if(fclose((*(t_pAaff*)pp_handle)->pFile)) rc=AAFF_CANNOT_CLOSE_FILE;
  CHK(AaffDestroyHandle0((t_pAaff*)pp_handle))

  return rc;
}

/*
 * AaffGetInfofileContent
 */
int AaffGetInfofileContent(void *p_handle, char **pp_info_buf) {
   uint64_t i;
   uint64_t Entries = 0;
   int      Pos     = 0;
   #define REM (AaffInfoBuffLen-Pos)

   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM,   "AFF IMAGE INFORMATION");
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\n---------------------");
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nAFF file    %s"  , ((t_pAaff)p_handle)->pFilename  );

   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nPage size   %u"  , ((t_pAaff)p_handle)->PageSize   );
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nSector size %d"  , ((t_pAaff)p_handle)->SectorSize );
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nSectors     %" PRIu64, ((t_pAaff)p_handle)->Sectors    );
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nImage size  %" PRIu64 " (%0.1f GiB)", ((t_pAaff)p_handle)->ImageSize, ((t_pAaff)p_handle)->ImageSize/(1024.0*1024.0*1024.0));
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nTotal pages %" PRIu64, ((t_pAaff)p_handle)->TotalPages );
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\n");
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\n%s", ((t_pAaff)p_handle)->pInfoBuffConst);
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\n");
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nCurrent page       ");
   if (((t_pAaff)p_handle)->CurrentPage == AAFF_CURRENTPAGE_NOTSET)
        Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "not set");
   else Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "%" PRIu64, ((t_pAaff)p_handle)->CurrentPage);
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nSeek array length  %" PRIu64, ((t_pAaff)p_handle)->PageSeekArrLen);
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nSeek interleave    %" PRIu64, ((t_pAaff)p_handle)->Interleave);

   for (i=0; i<((t_pAaff)p_handle)->PageSeekArrLen; i++)
      if (((t_pAaff)p_handle)->pPageSeekArr[i])
         Entries++;
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\nSeek array entries %" PRIu64, Entries);
   Pos += snprintf (&((t_pAaff)p_handle)->pInfoBuff[Pos], REM, "\n");
   #undef REM

  // TODO: Rather then copying, generate info text once here. It won't be used
  // after this anymore.
  *pp_info_buf=(char*)malloc(strlen(((t_pAaff)p_handle)->pInfoBuff)+1);
  if(*pp_info_buf==NULL) {
    return AAFF_MEMALLOC_FAILED;
  }

  strcpy(*pp_info_buf,((t_pAaff)p_handle)->pInfoBuff);
  return AAFF_OK;
}

/*
 * AaffSize
 */
int AaffSize(void *p_handle, uint64_t *p_size) {
  *p_size=((t_pAaff)p_handle)->ImageSize;
  return AAFF_OK;
}

/*
 * AaffRead
 */
int AaffRead(void *p_handle,
             uint64_t seek,
             char *p_buf,
             uint32_t count)
{
  uint64_t page;
  char *p_page_buffer;
  uint32_t page_len, ofs, to_copy;

  if((seek+count)>((t_pAaff)p_handle)->ImageSize) {
    return AAFF_READ_BEYOND_IMAGE_LENGTH;
  }

  page=seek/((t_pAaff)p_handle)->PageSize;
  ofs=seek%((t_pAaff)p_handle)->PageSize;

  while(count) {
    CHK(AaffReadPage((t_pAaff)p_handle,page,&p_page_buffer,&page_len))
    to_copy=GETMIN(page_len-ofs,count);
    memcpy(p_buf,p_page_buffer+ofs,to_copy);
    count-=to_copy;
    p_buf+=to_copy;
    ofs=0;
    page++;
  }

  return AAFF_OK;
}

/*
 * AaffOptionsHelp
 */
const char* AaffOptionsHelp() {
  return NULL;
}

/*
 * AaffOptionsParse
 */
int AaffOptionsParse(void *p_handle, char *p_options, char **pp_error) {
  return AAFF_OK;
}

/*
 * AaffFreeBuffer
 */
void AaffFreeBuffer(void *p_buf) {
  free(p_buf);
}

// -----------------------------------------------------
//              Small main routine for testing
//              It converts an aff file to dd
// -----------------------------------------------------

#ifdef AAFF_MAIN_FOR_TESTING

int main(int argc, char *argv[])
{
   t_pAaff   pAaff;
   char     *pInfoBuff;
   uint64_t   Remaining;
   uint64_t   CurrentPos=0;
   int        rc;
   int        Percent;
   int        PercentOld;

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
   rc = AaffOpen (&pAaff, argv[1], 1);
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

