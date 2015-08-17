/*******************************************************************************
* xmount Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* Mostly based upon code written and copyright 2014 by Guy Voncken.            *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../libxmount_input.h"
#include "libxmount_input_raw.h"

#define LOG_WARNING(...) {            \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
}

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
  return "raw\0dd\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
  p_functions->CreateHandle=&RawCreateHandle;
  p_functions->DestroyHandle=&RawDestroyHandle;
  p_functions->Open=&RawOpen;
  p_functions->Close=&RawClose;
  p_functions->Size=&RawSize;
  p_functions->Read=&RawRead;
  p_functions->OptionsHelp=&RawOptionsHelp;
  p_functions->OptionsParse=&RawOptionsParse;
  p_functions->GetInfofileContent=&RawGetInfofileContent;
  p_functions->GetErrorMessage=&RawGetErrorMessage;
  p_functions->FreeBuffer=&RawFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

// ---------------------------
//  Internal static functions
// ---------------------------

static inline uint64_t RawGetCurrentSeekPos(t_pPiece pPiece) {
  return ftello(pPiece->pFile);
}

static inline int RawSetCurrentSeekPos(t_pPiece pPiece,
                                       uint64_t Val,
                                       int Whence)
{
  if (fseeko (pPiece->pFile, Val, Whence) != 0) return RAW_CANNOT_SEEK;
  return RAW_OK;
}

static int RawRead0(t_praw praw, char *pBuffer, uint64_t Seek, uint32_t *pCount)
{
  t_pPiece pPiece;
  uint64_t  i;

  // Find correct piece to read from
  // -------------------------------

  for (i=0; i<praw->Pieces; i++)
  {
    pPiece = &praw->pPieceArr[i];
    if (Seek < pPiece->FileSize) break;
    Seek -= pPiece->FileSize;
  }
  if (i >= praw->Pieces) return RAW_READ_BEYOND_END_OF_IMAGE;

  // Read from this piece
  // --------------------
  CHK (RawSetCurrentSeekPos (pPiece, Seek, SEEK_SET))

  *pCount = GETMIN (*pCount, pPiece->FileSize - Seek);

  if (fread (pBuffer, *pCount, 1, pPiece->pFile) != 1)
  {
    return RAW_CANNOT_READ_DATA;
  }

  return RAW_OK;
}

// ---------------
//  API functions
// ---------------

/*
 * RawCreateHandle
 */
static int RawCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug)
{
  (void)p_format;
  t_praw p_raw=NULL;

  p_raw=(t_praw)malloc(sizeof(t_raw));
  if(p_raw==NULL) return RAW_MEMALLOC_FAILED;

  memset(p_raw,0,sizeof(t_raw));

  if(strcmp(p_format,"dd")==0) {
    LOG_WARNING("Using '--in dd' is deprecated and will be removed in the next "
                  "release. Please use '--in raw' instead.\n");
  }

  *pp_handle=p_raw;
  return RAW_OK;
}

/*
 * RawDestroyHandle
 */
static int RawDestroyHandle(void **pp_handle) {
  free(*pp_handle);
  *pp_handle=NULL;
  return RAW_OK;
}

/*
 * RawOpen
 */
static int RawOpen(void *p_handle,
                   const char **pp_filename_arr,
                   uint64_t filename_arr_len)
{
  t_praw praw=(t_praw)p_handle;
  t_pPiece pPiece;

  praw->Pieces    = filename_arr_len;
  praw->pPieceArr = (t_pPiece) malloc (praw->Pieces * sizeof(t_Piece));
  if (praw->pPieceArr == NULL) return RAW_MEMALLOC_FAILED;
  // Need to set everything to 0 in case an error occurs later and RawClose is
  // called
  memset(praw->pPieceArr,0,praw->Pieces * sizeof(t_Piece));

  praw->TotalSize = 0;
  for (uint64_t i=0; i < praw->Pieces; i++) 
  {
    pPiece = &praw->pPieceArr[i];
    pPiece->pFilename = strdup (pp_filename_arr[i]);
    if (pPiece->pFilename == NULL)
    {
      RawClose(p_handle);
      return RAW_MEMALLOC_FAILED;
    }
    pPiece->pFile = fopen (pPiece->pFilename, "r");
    if (pPiece->pFile == NULL)
    {
      RawClose(p_handle);
      return RAW_FILE_OPEN_FAILED;
    }
    CHK(RawSetCurrentSeekPos(pPiece, 0, SEEK_END))
    pPiece->FileSize = RawGetCurrentSeekPos (pPiece);
    praw->TotalSize  += pPiece->FileSize;
  }

  return RAW_OK;
}

