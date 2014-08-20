/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#include "../libxmount_input.h"

#ifndef HAVE_LIBEWF_STATIC
  #include <libewf.h>
#else
  #include "libewf/include/libewf.h"
#endif

#include "libxmount_input_ewf.h"

#ifndef LIBEWF_HANDLE
  // libewf version 2 no longer defines LIBEWF_HANDLE
  #define HAVE_LIBEWF_V2_API
#endif

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
  return "ewf\0\0";
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions *p_functions) {
  p_functions->CreateHandle=&EwfCreateHandle;
  p_functions->DestroyHandle=&EwfDestroyHandle;
  p_functions->Open=&EwfOpen;
  p_functions->Size=&EwfSize;
  p_functions->Read=&EwfRead;
  p_functions->Close=&EwfClose;
  p_functions->OptionsHelp=&EwfOptionsHelp;
  p_functions->OptionsParse=&EwfOptionsParse;
  p_functions->GetInfofileContent=&EwfGetInfofileContent;
  p_functions->GetErrorMessage=&EwfGetErrorMessage;
  p_functions->FreeBuffer=&EwfFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * EwfCreateHandle
 */
static int EwfCreateHandle(void **pp_handle, char *p_format) {
  (void)p_format;
  *pp_handle=NULL;

#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_initialize((libewf_handle_t**)pp_handle,NULL)!=1) {
    return EWF_HANDLE_CREATION_FAILED;
  }
#endif

  return EWF_OK;
}

/*
 * EwfDestroyHandle
 */
static int EwfDestroyHandle(void **pp_handle) {
#ifdef HAVE_LIBEWF_V2_API
  // Free EWF handle
  if(libewf_handle_free((libewf_handle_t**)pp_handle,NULL)!=1) {
    return EWF_HANDLE_DESTRUCTION_FAILED;
  }
#endif

  *pp_handle=NULL;
  return EWF_OK;
}

/*
 * EwfOpen
 */
static int EwfOpen(void **pp_handle,
                   const char **pp_filename_arr,
                   uint64_t filename_arr_len)
{
  // We need at least one file
  if(filename_arr_len==0) return EWF_NO_INPUT_FILES;

  // Make sure all files are EWF files
  for(uint64_t i=0;i<filename_arr_len;i++) {
#ifdef HAVE_LIBEWF_V2_API
    if(libewf_check_file_signature(pp_filename_arr[i],NULL)!=1)
#else
    if(libewf_check_file_signature(pp_filename_arr[i])!=1)
#endif
    {
      return EWF_INVALID_INPUT_FILES;
    }
  }

  // Open EWF file
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_open((libewf_handle_t*)*pp_handle,
                        (char* const*)pp_filename_arr,
                        filename_arr_len,
                        libewf_get_access_flags_read(),
                        NULL)!=1)
#else
  *pp_handle=(void*)libewf_open((char* const*)pp_filename_arr,
                                filename_arr_len,
                                libewf_get_flags_read());
  if(*pp_handle==NULL)
#endif
  {
    return EWF_OPEN_FAILED;
  }

#ifndef HAVE_LIBEWF_V2_API
  // Parse EWF header
  if(libewf_parse_header_values((LIBEWF_HANDLE*)*pp_handle,
                                LIBEWF_DATE_FORMAT_ISO8601)!=1)
  {
    return EWF_HEADER_PARSING_FAILED;
  }
#endif

  return EWF_OK;
}

/*
 * EwfClose
 */
static int EwfClose(void **pp_handle) {
  // Close EWF handle
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_close((libewf_handle_t*)*pp_handle,NULL)!=0)
#else
  if(libewf_close((LIBEWF_HANDLE*)*pp_handle)!=0)
#endif
  {
    return EWF_CLOSE_FAILED;
  }

  return EWF_OK;
}

/*
 * EwfSize
 */
static int EwfSize(void *p_handle, uint64_t *p_size) {
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_get_media_size((libewf_handle_t*)p_handle,p_size,NULL)!=1) {
#else
  if(libewf_get_media_size((LIBEWF_HANDLE*)p_handle,p_size)!=1) {
#endif
    return EWF_GET_SIZE_FAILED;
  }
  return EWF_OK;
}

