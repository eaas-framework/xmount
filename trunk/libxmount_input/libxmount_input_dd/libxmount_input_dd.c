/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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
#include "libxmount_input_dd.h"

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
  return "dd\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
  p_functions->CreateHandle=&DdCreateHandle;
  p_functions->DestroyHandle=&DdDestroyHandle;
  p_functions->Open=&DdOpen;
  p_functions->Close=&DdClose;
  p_functions->Size=&DdSize;
  p_functions->Read=&DdRead;
  p_functions->OptionsHelp=&DdOptionsHelp;
  p_functions->OptionsParse=&DdOptionsParse;
  p_functions->GetInfofileContent=&DdGetInfofileContent;
  p_functions->GetErrorMessage=&DdGetErrorMessage;
  p_functions->FreeBuffer=&DdFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/

// ---------------------------
//  Internal static functions
// ---------------------------

static inline uint64_t DdGetCurrentSeekPos (t_pPiece pPiece)
{
  return ftello (pPiece->pFile);
}

static inline int DdSetCurrentSeekPos (t_pPiece pPiece,
                                       uint64_t Val,
                                       int Whence)
{
  if (fseeko (pPiece->pFile, Val, Whence) != 0) return DD_CANNOT_SEEK;
  return DD_OK;
}

static int DdRead0 (t_pdd pdd, char *pBuffer, uint64_t Seek, uint32_t *pCount)
{
  t_pPiece pPiece;
  uint64_t  i;

  // Find correct piece to read from
  // -------------------------------

  for (i=0; i<pdd->Pieces; i++)
  {
    pPiece = &pdd->pPieceArr[i];
    if (Seek < pPiece->FileSize) break;
    Seek -= pPiece->FileSize;
  }
  if (i >= pdd->Pieces) return DD_READ_BEYOND_END_OF_IMAGE;

  // Read from this piece
  // --------------------
  CHK (DdSetCurrentSeekPos (pPiece, Seek, SEEK_SET))

  *pCount = GETMIN (*pCount, pPiece->FileSize - Seek);

  if (fread (pBuffer, *pCount, 1, pPiece->pFile) != 1)
  {
    return DD_CANNOT_READ_DATA;
  }

  return DD_OK;
}

// ---------------
//  API functions
// ---------------

/*
 * DdCreateHandle
 */
static int DdCreateHandle(void **pp_handle,
                          const char *p_format,
                          uint8_t debug)
{
  (void)p_format;
  t_pdd p_dd=NULL;

  p_dd=(t_pdd)malloc(sizeof(t_dd));
  if(p_dd==NULL) return DD_MEMALLOC_FAILED;

  memset(p_dd,0,sizeof(t_dd));

  *pp_handle=p_dd;
  return DD_OK;
}

/*
 * DdDestroyHandle
 */
static int DdDestroyHandle(void **pp_handle) {
  free(*pp_handle);
  *pp_handle=NULL;
  return DD_OK;
}

/*
 * DdOpen
 */
static int DdOpen(void *p_handle,
                  const char **pp_filename_arr,
                  uint64_t filename_arr_len)
{
  t_pdd pdd=(t_pdd)p_handle;
  t_pPiece pPiece;

  pdd->Pieces    = filename_arr_len;
  pdd->pPieceArr = (t_pPiece) malloc (pdd->Pieces * sizeof(t_Piece));
  if (pdd->pPieceArr == NULL) return DD_MEMALLOC_FAILED;
  // Need to set everything to 0 in case an error occurs later and DdClose is
  // called
  memset(pdd->pPieceArr,0,pdd->Pieces * sizeof(t_Piece));

  pdd->TotalSize = 0;
  for (uint64_t i=0; i < pdd->Pieces; i++) 
  {
    pPiece = &pdd->pPieceArr[i];
    pPiece->pFilename = strdup (pp_filename_arr[i]);
    if (pPiece->pFilename == NULL)
    {
      DdClose(p_handle);
      return DD_MEMALLOC_FAILED;
    }
    pPiece->pFile = fopen (pPiece->pFilename, "r");
    if (pPiece->pFile == NULL)
    {
      DdClose(p_handle);
      return DD_FILE_OPEN_FAILED;
    }
    CHK(DdSetCurrentSeekPos(pPiece, 0, SEEK_END))
    pPiece->FileSize = DdGetCurrentSeekPos (pPiece);
    pdd->TotalSize  += pPiece->FileSize;
  }

  return DD_OK;
}

