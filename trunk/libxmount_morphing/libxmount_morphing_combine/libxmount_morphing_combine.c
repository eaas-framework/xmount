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
#include <stdio.h>
#include <string.h>

#include "../libxmount_morphing.h"
#include "libxmount_morphing_combine.h"

#define DEBUG

/*******************************************************************************
 * LibXmount_Morphing API implementation
 ******************************************************************************/
/*
 * LibXmount_Morphing_GetApiVersion
 */
uint8_t LibXmount_Morphing_GetApiVersion() {
  return LIBXMOUNT_MORPHING_API_VERSION;
}

/*
 * LibXmount_Morphing_GetSupportedFormats
 */
const char* LibXmount_Morphing_GetSupportedTypes() {
  return "combine\0\0";
}

/*
 * LibXmount_Morphing_GetFunctions
 */
void LibXmount_Morphing_GetFunctions(ts_LibXmountMorphingFunctions *p_functions)
{
  p_functions->CreateHandle=&CombineCreateHandle;
  p_functions->DestroyHandle=&CombineDestroyHandle;
  p_functions->Morph=&CombineMorph;
  p_functions->Size=&CombineSize;
  p_functions->Read=&CombineRead;
  p_functions->OptionsHelp=&CombineOptionsHelp;
  p_functions->OptionsParse=&CombineOptionsParse;
  p_functions->GetInfofileContent=&CombineGetInfofileContent;
  p_functions->GetErrorMessage=&CombineGetErrorMessage;
  p_functions->FreeBuffer=&CombineFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * CombineCreateHandle
 */
static int CombineCreateHandle(void **pp_handle, char *p_format) {
  pts_CombineHandle p_handle;

  // Alloc new handle
  p_handle=malloc(sizeof(ts_CombineHandle));
  if(p_handle==NULL) return COMBINE_MEMALLOC_FAILED;

  // Init handle values
  p_handle->input_images_count=0;
  p_handle->pp_input_images=NULL;
  p_handle->morphed_image_size=0;

  // Return new handle
  *pp_handle=p_handle;
  return COMBINE_OK;
}

/*
 * CombineDestroyHandle
 */
static int CombineDestroyHandle(void **pp_handle) {
  pts_CombineHandle p_handle=(pts_CombineHandle)*pp_handle;

  // Free handle
  free(p_handle);

  *pp_handle=NULL;
  return COMBINE_OK;
}

/*
 * CombineMorph
 */
static int CombineMorph(void *p_handle,
                        uint64_t input_images,
                        const pts_LibXmountMorphingInputImage *pp_input_images)
{
  pts_CombineHandle p_combine_handle=(pts_CombineHandle)p_handle;

  // Add given values to our handle
  p_combine_handle->input_images_count=input_images;
  p_combine_handle->pp_input_images=pp_input_images;

  // Calculate morphed image size
  for(uint64_t i=0;i<input_images;i++) {
#ifdef DEBUG
    printf("Adding %" PRIu64 " bytes from image %" PRIu64 " to morphed size.\n",
           pp_input_images[i]->size,
           i);
#endif
    p_combine_handle->morphed_image_size+=pp_input_images[i]->size;
  }

#ifdef DEBUG
  printf("Total morphed image size is %" PRIu64 " bytes.\n",
         p_combine_handle->morphed_image_size);
#endif

  return COMBINE_OK;
}

/*
 * CombineSize
 */
static int CombineSize(void *p_handle, uint64_t *p_size) {
  *p_size=((pts_CombineHandle)(p_handle))->morphed_image_size;
  return COMBINE_OK;
}

/*
 * CombineRead
 */
static int CombineRead(void *p_handle,
                       char *p_buf,
                       off_t offset,
                       size_t count,
                       size_t *p_read)
{
  pts_CombineHandle p_combine_handle=(pts_CombineHandle)p_handle;
  uint64_t cur_input_image=0;
  off_t cur_offset=offset;
  int ret;
  size_t cur_count;
  size_t read;

  // Make sure read parameters are within morphed image bounds
  if(offset>=p_combine_handle->morphed_image_size ||
     offset+count>p_combine_handle->morphed_image_size)
  {
    return COMBINE_READ_BEYOND_END_OF_IMAGE;
  }

  // Search starting image to read from
  while(cur_offset>=p_combine_handle->pp_input_images[cur_input_image]->size) {
    cur_offset-=p_combine_handle->pp_input_images[cur_input_image]->size;
    cur_input_image++;
  }

  // Read data
  while(count!=0) {
    // Calculate how many bytes to read from current input image
    if(cur_offset+count>
         p_combine_handle->pp_input_images[cur_input_image]->size)
    {
      cur_count=
        p_combine_handle->pp_input_images[cur_input_image]->size-cur_offset;
    } else {
      cur_count=count;
    }

    // Read bytes
    ret=p_combine_handle->pp_input_images[cur_input_image]->
          Read(p_combine_handle->pp_input_images[cur_input_image]->
                 p_image_handle,
               p_buf,
               cur_offset,
               cur_count,
               &read);
    if(ret!=0 || read!=cur_count) return COMBINE_CANNOT_READ_DATA;

    p_buf+=cur_count;
    cur_offset=0;
    count-=cur_count;
    cur_input_image++;
  }

  *p_read=count;
  return COMBINE_OK;
}

/*
 * CombineOptionsHelp
 */
static const char* CombineOptionsHelp() {
  return COMBINE_OK;
}

/*
 * CombineOptionsParse
 */
static int CombineOptionsParse(void *p_handle,
                               char *p_options,
                               char **pp_error)
{
  *pp_error=NULL;
  return COMBINE_OK;
}

/*
 * CombineGetInfofileContent
 */
static int CombineGetInfofileContent(void *p_handle, char **pp_info_buf) {
  *pp_info_buf=NULL;
  return COMBINE_OK;
}

/*
 * CombineGetErrorMessage
 */
static const char* CombineGetErrorMessage(int err_num) {
  switch(err_num) {
    case COMBINE_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case COMBINE_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read data: Attempt to read past EOF";
      break;
    case COMBINE_CANNOT_READ_DATA:
      return "Unable to read data";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * CombineFreeBuffer
 */
static void CombineFreeBuffer(void *p_buf) {
  free(p_buf);
}

