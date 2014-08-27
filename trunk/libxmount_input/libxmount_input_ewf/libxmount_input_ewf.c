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
                   char *p_buf,
                   off_t offset,
                   size_t count,
                   size_t *p_read)
{
  size_t bytes_read;

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
    bytes_read=libewf_handle_read_buffer((libewf_handle_t*)p_handle,
                                         p_buf,
                                         count,
                                         NULL);
#else
    bytes_read=libewf_read_buffer((LIBEWF_HANDLE*)p_handle,p_buf,count);
#endif
    if(bytes_read!=count) return EWF_READ_FAILED;
  } else {
    return EWF_SEEK_FAILED;
  }

  *p_read=bytes_read;
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
static int EwfOptionsParse(void *p_handle,
                           uint32_t options_count,
                           pts_LibXmountOptions *pp_options,
                           char **pp_error)
{
  return EWF_OK;
}

/*
 * EwfGetInfofileContent
 */
static int EwfGetInfofileContent(void *p_handle, char **pp_info_buf) {
  char *p_infobuf=NULL;
  int ret;
  char buf[512];
  uint8_t uint8value;
  uint32_t uint32value;
  uint64_t uint64value;
#ifdef HAVE_LIBEWF_V2_API
  libewf_handle_t *p_ewf=(libewf_handle_t*)p_handle;
#else
  LIBEWF_HANDLE *p_ewf=(LIBEWF_HANDLE*)p_handle;
#endif

#define EWF_INFOBUF_REALLOC(size) {               \
  p_infobuf=(char*)realloc(p_infobuf,size);       \
  if(p_infobuf==NULL) return EWF_MEMALLOC_FAILED; \
}
#define EWF_INFOBUF_APPEND_STR(str) {                     \
  if(p_infobuf!=NULL) {                                   \
    EWF_INFOBUF_REALLOC(strlen(p_infobuf)+strlen(str)+1); \
    strcpy(p_infobuf+strlen(p_infobuf),str);              \
  } else {                                                \
    EWF_INFOBUF_REALLOC(strlen(str)+1);                   \
    strcpy(p_infobuf,str);                                \
  }                                                       \
}
#define EWF_INFOBUF_APPEND_VALUE(desc) { \
  if(ret==1) {                           \
    EWF_INFOBUF_APPEND_STR(desc);        \
    EWF_INFOBUF_APPEND_STR(buf);         \
    EWF_INFOBUF_APPEND_STR("\n");        \
  }                                      \
}

  EWF_INFOBUF_APPEND_STR("_Acquiry information_\n");

#ifdef HAVE_LIBEWF_V2_API
  #define EWF_GET_HEADER_VALUE(fun) {              \
    ret=fun(p_ewf,(uint8_t*)buf,sizeof(buf),NULL); \
  }

  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_case_number);
  EWF_INFOBUF_APPEND_VALUE("Case number: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_description);
  EWF_INFOBUF_APPEND_VALUE("Description: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_examiner_name);
  EWF_INFOBUF_APPEND_VALUE("Examiner: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_evidence_number);
  EWF_INFOBUF_APPEND_VALUE("Evidence number: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_notes);
  EWF_INFOBUF_APPEND_VALUE("Notes: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_acquiry_date);
  EWF_INFOBUF_APPEND_VALUE("Acquiry date: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_system_date);
  EWF_INFOBUF_APPEND_VALUE("System date: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_acquiry_operating_system);
  EWF_INFOBUF_APPEND_VALUE("Acquiry os: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_acquiry_software_version);
  EWF_INFOBUF_APPEND_VALUE("Acquiry sw version: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_model);
  EWF_INFOBUF_APPEND_VALUE("Model: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_header_value_serial_number);
  EWF_INFOBUF_APPEND_VALUE("Serial number: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_hash_value_md5);
  EWF_INFOBUF_APPEND_VALUE("MD5 hash: ");
  EWF_GET_HEADER_VALUE(libewf_handle_get_utf8_hash_value_sha1);
  EWF_INFOBUF_APPEND_VALUE("SHA1 hash: ");

  #undef EWF_GET_HEADER_VALUE
#else
  ret=libewf_get_header_value_case_number(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Case number: ");
  ret=libewf_get_header_value_description(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Description: ");
  ret=libewf_get_header_value_examiner_name(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Examiner: ");
  ret=libewf_get_header_value_evidence_number(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Evidence number: ");
  ret=libewf_get_header_value_notes(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Notes: ");
  ret=libewf_get_header_value_acquiry_date(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Acquiry date: ");
  ret=libewf_get_header_value_system_date(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("System date: ");
  ret=libewf_get_header_value_acquiry_operating_system(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Acquiry os: ");
  ret=libewf_get_header_value_acquiry_software_version(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Acquiry sw version: ");
  ret=libewf_get_header_value_model(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Model: ");
  ret=libewf_get_header_value_serial_number(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("Serial number: ");
  ret=libewf_get_hash_value_md5(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("MD5 hash: ");
  ret=libewf_get_hash_value_sha1(p_ewf,buf,sizeof(buf));
  EWF_INFOBUF_APPEND_VALUE("SHA1 hash: ");
#endif

  EWF_INFOBUF_APPEND_STR("\n_Media information_\n");

#ifdef HAVE_LIBEWF_V2_API
  ret=libewf_handle_get_media_type(p_ewf,&uint8value,NULL);
#else
  ret=libewf_get_media_type(p_ewf,&uint8value);
#endif
  if(ret==1) {
    EWF_INFOBUF_APPEND_STR("Media type: ");
    switch(uint8value) {
      case LIBEWF_MEDIA_TYPE_REMOVABLE:
        EWF_INFOBUF_APPEND_STR("removable disk\n");
        break;
      case LIBEWF_MEDIA_TYPE_FIXED:
        EWF_INFOBUF_APPEND_STR("fixed disk\n");
        break;
      case LIBEWF_MEDIA_TYPE_OPTICAL:
        EWF_INFOBUF_APPEND_STR("optical\n");
        break;
      case LIBEWF_MEDIA_TYPE_SINGLE_FILES:
        EWF_INFOBUF_APPEND_STR("single files\n");
        break;
      case LIBEWF_MEDIA_TYPE_MEMORY:
        EWF_INFOBUF_APPEND_STR("memory\n");
        break;
      default:
        EWF_INFOBUF_APPEND_STR("unknown\n");
    }
  }

#ifdef HAVE_LIBEWF_V2_API
  ret=libewf_handle_get_bytes_per_sector(p_ewf,&uint32value,NULL);
  sprintf(buf,"%" PRIu32,uint32value);
  EWF_INFOBUF_APPEND_VALUE("Bytes per sector: ");
  ret=libewf_handle_get_number_of_sectors(p_ewf,&uint64value,NULL);
  sprintf(buf,"%" PRIu64,uint64value);
  EWF_INFOBUF_APPEND_VALUE("Number of sectors: ");
#else
  ret=libewf_get_bytes_per_sector(p_ewf,&uint32value);
  sprintf(buf,"%" PRIu32,uint32value);
  EWF_INFOBUF_APPEND_VALUE("Bytes per sector: ");
  ret=libewf_handle_get_amount_of_sectors(p_ewf,&uint64value);
  sprintf(buf,"%" PRIu64,uint64value);
  EWF_INFOBUF_APPEND_VALUE("Number of sectors: ");
#endif

#undef EWF_INFOBUF_APPEND_VALUE
#undef EWF_INFOBUF_APPEND_STR
#undef EWF_INFOBUF_REALLOC

  *pp_info_buf=p_infobuf;
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

