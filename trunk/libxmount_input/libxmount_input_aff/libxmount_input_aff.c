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

#undef HAVE_LIBAFF_STATIC

#include <stdlib.h>
#include <string.h>

#include "../libxmount_input.h"

#ifndef HAVE_LIBAFF_STATIC
  #include <afflib/afflib.h>
#else
  #include "libaff/lib/afflib.h"
#endif

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
int AffOpen(void **pp_handle,
            const char **pp_filename_arr,
            uint64_t filename_arr_len);
int AffSize(void *p_handle,
            uint64_t *p_size);
int AffRead(void *p_handle,
            uint64_t seek,
            unsigned char *p_buf,
            uint32_t count);
int AffClose(void **pp_handle);
const char* AffOptionsHelp();
int AffOptionsParse(void *p_handle,
                    char *p_options,
                    char **pp_error);
int AffGetInfofileContent(void *p_handle,
                          const char **pp_info_buf);
void AffFreeBuffer(void *p_buf);

/*******************************************************************************
 * LibXmount_Input API implementation
 ******************************************************************************/
/*
 * LibXmount_Input_GetApiVersion
 */
void LibXmount_Input_GetApiVersion(uint8_t *p_ver) {
  *p_ver=LIBXMOUNT_INPUT_API_VERSION;
}

/*
 * LibXmount_Input_GetSupportedFormats
 */
void LibXmount_Input_GetSupportedFormats(char ***ppp_arr, uint8_t *p_arr_len) {
  // Alloc array containing 1 element with content "aff"
  *ppp_arr=(char**)malloc(sizeof(char*));
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
  strcpy(**ppp_arr,"aff");
  *p_arr_len=1;
}

/*
 * LibXmount_Input_GetFunctions
 */
void LibXmount_Input_GetFunctions(ts_LibXmountInputFunctions **pp_functions) {
  *pp_functions=
    (pts_LibXmountInputFunctions)malloc(sizeof(ts_LibXmountInputFunctions));
  if(*pp_functions==NULL) return;

  (*pp_functions)->Open=&AffOpen;
  (*pp_functions)->Size=&AffSize;
  (*pp_functions)->Read=&AffRead;
  (*pp_functions)->Close=&AffClose;
  (*pp_functions)->OptionsHelp=&AffOptionsHelp;
  (*pp_functions)->OptionsParse=&AffOptionsParse;
  (*pp_functions)->GetInfofileContent=&AffGetInfofileContent;
  (*pp_functions)->FreeBuffer=&AffFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * AffOpen
 */
int AffOpen(void **pp_handle,
            const char **pp_filename_arr,
            uint64_t filename_arr_len)
{
  // We need at least one file
  if(filename_arr_len==0) return 1;

  // Open AFF file
  *pp_handle=(void*)af_open(pp_filename_arr[0],O_RDONLY,0);
  if(!*pp_handle) {
    // LOG_ERROR("Couldn't open AFF file!\n")
    return 1;
  }

  // Encrypted images aren't supported for now
  if(af_cannot_decrypt((AFFILE*)*p_handle)) {
    // LOG_ERROR("Encrypted AFF input images aren't supported yet!\n")
    return 1;
  }

  return 0;
}

/*
 * AffSize
 */
int AffSize(void *p_handle, uint64_t *p_size) {
  *p_size=af_seek((AFFILE*)p_handle,0,SEEK_END);
  // TODO: Check for error
  return 0;
}

/*
 * AffRead
 */
int AffRead(void *p_handle,
            uint64_t offset,
            unsigned char *p_buf,
            uint32_t count)
{
  af_seek((AFFILE*)p_handle,offset,SEEK_SET);
  // TODO: Check for error
  if(af_read((AFFILE*)p_handle,p_buf,count)!=count) {
    // LOG_ERROR("Couldn't read %zd bytes from offset %" PRIu64
    //              "!\n",ToRead,offset);
    return 1;
  }
  return 0;
}

/*
 * AffClose
 */
int AffClose(void **pp_handle) {
  af_close((AFFILE*)*p_handle);
  // TODO: Check for error
  return 0;
}

/*
 * AffOptionsHelp
 */
const char* AffOptionsHelp() {
  return NULL;
}

/*
 * AffOptionsParse
 */
int AffOptionsParse(void *p_handle, char *p_options, char **pp_error) {
  return 0;
}

/*
 * AffGetInfofileContent
 */
int AffGetInfofileContent(void *p_handle, const char **pp_info_buf) {
  // TODO
  *pp_info_buf=NULL;
  return 0;
}

/*
 * AffFreeBuffer
 */
void AffFreeBuffer(void *p_buf) {
  free(p_buf);
}

/*
  ----- Change history -----
  20140724: * Initial version implementing AffOpen, AffSize, AffRead, AffClose,
              AffOptionsHelp, AffOptionsParse and AffFreeBuffer
*/

