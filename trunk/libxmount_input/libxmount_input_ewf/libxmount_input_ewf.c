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

#include "../libxmount_input.h"

#ifndef HAVE_LIBEWF_STATIC
  #include <libewf.h>
#else
  #include "libewf/include/libewf.h"
#endif

#ifndef LIBEWF_HANDLE
  // libewf version 2 no longer defines LIBEWF_HANDLE
  #define HAVE_LIBEWF_V2_API
#endif

#include "libxmount_input_ewf.h"

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
static int EwfCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug)
{
  (void)p_format;
  pts_EwfHandle p_ewf_handle;

  // Alloc new lib handle
  p_ewf_handle=(pts_EwfHandle)malloc(sizeof(ts_EwfHandle));
  if(p_ewf_handle==NULL) return EWF_MEMALLOC_FAILED;

  // Init handle values
  p_ewf_handle->h_ewf=NULL;
  p_ewf_handle->debug=debug;

  // Init lib handle
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_initialize(&(p_ewf_handle->h_ewf),NULL)!=1) {
    return EWF_HANDLE_CREATION_FAILED;
  }
#endif

  *pp_handle=p_ewf_handle;
  return EWF_OK;
}

/*
 * EwfDestroyHandle
 */
static int EwfDestroyHandle(void **pp_handle) {
  pts_EwfHandle p_ewf_handle=(pts_EwfHandle)*pp_handle;
  int ret=EWF_OK;

  if(p_ewf_handle!=NULL) {
    // Free EWF handle
#ifdef HAVE_LIBEWF_V2_API
    if(libewf_handle_free(&(p_ewf_handle->h_ewf),NULL)!=1) {
      ret=EWF_HANDLE_DESTRUCTION_FAILED;
    }
#endif
    // Free lib handle
    free(p_ewf_handle);
    p_ewf_handle=NULL;
  }

  *pp_handle=p_ewf_handle;
  return ret;
}

/*
 * EwfOpen
 */
