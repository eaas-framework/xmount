/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* xmount is a small tool to "fuse mount" various image formats and enable      *
* virtual write access.                                                        *
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

#undef HAVE_LIBEWF_STATIC

#include <stdlib.h>
#include <string.h>

#include "../libxmount_input.h"

#ifndef HAVE_LIBEWF_STATIC
  #include <libewf.h>
#else
  #include "libewf/include/libewf.h"
#endif

#if !defined(LIBEWF_HANDLE)
  // libewf version 2 no longer defines LIBEWF_HANDLE
  #define HAVE_LIBEWF_V2_API
#endif

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
int EwfOpen(void **pp_handle,
            const char **pp_filename_arr,
            uint64_t filename_arr_len);
int EwfSize(void *p_handle,
            uint64_t *p_size);
int EwfRead(void *p_handle,
            uint64_t seek,
            char *p_buf,
            uint32_t count);
int EwfClose(void **pp_handle);
const char* EwfOptionsHelp();
int EwfOptionsParse(void *p_handle,
                    char *p_options,
                    char **pp_error);
int EwfGetInfofileContent(void *p_handle,
                          const char **pp_info_buf);
void EwfFreeBuffer(void *p_buf);

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
  p_functions->Open=&EwfOpen;
  p_functions->Size=&EwfSize;
  p_functions->Read=&EwfRead;
  p_functions->Close=&EwfClose;
  p_functions->OptionsHelp=&EwfOptionsHelp;
  p_functions->OptionsParse=&EwfOptionsParse;
  p_functions->GetInfofileContent=&EwfGetInfofileContent;
  p_functions->FreeBuffer=&EwfFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * EwfOpen
 */
int EwfOpen(void **pp_handle,
            const char **pp_filename_arr,
            uint64_t filename_arr_len)
{
  // We need at least one file
  if(filename_arr_len==0) return 1;

  // Make sure all files are EWF files
  for(uint64_t i=0;i<filename_arr_len;i++) {
#ifdef HAVE_LIBEWF_V2_API
    if(libewf_check_file_signature(pp_filename_arr[i],NULL)!=1)
#else
    if(libewf_check_file_signature(pp_filename_arr[i])!=1)
#endif
    {
      return 1;
    }
  }

  // Init handle
  *pp_handle=NULL;
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_initialize((libewf_handle_t**)pp_handle,NULL)!=1) {
    // LOG_ERROR("Couldn't create EWF handle!\n")
    return 1;
  }
#endif

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
    // LOG_ERROR("Couldn't open EWF file(s)!\n")
    return 1;
  }

#ifndef HAVE_LIBEWF_V2_API
  // Parse EWF header
  if(libewf_parse_header_values((LIBEWF_HANDLE*)*pp_handle,
                                LIBEWF_DATE_FORMAT_ISO8601)!=1)
  {
    //LOG_ERROR("Couldn't parse ewf header values!\n")
    return 1;
  }
#endif

  return 0;
}

/*
 * EwfSize
 */
int EwfSize(void *p_handle, uint64_t *p_size) {
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_get_media_size((libewf_handle_t*)p_handle,p_size,NULL)!=1) {
#else
  if(libewf_get_media_size((LIBEWF_HANDLE*)p_handle,p_size)!=1) {
#endif
    return 1;
  }
  return 0;
}

/*
 * EwfRead
 */
int EwfRead(void *p_handle,
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
      return 1;
    }
  } else {
    return 1;
  }
  return 0;
}

/*
 * EwfClose
 */
int EwfClose(void **pp_handle) {
  // Close EWF handle
#ifdef HAVE_LIBEWF_V2_API
  if(libewf_handle_close((libewf_handle_t*)*pp_handle,NULL)!=0) {
    return 1;
  }
#else
  // TODO: No return value??
  libewf_close((LIBEWF_HANDLE*)*pp_handle);
#endif

#ifdef HAVE_LIBEWF_V2_API
  // Free EWF handle
  if(libewf_handle_free((libewf_handle_t**)pp_handle,NULL)!=1) {
    return 1;
  }
#endif

  *pp_handle=NULL;
  return 0;
}

/*
 * EwfOptionsHelp
 */
