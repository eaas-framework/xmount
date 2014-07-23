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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBEWF
  #include <libewf.h>
#endif
#ifdef HAVE_LIBEWF_STATIC
  #include "libewf/include/libewf.h"
#endif

#include "../libxmount_input.h"

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
            unsigned char *p_buf,
            uint32_t count);
int EwfClose(void **pp_handle);
int EwfOptionsHelp(const char **pp_help);
int EwfOptionsParse(void *p_handle,
                    char *p_options,
                    char **pp_error);
int EwfGetInfofileContent(void *p_handle,
                          const char **pp_info_buf);

/*******************************************************************************
 * LibXmount_Input API implementation
 ******************************************************************************/
void LibXmount_Input_GetApiVersion(uint8_t *p_ver) {
  *p_ver=LIBXMOUNT_INPUT_API_VERSION;
}

void LibXmount_Input_GetSupportedFormats(char ***ppp_arr, uint8_t *p_arr_len) {
  *ppp_arr=(char*)malloc(sizeof(char*));
  if(*ppp_arr==NULL) {
    *p_arr_len=0;
    return;
  }

  **ppp_arr=(char*)malloc(sizeof(char)*4);
  if(**ppp_arr==NULL) {
    free(*ppp_arr);
    *ppp_arr=NULL;
    *p_arr_len=0;
    return;
  }

  strcpy(**ppp_arr,"ewf");
  *p_arr_len=1;
}

void LibXmount_Input_GetFunctions(tsLibXmountInputFunctions **pp_functions) {
  *pp_functions=
    (ptsLibXmountInputFunctions)malloc(sizeof(tsLibXmountInputFunctions));
  if(*pp_functions==NULL) return;

  (*pp_functions)->Open=&EwfOpen;
  (*pp_functions)->Size=&EwfSize;
  (*pp_functions)->Read=&EwfRead;
  (*pp_functions)->Close=&EwfClose;
  (*pp_functions)->OptionsHelp=&EwfOptionsHelp;
  (*pp_functions)->OptionsParse=&EwfOptionsParse;
  (*pp_functions)->GetInfofileContent=&EwfGetInfofileContent;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
int EwfOpen(void **pp_handle,
            const char **pp_filename_arr,
            uint64_t filename_arr_len)
{
#if defined( HAVE_LIBEWF_V2_API )
    static libewf_handle_t *hEwfFile=NULL;
#else
    static LIBEWF_HANDLE *hEwfFile=NULL;
#endif


#if defined( HAVE_LIBEWF_V2_API )
      if(libewf_check_file_signature(ppInputFilenames[i],NULL)!=1) {
#else
      if(libewf_check_file_signature(ppInputFilenames[i])!=1) {
#endif

#if defined( HAVE_LIBEWF_V2_API )
      if( libewf_handle_initialize(
           &hEwfFile,
           NULL ) != 1 )
      {
        LOG_ERROR("Couldn't create EWF handle!\n")
        return 1;
      }
      if( libewf_handle_open(
           hEwfFile,
           ppInputFilenames,
           InputFilenameCount,
           libewf_get_access_flags_read(),
           NULL ) != 1 )
      {
        LOG_ERROR("Couldn't open EWF file(s)!\n")
        return 1;
      }
#else
      hEwfFile=libewf_open(ppInputFilenames,
                           InputFilenameCount,
                           libewf_get_flags_read());
      if(hEwfFile==NULL) {
        LOG_ERROR("Couldn't open EWF file(s)!\n")
        return 1;
      }
      // Parse EWF header
      if(libewf_parse_header_values(hEwfFile,LIBEWF_DATE_FORMAT_ISO8601)!=1) {
        LOG_ERROR("Couldn't parse ewf header values!\n")
        return 1;
      }
#endif

  return 1;
}

int EwfSize(void *p_handle, uint64_t *p_size) {
#if defined( HAVE_LIBEWF_V2_API )
  if(libewf_handle_get_media_size((libewf_handle_t*)p_handle,p_size,NULL)!=1)
#else
  if(libewf_get_media_size((LIBEWF_HANDLE*)p_handle,p_size)!=1)
#endif
    return 1;
  }
  return 0;
}

int EwfRead(void *p_handle,
            uint64_t offset,
            unsigned char *p_buf,
            uint32_t count)
{
#if defined( HAVE_LIBEWF_V2_API )
  if(libewf_handle_seek_offset((libewf_handle_t*)p_handle,
                               offset,
                               SEEK_SET,
                               NULL)!=-1)
  {
    if(libewf_handle_read_buffer((libewf_handle_t*)p_handle,
                                 p_buf,
                                 count,
                                 NULL)!=count)
    {
#else
  if(libewf_seek_offset((LIBEWF_HANDLE*)p_handle,offset)!=-1) {
    if(libewf_read_buffer((LIBEWF_HANDLE*)p_handle,p_buf,count)!=count) {
#endif
      return 1;
    }
  } else {
    return 1;
  }
  return 0;
}

int EwfClose(void **pp_handle) {
#if defined( HAVE_LIBEWF_V2_API )
  libewf_handle_close((libewf_handle_t*)*pp_handle,NULL);
  libewf_handle_free((libewf_handle_t*)pp_handle,NULL);
#else
  libewf_close((LIBEWF_HANDLE*)*pp_handle);
#endif
  return 0;
}

int EwfOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return 0;
}

int EwfOptionsParse(void *p_handle, char *p_options, char **pp_error) {
  return 0;
}

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