static int EwfOpen(void *p_handle,
                   const char **pp_filename_arr,
                   uint64_t filename_arr_len)
{
  pts_EwfHandle p_ewf_handle=(pts_EwfHandle)p_handle;

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
  if(libewf_handle_open(p_ewf_handle->h_ewf,
                        (char* const*)pp_filename_arr,
                        filename_arr_len,
                        libewf_get_access_flags_read(),
                        NULL)!=1)
#else
  p_ewf_handle->h_ewf=libewf_open((char* const*)pp_filename_arr,
                                  filename_arr_len,
                                  libewf_get_flags_read());
  if(p_ewf_handle->h_ewf==NULL)
#endif
  {
    return EWF_OPEN_FAILED;
  }

#ifdef HAVE_LIBEWF_V2_API
  // Try to read 1 byte from the image end to verify that all segments were
  // specified (Only needed because libewf_handle_open() won't fail even if not
  // all segments were specified!)
  uint64_t image_size=0;
  char buf;
  if(libewf_handle_get_media_size(p_ewf_handle->h_ewf,&image_size,NULL)!=1) {
    return EWF_GET_SIZE_FAILED;
  }
  if(image_size==0) return EWF_OK;
  LIBXMOUNT_LOG_DEBUG(p_ewf_handle->debug,
                      "Trying to read last byte of image at offset %"
                        PRIu64 " (image size = %" PRIu64 " bytes)\n",
                      image_size-1,
                      image_size);
  if(libewf_handle_seek_offset(p_ewf_handle->h_ewf,
                               image_size-1,
                               SEEK_SET,
                               NULL)==-1)
  {
    return EWF_OPEN_FAILED_SEEK;
  }
  if(libewf_handle_read_buffer(p_ewf_handle->h_ewf,&buf,1,NULL)!=1) {
    return EWF_OPEN_FAILED_READ;
  }
#endif

#ifndef HAVE_LIBEWF_V2_API
  // Parse EWF header
  if(libewf_parse_header_values(p_ewf_handle->h_ewf,
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
static int EwfClose(void *p_handle) {
  pts_EwfHandle p_ewf_handle=(pts_EwfHandle)p_handle;

  // Close EWF handle
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_close(p_ewf_handle->h_ewf,NULL)!=0)
#else
  if(libewf_close(p_ewf_handle->h_ewf)!=0)
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
  pts_EwfHandle p_ewf_handle=(pts_EwfHandle)p_handle;

#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_get_media_size(p_ewf_handle->h_ewf,p_size,NULL)!=1) {
#else
  if(libewf_get_media_size(p_ewf_handle->h_ewf,p_size)!=1) {
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
                   size_t *p_read,
                   int *p_errno)
{
  pts_EwfHandle p_ewf_handle=(pts_EwfHandle)p_handle;
  // TODO: Return value of libewf_handle_read_buffer is ssize_t with -1 on error
  size_t bytes_read;

#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_seek_offset(p_ewf_handle->h_ewf,
                               offset,
                               SEEK_SET,
                               NULL)!=-1)
#else
  if(libewf_seek_offset(p_ewf_handle->h_ewf,offset)!=-1)
#endif
  {
#ifdef HAVE_LIBEWF_V2_API
    bytes_read=libewf_handle_read_buffer(p_ewf_handle->h_ewf,
                                         p_buf,
                                         count,
                                         NULL);
#else
    bytes_read=libewf_read_buffer(p_ewf_handle->h_ewf,p_buf,count);
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
static int EwfOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return EWF_OK;
}

/*
 * EwfOptionsParse
 */
static int EwfOptionsParse(void *p_handle,
                           uint32_t options_count,
                           const pts_LibXmountOptions *pp_options,
                           const char **pp_error)
{
  return EWF_OK;
}

/*
 * EwfGetInfofileContent
 */
static int EwfGetInfofileContent(void *p_handle, const char **pp_info_buf) {
  pts_EwfHandle p_ewf_handle=(pts_EwfHandle)p_handle;
  char *p_infobuf=NULL;
  int ret;
  char buf[512];
  uint8_t uint8value;
  uint32_t uint32value;
  uint64_t uint64value;

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
  #define EWF_GET_HEADER_VALUE(fun) {                            \
    ret=fun(p_ewf_handle->h_ewf,(uint8_t*)buf,sizeof(buf),NULL); \
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
  #define EWF_GET_HEADER_VALUE(fun) {             \
    ret=fun(p_ewf_handle->h_ewf,buf,sizeof(buf)); \
  }

  EWF_GET_HEADER_VALUE(libewf_get_header_value_case_number);
  EWF_INFOBUF_APPEND_VALUE("Case number: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_description);
  EWF_INFOBUF_APPEND_VALUE("Description: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_examiner_name);
  EWF_INFOBUF_APPEND_VALUE("Examiner: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_evidence_number);
  EWF_INFOBUF_APPEND_VALUE("Evidence number: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_notes);
  EWF_INFOBUF_APPEND_VALUE("Notes: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_acquiry_date);
  EWF_INFOBUF_APPEND_VALUE("Acquiry date: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_system_date);
  EWF_INFOBUF_APPEND_VALUE("System date: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_acquiry_operating_system);
  EWF_INFOBUF_APPEND_VALUE("Acquiry os: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_acquiry_software_version);
  EWF_INFOBUF_APPEND_VALUE("Acquiry sw version: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_model);
  EWF_INFOBUF_APPEND_VALUE("Model: ");
  EWF_GET_HEADER_VALUE(libewf_get_header_value_serial_number);
  EWF_INFOBUF_APPEND_VALUE("Serial number: ");
  EWF_GET_HEADER_VALUE(libewf_get_hash_value_md5);
  EWF_INFOBUF_APPEND_VALUE("MD5 hash: ");
  EWF_GET_HEADER_VALUE(libewf_get_hash_value_sha1);
  EWF_INFOBUF_APPEND_VALUE("SHA1 hash: ");

  #undef EWF_GET_HEADER_VALUE
#endif

  EWF_INFOBUF_APPEND_STR("\n_Media information_\n");

#ifdef HAVE_LIBEWF_V2_API
  ret=libewf_handle_get_media_type(p_ewf_handle->h_ewf,&uint8value,NULL);
#else
  ret=libewf_get_media_type(p_ewf_handle->h_ewf,&uint8value);
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
  ret=libewf_handle_get_bytes_per_sector(p_ewf_handle->h_ewf,&uint32value,NULL);
  sprintf(buf,"%" PRIu32,uint32value);
  EWF_INFOBUF_APPEND_VALUE("Bytes per sector: ");
  ret=libewf_handle_get_number_of_sectors(p_ewf_handle->h_ewf,&uint64value,NULL);
  sprintf(buf,"%" PRIu64,uint64value);
  EWF_INFOBUF_APPEND_VALUE("Number of sectors: ");
#else
  ret=libewf_get_bytes_per_sector(p_ewf_handle->h_ewf,&uint32value);
  sprintf(buf,"%" PRIu32,uint32value);
  EWF_INFOBUF_APPEND_VALUE("Bytes per sector: ");
  ret=libewf_handle_get_amount_of_sectors(p_ewf_handle->h_ewf,&uint64value);
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
    case EWF_OPEN_FAILED_SEEK:
      return "Unable to seek to end of data! Did you specify all EWF segments?";
      break;
    case EWF_OPEN_FAILED_READ:
      return "Unable to read end of data! Did you specify all EWF segments?";
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
static int EwfFreeBuffer(void *p_buf) {
  free(p_buf);
  return EWF_OK;
}

/*
  ----- Change log -----
  20140724: * Initial version implementing EwfOpen, EwfSize, EwfRead, EwfClose,
              EwfOptionsHelp, EwfOptionsParse and EwfFreeBuffer.
  20140731: * Added support for libewf v1 API.
  20140803: * Added EwfCreateHandle, EwfDestroyHandle and EwfGetInfofileContent.
  20150819: * Added debug value to handle.
            * Added v2 API check in EwfOpen to make sure user specified all EWF
              segments.
*/