const char* EwfOptionsHelp() {
  return NULL;
}

/*
 * EwfOptionsParse
 */
int EwfOptionsParse(void *p_handle, char *p_options, char **pp_error) {
  return 0;
}

/*
 * EwfGetInfofileContent
 */
int EwfGetInfofileContent(void *p_handle, const char **pp_info_buf) {
/*
#define M_SAVE_VALUE(DESC,SHORT_DESC) { \
  if(ret==1) {             \
    XMOUNT_REALLOC(pVirtualImageInfoFile,char*, \
      (strlen(pVirtualImageInfoFile)+strlen(buf)+strlen(DESC)+2)) \
    strncpy((pVirtualImageInfoFile+strlen(pVirtualImageInfoFile)),DESC,strlen(DESC)+1); \
    strncpy((pVirtualImageInfoFile+strlen(pVirtualImageInfoFile)),buf,strlen(buf)+1); \
    strncpy((pVirtualImageInfoFile+strlen(pVirtualImageInfoFile)),"\n",2); \
  } else if(ret==-1) { \
    LOG_WARNING("Couldn't query EWF image header value '%s'\n",SHORT_DESC) \
  } \
}
    case TOrigImageType_EWF:
      // Original image is an EWF file. Extract various infos from ewf file and
      // add them to the virtual image info file content.
#if defined( HAVE_LIBEWF_V2_API )
      ret=libewf_handle_get_utf8_header_value_case_number(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Case number: ","Case number")
      ret=libewf_handle_get_utf8_header_value_description(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Description: ","Description")
      ret=libewf_handle_get_utf8_header_value_examiner_name(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Examiner: ","Examiner")
      ret=libewf_handle_get_utf8_header_value_evidence_number(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Evidence number: ","Evidence number")
      ret=libewf_handle_get_utf8_header_value_notes(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Notes: ","Notes")
      ret=libewf_handle_get_utf8_header_value_acquiry_date(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Acquiry date: ","Acquiry date")
      ret=libewf_handle_get_utf8_header_value_system_date(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("System date: ","System date")
      ret=libewf_handle_get_utf8_header_value_acquiry_operating_system(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Acquiry os: ","Acquiry os")
      ret=libewf_handle_get_utf8_header_value_acquiry_software_version(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("Acquiry sw version: ","Acquiry sw version")
      ret=libewf_handle_get_utf8_hash_value_md5(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("MD5 hash: ","MD5 hash")
      ret=libewf_handle_get_utf8_hash_value_sha1(hEwfFile,buf,sizeof(buf),NULL);
      M_SAVE_VALUE("SHA1 hash: ","SHA1 hash")
#else
      ret=libewf_get_header_value_case_number(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Case number: ","Case number")
      ret=libewf_get_header_value_description(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Description: ","Description")
      ret=libewf_get_header_value_examiner_name(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Examiner: ","Examiner")
      ret=libewf_get_header_value_evidence_number(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Evidence number: ","Evidence number")
      ret=libewf_get_header_value_notes(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Notes: ","Notes")
      ret=libewf_get_header_value_acquiry_date(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Acquiry date: ","Acquiry date")
      ret=libewf_get_header_value_system_date(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("System date: ","System date")
      ret=libewf_get_header_value_acquiry_operating_system(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Acquiry os: ","Acquiry os")
      ret=libewf_get_header_value_acquiry_software_version(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("Acquiry sw version: ","Acquiry sw version")
      ret=libewf_get_hash_value_md5(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("MD5 hash: ","MD5 hash")
      ret=libewf_get_hash_value_sha1(hEwfFile,buf,sizeof(buf));
      M_SAVE_VALUE("SHA1 hash: ","SHA1 hash")
#endif
      break;
#undef M_SAVE_VALUE
*/
  *pp_info_buf=NULL;
  return 0;
}

/*
 * EwfFreeBuffer
 */
void EwfFreeBuffer(void *p_buf) {
  free(p_buf);
}

/*
  ----- Change history -----
  20140724: * Initial version implementing EwfOpen, EwfSize, EwfRead, EwfClose,
              EwfOptionsHelp, EwfOptionsParse and EwfFreeBuffer
  20140731: * Added ifdef's for libewf1 functions
*/