/*
 * DdClose
 */
static int DdClose(void *p_handle) {
  t_pdd    pdd = (t_pdd)p_handle;
  t_pPiece pPiece;
  int       CloseErrors = 0;

  if (pdd->pPieceArr)
  {
    for (uint64_t i=0; i < pdd->Pieces; i++)
    {
      pPiece = &pdd->pPieceArr[i];
      if (pPiece->pFile) {
        if (fclose (pPiece->pFile)) CloseErrors=1;
      }
      if (pPiece->pFilename) free (pPiece->pFilename);
    }
    free (pdd->pPieceArr);
  }

  if (CloseErrors) return DD_CANNOT_CLOSE_FILE;

  return DD_OK;
}

/*
 * DdSize
 */
static int DdSize(void *p_handle, uint64_t *p_size) {
  t_pdd p_dd_handle=(t_pdd)p_handle;

  *p_size=p_dd_handle->TotalSize;
  return DD_OK;
}

/*
 * DdRead
 */
static int DdRead(void *p_handle,
                  char *p_buf,
                  off_t seek,
                  size_t count,
                  size_t *p_read,
                  int *p_errno)
{
  t_pdd p_dd_handle=(t_pdd)p_handle;
  uint32_t remaining=count;
  uint32_t to_read;

  if((seek+count)>p_dd_handle->TotalSize) {
    return DD_READ_BEYOND_END_OF_IMAGE;
  }

  do {
    to_read=remaining;
    CHK(DdRead0(p_dd_handle,p_buf,seek,&to_read))
    remaining-=to_read;
    p_buf+=to_read;
    seek+=to_read;
  } while(remaining);

  *p_read=count;
  return DD_OK;
}

/*
 * DdOptionsHelp
 */
static int DdOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return DD_OK;
}

/*
 * DdOptionsParse
 */
static int DdOptionsParse(void *p_handle,
                          uint32_t options_count,
                          const pts_LibXmountOptions *pp_options,
                          const char **pp_error)
{
  return DD_OK;
}

/*
 * DdGetInfofileContent
 */
static int DdGetInfofileContent(void *p_handle, const char **pp_info_buf) {
  t_pdd p_dd_handle=(t_pdd)p_handle;
  int ret;
  char *p_info_buf;

  ret=asprintf(&p_info_buf,
               "DD image assembled of %" PRIu64 " piece(s)\n"
                 "%" PRIu64 " bytes in total (%0.3f GiB)\n",
               p_dd_handle->Pieces,
               p_dd_handle->TotalSize,
               p_dd_handle->TotalSize/(1024.0*1024.0*1024.0));
  if(ret<0 || *pp_info_buf==NULL) return DD_MEMALLOC_FAILED;

  *pp_info_buf=p_info_buf;
  return DD_OK;
}

/*
 * DdGetErrorMessage
 */
static const char* DdGetErrorMessage(int err_num) {
  switch(err_num) {
    case DD_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case DD_FILE_OPEN_FAILED:
      return "Unable to open DD file(s)";
      break;
    case DD_CANNOT_READ_DATA:
      return "Unable to read DD data";
      break;
    case DD_CANNOT_CLOSE_FILE:
      return "Unable to close DD file(s)";
      break;
    case DD_CANNOT_SEEK:
      return "Unable to seek into DD data";
      break;
    case DD_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read DD data: Attempt to read past EOF";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * DdFreeBuffer
 */
static int DdFreeBuffer(void *p_buf) {
  free(p_buf);
  return DD_OK;
}