/*
 * RawClose
 */
static int RawClose(void *p_handle) {
  t_praw    praw = (t_praw)p_handle;
  t_pPiece pPiece;
  int       CloseErrors = 0;

  if (praw->pPieceArr)
  {
    for (uint64_t i=0; i < praw->Pieces; i++)
    {
      pPiece = &praw->pPieceArr[i];
      if (pPiece->pFile) {
        if (fclose (pPiece->pFile)) CloseErrors=1;
      }
      if (pPiece->pFilename) free (pPiece->pFilename);
    }
    free (praw->pPieceArr);
  }

  if (CloseErrors) return RAW_CANNOT_CLOSE_FILE;

  return RAW_OK;
}

/*
 * RawSize
 */
static int RawSize(void *p_handle, uint64_t *p_size) {
  t_praw p_raw_handle=(t_praw)p_handle;

  *p_size=p_raw_handle->TotalSize;
  return RAW_OK;
}

/*
 * RawRead
 */
static int RawRead(void *p_handle,
                   char *p_buf,
                   off_t seek,
                   size_t count,
                   size_t *p_read,
                   int *p_errno)
{
  t_praw p_raw_handle=(t_praw)p_handle;
  uint32_t remaining=count;
  uint32_t to_read;

  if((seek+count)>p_raw_handle->TotalSize) {
    return RAW_READ_BEYOND_END_OF_IMAGE;
  }

  do {
    to_read=remaining;
    CHK(RawRead0(p_raw_handle,p_buf,seek,&to_read))
    remaining-=to_read;
    p_buf+=to_read;
    seek+=to_read;
  } while(remaining);

  *p_read=count;
  return RAW_OK;
}

/*
 * RawOptionsHelp
 */
static int RawOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return RAW_OK;
}

/*
 * RawOptionsParse
 */
static int RawOptionsParse(void *p_handle,
                           uint32_t options_count,
                           const pts_LibXmountOptions *pp_options,
                           const char **pp_error)
{
  return RAW_OK;
}

/*
 * RawGetInfofileContent
 */
static int RawGetInfofileContent(void *p_handle, const char **pp_info_buf) {
  t_praw p_raw_handle=(t_praw)p_handle;
  int ret;
  char *p_info_buf;

  ret=asprintf(&p_info_buf,
               "RAW image assembled of %" PRIu64 " piece(s)\n"
                 "%" PRIu64 " bytes in total (%0.3f GiB)\n",
               p_raw_handle->Pieces,
               p_raw_handle->TotalSize,
               p_raw_handle->TotalSize/(1024.0*1024.0*1024.0));
  if(ret<0 || *pp_info_buf==NULL) return RAW_MEMALLOC_FAILED;

  *pp_info_buf=p_info_buf;
  return RAW_OK;
}

/*
 * RawGetErrorMessage
 */
static const char* RawGetErrorMessage(int err_num) {
  switch(err_num) {
    case RAW_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case RAW_FILE_OPEN_FAILED:
      return "Unable to open raw file(s)";
      break;
    case RAW_CANNOT_READ_DATA:
      return "Unable to read raw data";
      break;
    case RAW_CANNOT_CLOSE_FILE:
      return "Unable to close raw file(s)";
      break;
    case RAW_CANNOT_SEEK:
      return "Unable to seek into raw data";
      break;
    case RAW_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read raw data: Attempt to read past EOF";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * RawFreeBuffer
 */
static int RawFreeBuffer(void *p_buf) {
  free(p_buf);
  return RAW_OK;
}

