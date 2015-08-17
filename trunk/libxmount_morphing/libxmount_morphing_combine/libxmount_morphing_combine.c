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
#include <stdio.h>
#include <string.h>

#include "../libxmount_morphing.h"
#include "libxmount_morphing_combine.h"

#define LOG_DEBUG(...) {                                    \
  LIBXMOUNT_LOG_DEBUG(p_combine_handle->debug,__VA_ARGS__); \
}

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
static int CombineCreateHandle(void **pp_handle,
                               const char *p_format,
                               uint8_t debug)
{
  pts_CombineHandle p_combine_handle;

  // Alloc new handle
  p_combine_handle=malloc(sizeof(ts_CombineHandle));
  if(p_combine_handle==NULL) return COMBINE_MEMALLOC_FAILED;

  // Init handle values
  p_combine_handle->debug=debug;
  p_combine_handle->input_images_count=0;
  p_combine_handle->p_input_functions=NULL;
  p_combine_handle->morphed_image_size=0;

  LOG_DEBUG("Created new LibXmount_Morphing_Combine handle\n");

  // Return new handle
  *pp_handle=p_combine_handle;
  return COMBINE_OK;
}

/*
 * CombineDestroyHandle
 */
static int CombineDestroyHandle(void **pp_handle) {
  pts_CombineHandle p_combine_handle=(pts_CombineHandle)*pp_handle;

  LOG_DEBUG("Destroying LibXmount_Morphing_Combine handle\n");

  // Free handle
  free(p_combine_handle);

  *pp_handle=NULL;
  return COMBINE_OK;
}

/*
 * CombineMorph
 */
static int CombineMorph(void *p_handle,
                        pts_LibXmountMorphingInputFunctions p_input_functions)
{
  pts_CombineHandle p_combine_handle=(pts_CombineHandle)p_handle;
  int ret;
  uint64_t input_image_size;

  LOG_DEBUG("Initializing LibXmount_Morphing_Combine\n");

  // Set input functions and get image count
  p_combine_handle->p_input_functions=p_input_functions;
  if(p_combine_handle->
       p_input_functions->
         ImageCount(&p_combine_handle->input_images_count)!=0)
  {
    return COMBINE_CANNOT_GET_IMAGECOUNT;
  }

  // Calculate morphed image size
  for(uint64_t i=0;i<p_combine_handle->input_images_count;i++) {
    ret=p_combine_handle->
          p_input_functions->
            Size(i,&input_image_size);
    if(ret!=0) return COMBINE_CANNOT_GET_IMAGESIZE;

    LOG_DEBUG("Adding %" PRIu64 " bytes from image %" PRIu64 "\n",
              input_image_size,
              i);

    p_combine_handle->morphed_image_size+=input_image_size;
  }

  LOG_DEBUG("Total morphed image size is %" PRIu64 " bytes\n",
            p_combine_handle->morphed_image_size);

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
  uint64_t cur_input_image_size=0;
  off_t cur_offset=offset;
  int ret;
  size_t cur_count;
  size_t read;

  LOG_DEBUG("Reading %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset>=p_combine_handle->morphed_image_size ||
     offset+count>p_combine_handle->morphed_image_size)
  {
    return COMBINE_READ_BEYOND_END_OF_IMAGE;
  }

  // Search starting image to read from
  ret=p_combine_handle->p_input_functions->Size(cur_input_image,
                                                &cur_input_image_size);
  while(ret==0 && cur_offset>=cur_input_image_size) {
    cur_offset-=cur_input_image_size;
    cur_input_image++;
    ret=p_combine_handle->p_input_functions->Size(cur_input_image,
                                                  &cur_input_image_size);
  }
  if(ret!=0) return COMBINE_CANNOT_GET_IMAGESIZE;

  // Init p_read
  *p_read=0;

  // Read data
  while(cur_input_image<p_combine_handle->input_images_count && count!=0) {
    // Get current input image size
    ret=p_combine_handle->p_input_functions->Size(cur_input_image,
                                                  &cur_input_image_size);
    if(ret!=0) return COMBINE_CANNOT_GET_IMAGESIZE;

    // Calculate how many bytes to read from current input image
    if(cur_offset+count>cur_input_image_size) {
      cur_count=cur_input_image_size-cur_offset;
    } else {
      cur_count=count;
    }

    LOG_DEBUG("Reading %zu bytes at offset %zu from input image %" PRIu64 "\n",
              cur_count,
              cur_offset,
              cur_input_image);

    // Read bytes
    ret=p_combine_handle->p_input_functions->
          Read(cur_input_image,
               p_buf,
               cur_offset,
               cur_count,
               &read);
    if(ret!=0 || read!=cur_count) return COMBINE_CANNOT_READ_DATA;

    p_buf+=cur_count;
    cur_offset=0;
    count-=cur_count;
    cur_input_image++;
    (*p_read)+=cur_count;
  }
  if(count!=0) return COMBINE_CANNOT_READ_DATA;

  return COMBINE_OK;
}

/*
 * CombineOptionsHelp
 */
static int CombineOptionsHelp(const char **pp_help) {
  *pp_help=NULL;
  return COMBINE_OK;
}

/*
 * CombineOptionsParse
 */
static int CombineOptionsParse(void *p_handle,
                               uint32_t options_count,
                               const pts_LibXmountOptions *pp_options,
                               const char **pp_error)
{
  return COMBINE_OK;
}

/*
 * CombineGetInfofileContent
 */
static int CombineGetInfofileContent(void *p_handle,
                                     const char **pp_info_buf)
{
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
    case COMBINE_CANNOT_GET_IMAGECOUNT:
      return "Unable to get input image count";
      break;
    case COMBINE_CANNOT_GET_IMAGESIZE:
      return "Unable to get input image size";
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

