/*******************************************************************************
* xmount Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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
#include <string.h>
#include <fcntl.h> // For O_RDONLY

#include "../libxmount_input.h"

#ifndef HAVE_LIBAFF_STATIC
  #include <afflib/afflib.h>
#else
  #include "libaff/lib/afflib.h"
#endif

#include "libxmount_input_aff.h"

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
  return "aff\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
  p_functions->CreateHandle=&AffCreateHandle;
  p_functions->DestroyHandle=&AffDestroyHandle;
  p_functions->Open=&AffOpen;
  p_functions->Close=&AffClose;
  p_functions->Size=&AffSize;
  p_functions->Read=&AffRead;
  p_functions->OptionsHelp=&AffOptionsHelp;
  p_functions->OptionsParse=&AffOptionsParse;
  p_functions->GetInfofileContent=&AffGetInfofileContent;
  p_functions->GetErrorMessage=&AffGetErrorMessage;
  p_functions->FreeBuffer=&AffFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * AffCreateHandle
 */
static int AffCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug)
{
  (void)p_format;
  pts_AffHandle p_aff_handle;

  // Alloc new lib handle
  p_aff_handle=(pts_AffHandle)malloc(sizeof(ts_AffHandle));
  if(p_aff_handle==NULL) return AFF_MEMALLOC_FAILED;

  // Init lib handle  
  p_aff_handle->h_aff=NULL;

  *pp_handle=p_aff_handle;
  return AFF_OK;
}

/*
 * AffDestroyHandle
 */
static int AffDestroyHandle(void **pp_handle) {
  pts_AffHandle p_aff_handle=(pts_AffHandle)*pp_handle;

  // Free lib handle
  if(p_aff_handle!=NULL) free(p_aff_handle);

  *pp_handle=NULL;
  return AFF_OK;
}

/*
 * AffOpen
 */
static int AffOpen(void *p_handle,
                   const char **pp_filename_arr,
                   uint64_t filename_arr_len)
{
  pts_AffHandle p_aff_handle=(pts_AffHandle)p_handle;

  // We need exactly one file
  if(filename_arr_len==0) return AFF_NO_INPUT_FILES;
  if(filename_arr_len>1) return AFF_TOO_MANY_INPUT_FILES;

  // Open AFF file
  p_aff_handle->h_aff=af_open(pp_filename_arr[0],O_RDONLY,0);
  if(p_aff_handle->h_aff==NULL) {
    // LOG_ERROR("Couldn't open AFF file!\n")
    return AFF_OPEN_FAILED;
  }

  // Encrypted images aren't supported for now
  // TODO: Add support trough lib params, f. ex. aff_password=xxxx
  if(af_cannot_decrypt(p_aff_handle->h_aff)) {
    af_close(p_aff_handle->h_aff);
    return AFF_ENCRYPTION_UNSUPPORTED;
  }

  return AFF_OK;
}

/*
 * AffClose
 */
static int AffClose(void *p_handle) {
  pts_AffHandle p_aff_handle=(pts_AffHandle)p_handle;

  // Close AFF handle
  if(af_close(p_aff_handle->h_aff)!=0) return AFF_CLOSE_FAILED;

  return AFF_OK;
}

/*
 * AffSize
 */
static int AffSize(void *p_handle, uint64_t *p_size) {
  pts_AffHandle p_aff_handle=(pts_AffHandle)p_handle;

  // TODO: Check for error. Unfortunately, I don't know how :(
  *p_size=af_seek(p_aff_handle->h_aff,0,SEEK_END);

  return AFF_OK;
}

/*
 * AffRead
 */
static int AffRead(void *p_handle,
                   char *p_buf,
                   off_t offset,
                   size_t count,
                   size_t *p_read,
                   int *p_errno)
{
  pts_AffHandle p_aff_handle=(pts_AffHandle)p_handle;
  size_t bytes_read;

  // Seek to requested position
  if(af_seek(p_aff_handle->h_aff,offset,SEEK_SET)!=offset) {
    return AFF_SEEK_FAILED;
  }

  // Read data
  // TODO: Check for errors
  bytes_read=af_read(p_aff_handle->h_aff,(unsigned char*)p_buf,count);
  if(bytes_read!=count) return AFF_READ_FAILED;

  *p_read=bytes_read;
  return AFF_OK;
}

/*
 * AffOptionsHelp
 */
static int AffOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return AFF_OK;
}

/*
 * AffOptionsParse
 */
static int AffOptionsParse(void *p_handle,
                           uint32_t options_count,
                           const pts_LibXmountOptions *pp_options,
                           const char **pp_error)
{
  return AFF_OK;
}

/*
 * AffGetInfofileContent
 */
static int AffGetInfofileContent(void *p_handle, const char **pp_info_buf) {
  // TODO
  *pp_info_buf=NULL;
  return AFF_OK;
}

/*
 * AffGetErrorMessage
 */
static const char* AffGetErrorMessage(int err_num) {
  switch(err_num) {
    case AFF_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case AFF_NO_INPUT_FILES:
      return "No input file(s) specified";
      break;
    case AFF_TOO_MANY_INPUT_FILES:
      return "Too many input files specified";
      break;
    case AFF_OPEN_FAILED:
      return "Unable to open EWF file(s)";
      break;
    case AFF_CLOSE_FAILED:
      return "Unable to close EWF file(s)";
      break;
    case AFF_ENCRYPTION_UNSUPPORTED:
      return "Encrypted AFF files are currently not supported";
      break;
    case AFF_SEEK_FAILED:
      return "Unable to seek into EWF data";
      break;
    case AFF_READ_FAILED:
      return "Unable to read EWF data";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * AffFreeBuffer
 */
static int AffFreeBuffer(void *p_buf) {
  free(p_buf);
  return AFF_OK;
}

/*
  ----- Change log -----
  20140724: * Initial version implementing AffOpen, AffSize, AffRead, AffClose,
              AffOptionsHelp, AffOptionsParse and AffFreeBuffer
  20140804: * Added error handling and AffGetErrorMessage
*/

