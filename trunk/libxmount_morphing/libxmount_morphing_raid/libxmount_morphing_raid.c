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
#include "libxmount_morphing_raid.h"

#define LOG_DEBUG(...) {                                 \
  LIBXMOUNT_LOG_DEBUG(p_raid_handle->debug,__VA_ARGS__); \
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
  return "raid0\0\0";
}

/*
 * LibXmount_Morphing_GetFunctions
 */
void LibXmount_Morphing_GetFunctions(ts_LibXmountMorphingFunctions *p_functions)
{
  p_functions->CreateHandle=&RaidCreateHandle;
  p_functions->DestroyHandle=&RaidDestroyHandle;
  p_functions->Morph=&RaidMorph;
  p_functions->Size=&RaidSize;
  p_functions->Read=&RaidRead;
  p_functions->OptionsHelp=&RaidOptionsHelp;
  p_functions->OptionsParse=&RaidOptionsParse;
  p_functions->GetInfofileContent=&RaidGetInfofileContent;
  p_functions->GetErrorMessage=&RaidGetErrorMessage;
  p_functions->FreeBuffer=&RaidFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * RaidCreateHandle
 */
static int RaidCreateHandle(void **pp_handle,
                           const char *p_format,
                           uint8_t debug)
{
  pts_RaidHandle p_raid_handle;

  // Alloc new handle
  p_raid_handle=malloc(sizeof(ts_RaidHandle));
  if(p_raid_handle==NULL) return RAID_MEMALLOC_FAILED;

  // Init handle values
  p_raid_handle->debug=debug;
  p_raid_handle->input_images_count=0;
  p_raid_handle->chunk_size=RAID_DEFAULT_CHUNKSIZE;
  p_raid_handle->chunks_per_image=0;
  p_raid_handle->p_input_functions=NULL;
  p_raid_handle->morphed_image_size=0;

  LOG_DEBUG("Created new LibXmount_Morphing_Raid handle\n");

  // Return new handle
  *pp_handle=p_raid_handle;
  return RAID_OK;
}

/*
 * RaidDestroyHandle
 */
static int RaidDestroyHandle(void **pp_handle) {
  pts_RaidHandle p_raid_handle=(pts_RaidHandle)*pp_handle;

  LOG_DEBUG("Destroying LibXmount_Morphing_Raid handle\n");

  // Free handle
  free(p_raid_handle);

  *pp_handle=NULL;
  return RAID_OK;
}

/*
 * RaidMorph
 */
static int RaidMorph(void *p_handle,
                     pts_LibXmountMorphingInputFunctions p_input_functions)
{
  pts_RaidHandle p_raid_handle=(pts_RaidHandle)p_handle;
  int ret;
  uint64_t input_image_size;
  uint64_t chunks_per_image;

  LOG_DEBUG("Initializing LibXmount_Morphing_Raid\n");

  // Set input functions and get image count
  p_raid_handle->p_input_functions=p_input_functions;
  if(p_raid_handle->
       p_input_functions->
         ImageCount(&p_raid_handle->input_images_count)!=0)
  {
    return RAID_CANNOT_GET_IMAGECOUNT;
  }

  // Calculate chunks per image
  for(uint64_t i=0;i<p_raid_handle->input_images_count;i++) {
    ret=p_raid_handle->
          p_input_functions->
            Size(i,&input_image_size);
    if(ret!=0) return RAID_CANNOT_GET_IMAGESIZE;

    chunks_per_image=input_image_size/p_raid_handle->chunk_size;

    LOG_DEBUG("Image %" PRIu64 " can hold a maximum of %" PRIu64 " chunks of %"
                PRIu32 " bytes\n",
              i,
              chunks_per_image,
              p_raid_handle->chunk_size);

    // The smallest image determines how many chunks are availbale on all images
    if(p_raid_handle->chunks_per_image==0) {
      p_raid_handle->chunks_per_image=chunks_per_image;
    } else if(chunks_per_image<p_raid_handle->chunks_per_image) {
      p_raid_handle->chunks_per_image=chunks_per_image;
    }
  }

  LOG_DEBUG("Smallest image holds %" PRIu64 " chunks of %" PRIu32 " bytes\n",
            p_raid_handle->chunks_per_image,
            p_raid_handle->chunk_size);

  // Calculate total raid capacity based on smallest disk
  p_raid_handle->morphed_image_size=
    p_raid_handle->chunks_per_image*
      p_raid_handle->chunk_size*p_raid_handle->input_images_count;

  LOG_DEBUG("Total raid capacity is %" PRIu64 " bytes\n",
            p_raid_handle->morphed_image_size);

  return RAID_OK;
}

/*
 * RaidSize
 */
static int RaidSize(void *p_handle, uint64_t *p_size) {
  *p_size=((pts_RaidHandle)(p_handle))->morphed_image_size;
  return RAID_OK;
}

/*
 * RaidRead
 */
static int RaidRead(void *p_handle,
                    char *p_buf,
                    off_t offset,
                    size_t count,
                    size_t *p_read)
{
  pts_RaidHandle p_raid_handle=(pts_RaidHandle)p_handle;
  uint64_t cur_chunk;
  uint64_t cur_image;
  off_t cur_chunk_offset;
  off_t cur_image_offset;
  size_t cur_count;
  int ret;
  size_t read;

  LOG_DEBUG("Reading %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset>=p_raid_handle->morphed_image_size ||
     offset+count>p_raid_handle->morphed_image_size)
  {
    return RAID_READ_BEYOND_END_OF_IMAGE;
  }

  // Calculate starting chunk, and chunk offset
  cur_chunk=offset/p_raid_handle->chunk_size;
  cur_chunk_offset=offset-(cur_chunk*p_raid_handle->chunk_size);

  // Init p_read
  *p_read=0;

  while(count!=0) {
    // Calculate image and image offset to read from
    cur_image=cur_chunk%p_raid_handle->input_images_count;
    cur_image_offset=
      (cur_chunk/p_raid_handle->input_images_count)*p_raid_handle->chunk_size;

    // Calculate how many bytes to read from current chunk
    if(cur_chunk_offset+count>p_raid_handle->chunk_size) {
      cur_count=p_raid_handle->chunk_size-cur_chunk_offset;
    } else {
      cur_count=count;
    }

    LOG_DEBUG("Reading %zu bytes at offset %zu from image %" PRIu64
                " (chunk %" PRIu64 ")\n",
              cur_count,
              cur_image_offset+cur_chunk_offset,
              cur_image,
              cur_chunk);

    // Read bytes
    ret=p_raid_handle->p_input_functions->
          Read(cur_image,
               p_buf,
               cur_image_offset+cur_chunk_offset,
               cur_count,
               &read);
    if(ret!=0 || read!=cur_count) return RAID_CANNOT_READ_DATA;

    p_buf+=cur_count;
    cur_chunk_offset=0;
    count-=cur_count;
    cur_chunk++;
    (*p_read)+=cur_count;
  }

  return RAID_OK;
}

/*
 * RaidOptionsHelp
 */
static int RaidOptionsHelp(const char **pp_help) {
  int ok;
  char *p_buf;

  ok=asprintf(&p_buf,
              "    raid_chunksize : Specify the chunk size to use in bytes. "
                "Defaults to 524288 (512k).\n");
  if(ok<0 || p_buf==NULL) {
    *pp_help=NULL;
    return RAID_MEMALLOC_FAILED;
  }

  *pp_help=p_buf;
  return RAID_OK;
}

/*
 * RaidOptionsParse
 */
static int RaidOptionsParse(void *p_handle,
                            uint32_t options_count,
                            const pts_LibXmountOptions *pp_options,
                            const char **pp_error)
{
  pts_RaidHandle p_raid_handle=(pts_RaidHandle)p_handle;
  int ok;
  uint32_t uint32value;
  char *p_buf;

  for(uint32_t i=0;i<options_count;i++) {
    if(strcmp(pp_options[i]->p_key,"raid_chunksize")) {
      // Convert value to uint32
      uint32value=StrToUint32(pp_options[i]->p_value,&ok);
      if(ok==0 || uint32value==0) {
        // Conversion failed, generate error message and return
        ok=asprintf(&p_buf,
                    "Unable to parse value '%s' of '%s' as valid 32bit number",
                    pp_options[i]->p_value,
                    pp_options[i]->p_key);
        if(ok<0 || *pp_error==NULL) {
          *pp_error=NULL;
          return RAID_MEMALLOC_FAILED;
        }
        *pp_error=p_buf;
        return RAID_CANNOT_PARSE_OPTION;
      }

      LOG_DEBUG("Setting chunk size to %" PRIu32 " bytes\n",uint32value);

      // Conversion ok, save value and mark option as valid
      p_raid_handle->chunk_size=uint32value;
      pp_options[i]->valid=1;
      continue;
    }
  }

  return RAID_OK;
}

/*
 * RaidGetInfofileContent
 */
static int RaidGetInfofileContent(void *p_handle,
                                  const char **pp_info_buf)
{
  pts_RaidHandle p_raid_handle=(pts_RaidHandle)p_handle;
  int ret;
  char *p_buf;

  ret=asprintf(&p_buf,
               "Simulating RAID level 0 over %" PRIu64 " disks.\n"
                 "Chunk size: %" PRIu32 " bytes\n"
                 "Chunks per disk: %" PRIu64 "\n"
                 "Total capacity: %" PRIu64 " bytes (%0.3f GiB)\n",
               p_raid_handle->input_images_count,
               p_raid_handle->chunk_size,
               p_raid_handle->chunks_per_image,
               p_raid_handle->morphed_image_size,
               p_raid_handle->morphed_image_size/(1024.0*1024.0*1024.0));
  if(ret<0 || *pp_info_buf==NULL) return RAID_MEMALLOC_FAILED;

  *pp_info_buf=p_buf;
  return RAID_OK;
}

/*
 * RaidGetErrorMessage
 */
static const char* RaidGetErrorMessage(int err_num) {
  switch(err_num) {
    case RAID_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case RAID_CANNOT_GET_IMAGECOUNT:
      return "Unable to get input image count";
      break;
    case RAID_CANNOT_GET_IMAGESIZE:
      return "Unable to get input image size";
      break;
    case RAID_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read data: Attempt to read past EOF";
      break;
    case RAID_CANNOT_READ_DATA:
      return "Unable to read data";
      break;
    case RAID_CANNOT_PARSE_OPTION:
      return "Unable to parse library option";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * RaidFreeBuffer
 */
static void RaidFreeBuffer(void *p_buf) {
  free(p_buf);
}