/*
 * EwfRead
 */
static int EwfRead(void *p_handle,
                   uint64_t offset,
                   char *p_buf,
                   uint32_t count)
{
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_seek_offset((libewf_handle_t*)p_handle,
                               offset,
                               SEEK_SET,
                               NULL)!=-1)
#else
  if(libewf_seek_offset((LIBEWF_HANDLE*)p_handle,offset)!=-1)
#endif
  {
#ifdef HAVE_LIBEWF_V2_API
    if(libewf_handle_read_buffer((libewf_handle_t*)p_handle,
                                 p_buf,
                                 count,
                                 NULL)!=count)
#else
  
    if(libewf_read_buffer((LIBEWF_HANDLE*)p_handle,p_buf,count)!=count)
#endif
    {
      return EWF_READ_FAILED;
    }
  } else {
    return EWF_SEEK_FAILED;
  }
  return EWF_OK;
}

/*
 * EwfOptionsHelp
 */
static const char* EwfOptionsHelp() {
  return NULL;
}

/*
 * EwfOptionsParse
 */
static int EwfOptionsParse(void *p_handle, char *p_options, char **pp_error) {
  return EWF_OK;
}

/*
 * EwfGetInfofileContent
 */
static int EwfGetInfofileContent(void *p_handle, char **pp_info_buf) {
  int ret;
  char buf[512];
#ifdef HAVE_LIBEWF_V2_API
  libewf_handle_t *p_ewf=(libewf_handle_t*)p_handle;
#else
  LIBEWF_HANDLE *p_ewf=(LIBEWF_HANDLE*)p_handle;
#endif
  *pp_info_buf=NULL;

#define M_SAVE_VALUE(desc) {                                          \
  if(ret==1) {                                                        \
    if(*pp_info_buf!=NULL) {                                          \
      *pp_info_buf=(char*)realloc(*pp_info_buf,                       \
                                  strlen(*pp_info_buf)+strlen(desc)+  \
                                    strlen(buf)+2);                   \
      if(*pp_info_buf==NULL) return EWF_MEMALLOC_FAILED;              \
      strncpy(*pp_info_buf+strlen(*pp_info_buf),desc,strlen(desc)+1); \
    } else {                                                          \
      *pp_info_buf=(char*)malloc(strlen(desc)+strlen(buf)+2);         \
      if(*pp_info_buf==NULL) return EWF_MEMALLOC_FAILED;              \
      strncpy(*pp_info_buf,desc,strlen(desc)+1);                      \
    }                                                                 \
    strncpy(*pp_info_buf+strlen(*pp_info_buf),buf,strlen(buf)+1);     \
    strncpy(*pp_info_buf+strlen(*pp_info_buf),"\n",2);                \
  }                                                                   \
}

#ifdef HAVE_LIBEWF_V2_API
  ret=libewf_handle_get_utf8_header_value_case_number(p_ewf,
                                                      (uint8_t*)buf,
                                                      sizeof(buf),
                                                      NULL);
  M_SAVE_VALUE("Case number: ");
  ret=libewf_handle_get_utf8_header_value_description(p_ewf,
                                                      (uint8_t*)buf,
                                                      sizeof(buf),
                                                      NULL);
  M_SAVE_VALUE("Description: ");
  ret=libewf_handle_get_utf8_header_value_examiner_name(p_ewf,
                                                        (uint8_t*)buf,
                                                        sizeof(buf),
                                                        NULL);
  M_SAVE_VALUE("Examiner: ");
  ret=libewf_handle_get_utf8_header_value_evidence_number(p_ewf,
                                                          (uint8_t*)buf,
                                                          sizeof(buf),
                                                          NULL);
  M_SAVE_VALUE("Evidence number: ");
  ret=libewf_handle_get_utf8_header_value_notes(p_ewf,
                                                (uint8_t*)buf,
                                                sizeof(buf),
                                                NULL);
  M_SAVE_VALUE("Notes: ");
  ret=libewf_handle_get_utf8_header_value_acquiry_date(p_ewf,
                                                       (uint8_t*)buf,
                                                       sizeof(buf),
                                                       NULL);
  M_SAVE_VALUE("Acquiry date: ");
  ret=libewf_handle_get_utf8_header_value_system_date(p_ewf,
                                                      (uint8_t*)buf,
                                                      sizeof(buf),
                                                      NULL);
  M_SAVE_VALUE("System date: ");
  ret=
    libewf_handle_get_utf8_header_value_acquiry_operating_system(p_ewf,
                                                                 (uint8_t*)buf,
                                                                 sizeof(buf),
                                                                 NULL);
  M_SAVE_VALUE("Acquiry os: ");
  ret=
    libewf_handle_get_utf8_header_value_acquiry_software_version(p_ewf,
                                                                 (uint8_t*)buf,
                                                                 sizeof(buf),
                                                                 NULL);
  M_SAVE_VALUE("Acquiry sw version: ");
  ret=libewf_handle_get_utf8_hash_value_md5(p_ewf,
                                            (uint8_t*)buf,
                                            sizeof(buf),
                                            NULL);
  M_SAVE_VALUE("MD5 hash: ");
  ret=libewf_handle_get_utf8_hash_value_sha1(p_ewf,
                                             (uint8_t*)buf,
                                             sizeof(buf),
                                             NULL);
  M_SAVE_VALUE("SHA1 hash: ");
#else
  ret=libewf_get_header_value_case_number(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Case number: ");
  ret=libewf_get_header_value_description(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Description: ");
  ret=libewf_get_header_value_examiner_name(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Examiner: ");
  ret=libewf_get_header_value_evidence_number(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Evidence number: ");
  ret=libewf_get_header_value_notes(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Notes: ");
  ret=libewf_get_header_value_acquiry_date(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Acquiry date: ");
  ret=libewf_get_header_value_system_date(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("System date: ");
  ret=libewf_get_header_value_acquiry_operating_system(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Acquiry os: ");
  ret=libewf_get_header_value_acquiry_software_version(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("Acquiry sw version: ");
  ret=libewf_get_hash_value_md5(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("MD5 hash: ");
  ret=libewf_get_hash_value_sha1(p_ewf,buf,sizeof(buf));
  M_SAVE_VALUE("SHA1 hash: ");
#endif

#undef M_SAVE_VALUE

  return EWF_OK;
}

/*
 * EwfGetErrorMessage
 */
static const char* EwfGetErrorMessage(int err_num) {
  switch(err_num) {
    case EWF_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case EWF_HANDLE_CREATION_FAILED:
      return "Unable to create EWF handle";
      break;
    case EWF_HANDLE_DESTRUCTION_FAILED:
      return "Unable to destroy EWF handle";
      break;
    case EWF_NO_INPUT_FILES:
      return "No input file(s) specified";
      break;
    case EWF_INVALID_INPUT_FILES:
      return "The specified input file(s) are not valid EWF files";
      break;
    case EWF_OPEN_FAILED:
      return "Unable to open EWF file(s)";
      break;
    case EWF_HEADER_PARSING_FAILED:
      return "Unable to parse EWF header values";
      break;
    case EWF_CLOSE_FAILED:
      return "Unable to close EWF file(s)";
      break;
    case EWF_GET_SIZE_FAILED:
      return "Unable to get EWF data size";
      break;
    case EWF_SEEK_FAILED:
      return "Unable to seek into EWF data";
      break;
    case EWF_READ_FAILED:
      return "Unable to read EWF data";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * EwfFreeBuffer
 */
static void EwfFreeBuffer(void *p_buf) {
  free(p_buf);
}

/*
  ----- Change log -----
  20140724: * Initial version implementing EwfOpen, EwfSize, EwfRead, EwfClose,
              EwfOptionsHelp, EwfOptionsParse and EwfFreeBuffer.
  20140731: * Added support for libewf v1 API.
  20140803: * Added EwfCreateHandle, EwfDestroyHandle and EwfGetInfofileContent.
*/

