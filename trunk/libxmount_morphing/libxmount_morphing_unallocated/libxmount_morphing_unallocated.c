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
#include "libxmount_morphing_unallocated.h"

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
  return "unallocated\0\0";
}

/*
 * LibXmount_Morphing_GetFunctions
 */
void LibXmount_Morphing_GetFunctions(ts_LibXmountMorphingFunctions *p_functions)
{
  p_functions->CreateHandle=&UnallocatedCreateHandle;
  p_functions->DestroyHandle=&UnallocatedDestroyHandle;
  p_functions->Morph=&UnallocatedMorph;
  p_functions->Size=&UnallocatedSize;
  p_functions->Read=&UnallocatedRead;
  p_functions->OptionsHelp=&UnallocatedOptionsHelp;
  p_functions->OptionsParse=&UnallocatedOptionsParse;
  p_functions->GetInfofileContent=&UnallocatedGetInfofileContent;
  p_functions->GetErrorMessage=&UnallocatedGetErrorMessage;
  p_functions->FreeBuffer=&UnallocatedFreeBuffer;
}

/*******************************************************************************
 * Private
 ******************************************************************************/
/*
 * UnallocatedCreateHandle
 */
static int UnallocatedCreateHandle(void **pp_handle,
                                   const char *p_format,
                                   uint8_t debug)
{
  pts_UnallocatedHandle p_unallocated_handle;

  // Alloc new handle. Using calloc in order to set everything to 0x00
  p_unallocated_handle=calloc(1,sizeof(ts_UnallocatedHandle));
  if(p_unallocated_handle==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Init handle values
  p_unallocated_handle->debug=debug;
  p_unallocated_handle->fs_type=UnallocatedFsType_Unknown;

  LOG_DEBUG("Created new LibXmount_Morphing_Unallocated handle\n");

  // Return new handle
  *pp_handle=p_unallocated_handle;
  return UNALLOCATED_OK;
}

/*
 * UnallocatedDestroyHandle
 */
static int UnallocatedDestroyHandle(void **pp_handle) {
  pts_UnallocatedHandle p_unallocated_handle=(pts_UnallocatedHandle)*pp_handle;

  LOG_DEBUG("Destroying LibXmount_Morphing_Unallocated handle\n");

  // TODO: Return if p_unallocated_handle==NULL

  // Free fs data
  switch(p_unallocated_handle->fs_type) {
    case UnallocatedFsType_HfsPlus: {
      if(p_unallocated_handle->p_hfsplus_vh!=NULL)
        free(p_unallocated_handle->p_hfsplus_vh);
      break;
    }
    case UnallocatedFsType_Fat12:
    case UnallocatedFsType_Fat16:
    case UnallocatedFsType_Fat32: {
      if(p_unallocated_handle->p_fat_vh!=NULL)
        free(p_unallocated_handle->p_fat_vh);
      break;
    }
    case UnallocatedFsType_Unknown:
    default:
      break;
  }

  // Free handle values and handle
  if(p_unallocated_handle->p_free_block_map!=NULL)
    free(p_unallocated_handle->p_free_block_map);
  free(p_unallocated_handle);

  *pp_handle=NULL;
  return UNALLOCATED_OK;
}

/*
 * UnallocatedMorph
 */
static int UnallocatedMorph(
  void *p_handle,
  pts_LibXmountMorphingInputFunctions p_input_functions)
{
  pts_UnallocatedHandle p_unallocated_handle=(pts_UnallocatedHandle)p_handle;
  uint64_t input_images_count;
  int ret;

  LOG_DEBUG("Initializing LibXmount_Morphing_Unallocated\n");

  // Set input functions and get image count
  p_unallocated_handle->p_input_functions=p_input_functions;
  if(p_unallocated_handle->
       p_input_functions->
         ImageCount(&input_images_count)!=0)
  {
    return UNALLOCATED_CANNOT_GET_IMAGECOUNT;
  }

  // Make sure there is exactly one input image
  if(input_images_count==0 || input_images_count>1) {
    return UNALLOCATED_WRONG_INPUT_IMAGE_COUNT;
  }

  // Read filesystem header
  switch(p_unallocated_handle->fs_type) {
    case UnallocatedFsType_HfsPlus: {
      // Read HFS+ VH
      ret=ReadHfsPlusHeader(p_unallocated_handle);
      if(ret!=UNALLOCATED_OK) return ret;
      break;
    }
    case UnallocatedFsType_Unknown: {
      // Filesystem wasn't specified. Try to autodetect it. This will also read
      // its header.
      ret=DetectFs(p_unallocated_handle);
      if(ret!=UNALLOCATED_OK) return ret;
      break;
    }
    default: {
      return UNALLOCATED_INTERNAL_ERROR;
    }
  }

  // Extract unallocated blocks from input image
  switch(p_unallocated_handle->fs_type) {
    case UnallocatedFsType_HfsPlus: {
      uint8_t *p_alloc_file;

      // Read HFS+ alloc file
      ret=ReadHfsPlusAllocFile(p_unallocated_handle,&p_alloc_file);
      if(ret!=UNALLOCATED_OK) return ret;

      // Build free block map
      ret=BuildHfsPlusBlockMap(p_unallocated_handle,p_alloc_file);
      // Free alloc file before checking for errors as it needs to be freed
      // either way
      free(p_alloc_file);
      if(ret!=UNALLOCATED_OK) return ret;

      break;
    }
    case UnallocatedFsType_Fat12:
    case UnallocatedFsType_Fat16:
    case UnallocatedFsType_Fat32: {
      // TODO
      break;
    }
    case UnallocatedFsType_Unknown:
    default:
      return UNALLOCATED_INTERNAL_ERROR;
  }

  // Calculate morphed image size
  p_unallocated_handle->morphed_image_size=p_unallocated_handle->block_size*
    p_unallocated_handle->free_block_map_size;

  LOG_DEBUG("Total size of unallocated blocks is %" PRIu64 " bytes\n",
            p_unallocated_handle->morphed_image_size);

  return UNALLOCATED_OK;
}

/*
 * UnallocatedSize
 */
static int UnallocatedSize(void *p_handle, uint64_t *p_size) {
  *p_size=((pts_UnallocatedHandle)(p_handle))->morphed_image_size;
  return UNALLOCATED_OK;
}

/*
 * UnallocatedRead
 */
static int UnallocatedRead(void *p_handle,
                           char *p_buf,
                           off_t offset,
                           size_t count,
                           size_t *p_read)
{
  pts_UnallocatedHandle p_unallocated_handle=(pts_UnallocatedHandle)p_handle;
  uint64_t cur_block;
  off_t cur_block_offset;
  off_t cur_image_offset;
  size_t cur_count;
  int ret;
  size_t bytes_read;

  LOG_DEBUG("Reading %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset>=p_unallocated_handle->morphed_image_size ||
     offset+count>p_unallocated_handle->morphed_image_size)
  {
    return UNALLOCATED_READ_BEYOND_END_OF_IMAGE;
  }

  // Calculate starting block and block offset
  cur_block=offset/p_unallocated_handle->block_size;
  cur_block_offset=offset-(cur_block*p_unallocated_handle->block_size);

  // Init p_read
  *p_read=0;

  while(count!=0) {
    // Calculate input image offset to read from
    cur_image_offset=
      p_unallocated_handle->p_free_block_map[cur_block]+cur_block_offset;

    // Calculate how many bytes to read from current block
    if(cur_block_offset+count>p_unallocated_handle->block_size) {
      cur_count=p_unallocated_handle->block_size-cur_block_offset;
    } else {
      cur_count=count;
    }

    LOG_DEBUG("Reading %zu bytes at offset %zu (block %" PRIu64 ")\n",
              cur_count,
              cur_image_offset+cur_block_offset,
              cur_block);

    // Read bytes
    ret=p_unallocated_handle->p_input_functions->
          Read(0,
               p_buf,
               cur_image_offset+cur_block_offset,
               cur_count,
               &bytes_read);
    if(ret!=0 || bytes_read!=cur_count) return UNALLOCATED_CANNOT_READ_DATA;

    p_buf+=cur_count;
    cur_block_offset=0;
    count-=cur_count;
    cur_block++;
    (*p_read)+=cur_count;
  }

  return UNALLOCATED_OK;
}

/*
 * UnallocatedOptionsHelp
 */
static int UnallocatedOptionsHelp(const char **pp_help) {
  int ok;
  char *p_buf;

  ok=asprintf(&p_buf,
              "    unallocated_fs : Specify the filesystem to extract "
                "unallocated blocks from. Supported filesystems are: "
                "'hfs+', 'fat12', 'fat16', 'fat32'. "
                "Default: autodetect.\n");
  if(ok<0 || p_buf==NULL) {
    *pp_help=NULL;
    return UNALLOCATED_MEMALLOC_FAILED;
  }

  *pp_help=p_buf;
  return UNALLOCATED_OK;
}

/*
 * UnallocatedOptionsParse
 */
static int UnallocatedOptionsParse(void *p_handle,
                                   uint32_t options_count,
                                   const pts_LibXmountOptions *pp_options,
                                   const char **pp_error)
{
  pts_UnallocatedHandle p_unallocated_handle=(pts_UnallocatedHandle)p_handle;
  int ok;
  char *p_buf;

  for(uint32_t i=0;i<options_count;i++) {
    if(strcmp(pp_options[i]->p_key,"unallocated_fs")==0) {
      if(strcmp(pp_options[i]->p_value,"hfs+")==0) {
        p_unallocated_handle->fs_type=UnallocatedFsType_HfsPlus;
      } else if(strcmp(pp_options[i]->p_value,"fat12")==0) {
        p_unallocated_handle->fs_type=UnallocatedFsType_Fat12;
      } else if(strcmp(pp_options[i]->p_value,"fat16")==0) {
        p_unallocated_handle->fs_type=UnallocatedFsType_Fat16;
      } else if(strcmp(pp_options[i]->p_value,"fat32")==0) {
        p_unallocated_handle->fs_type=UnallocatedFsType_Fat32;
      } else {
        ok=asprintf(&p_buf,
                    "Unsupported filesystem '%s' specified",
                    pp_options[i]->p_value);
        if(ok<0 || p_buf==NULL) {
          *pp_error=NULL;
          return UNALLOCATED_MEMALLOC_FAILED;
        }
        *pp_error=p_buf;
        return UNALLOCATED_UNSUPPORTED_FS_SPECIFIED;
      }

      LOG_DEBUG("Setting fs to %s\n",pp_options[i]->p_value);

      pp_options[i]->valid=1;
    }
  }

  return UNALLOCATED_OK;
}

/*
 * UnallocatedGetInfofileContent
 */
static int UnallocatedGetInfofileContent(void *p_handle,
                                         const char **pp_info_buf)
{
  pts_UnallocatedHandle p_unallocated_handle=(pts_UnallocatedHandle)p_handle;
  int ret=-1;
  char *p_buf=NULL;

  switch(p_unallocated_handle->fs_type) {
    case UnallocatedFsType_HfsPlus: {
      pts_HfsPlusVH p_hfsplus_vh=p_unallocated_handle->p_hfsplus_vh;
      ret=asprintf(&p_buf,
                   "HFS+ VH signature: 0x%04X\n"
                     "HFS+ VH version: %" PRIu16 "\n"
                     "HFS+ block size: %" PRIu32 " bytes\n"
                     "HFS+ total blocks: %" PRIu32 "\n"
                     "HFS+ free blocks: %" PRIu32 "\n"
                     "HFS+ allocation file size: %" PRIu64 " bytes\n"
                     "HFS+ allocation file blocks: %" PRIu32 "\n"
                     "Discovered free blocks: %" PRIu64 "\n"
                     "Total unallocated size: %" PRIu64 " bytes (%0.3f GiB)\n",
                   p_hfsplus_vh->signature,
                   p_hfsplus_vh->version,
                   p_hfsplus_vh->block_size,
                   p_hfsplus_vh->total_blocks,
                   p_hfsplus_vh->free_blocks,
                   p_hfsplus_vh->alloc_file_size,
                   p_hfsplus_vh->alloc_file_total_blocks,
                   p_unallocated_handle->free_block_map_size,
                   p_unallocated_handle->free_block_map_size*
                     p_unallocated_handle->block_size,
                   (p_unallocated_handle->free_block_map_size*
                     p_unallocated_handle->block_size)/(1024.0*1024.0*1024.0));
      break;
    }
    case UnallocatedFsType_Fat12:
    case UnallocatedFsType_Fat16:
    case UnallocatedFsType_Fat32: {
      pts_FatVH p_fat_vh=p_unallocated_handle->p_fat_vh;
      ret=asprintf(&p_buf,
                   "FAT bytes per sector: %" PRIu16 "\n"
                     "FAT sectors per cluster: %" PRIu8 "\n"
                     "FAT reserved sectors: %" PRIu16 "\n"
                     "FAT count: %" PRIu8 "\n"
                     "FAT root entry count: %" PRIu16 "\n"
                     "FAT media type: %02X\n"
                     "FAT total sector count (16bit): %" PRIu16 "\n"
                     "FAT sectors per FAT (16bit): %" PRIu16 "\n"
                     "FAT total sector count (32bit): %" PRIu32 "\n"
                     "FAT sectors per FAT (32bit): %" PRIu32 "\n"
                     "Discovered free blocks: %" PRIu64 "\n"
                     "Total unallocated size: %" PRIu64 " bytes (%0.3f GiB)\n",
                   p_fat_vh->bytes_per_sector,
                   p_fat_vh->sectors_per_cluster,
                   p_fat_vh->reserved_sectors,
                   p_fat_vh->fat_count,
                   p_fat_vh->root_entry_count,
                   p_fat_vh->media_type,
                   p_fat_vh->total_sectors_16,
                   p_fat_vh->fat16_sectors,
                   p_fat_vh->total_sectors_32,
                   p_fat_vh->fat32_sectors,
                   p_unallocated_handle->free_block_map_size,
                   p_unallocated_handle->free_block_map_size*
                     p_unallocated_handle->block_size,
                   (p_unallocated_handle->free_block_map_size*
                     p_unallocated_handle->block_size)/(1024.0*1024.0*1024.0));
      break;
    }
    case UnallocatedFsType_Unknown:
    default:
      return UNALLOCATED_INTERNAL_ERROR;
  }

  // Check if asprintf worked
  if(ret<0 || p_buf==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  *pp_info_buf=p_buf;
  return UNALLOCATED_OK;
}

/*
 * UnallocatedGetErrorMessage
 */
static const char* UnallocatedGetErrorMessage(int err_num) {
  switch(err_num) {
    case UNALLOCATED_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case UNALLOCATED_NO_SUPPORTED_FS_DETECTED:
      return "Unable to detect a supported file system";
      break;
    case UNALLOCATED_UNSUPPORTED_FS_SPECIFIED:
      return "Unsupported fs specified";
      break;
    case UNALLOCATED_INTERNAL_ERROR:
      return "Internal error";
      break;
    case UNALLOCATED_CANNOT_GET_IMAGECOUNT:
      return "Unable to get input image count";
      break;
    case UNALLOCATED_WRONG_INPUT_IMAGE_COUNT:
      return "Only 1 input image is supported";
      break;
    case UNALLOCATED_CANNOT_GET_IMAGESIZE:
      return "Unable to get input image size";
      break;
    case UNALLOCATED_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read data: Attempt to read past EOF";
      break;
    case UNALLOCATED_CANNOT_READ_DATA:
      return "Unable to read data";
      break;
    case UNALLOCATED_CANNOT_PARSE_OPTION:
      return "Unable to parse library option";
      break;
    case UNALLOCATED_CANNOT_READ_HFSPLUS_HEADER:
      return "Unable to read HFS+ volume header";
      break;
    case UNALLOCATED_INVALID_HFSPLUS_HEADER:
      return "Found invalid HFS+ volume header";
      break;
    case UNALLOCATED_CANNOT_READ_HFSPLUS_ALLOC_FILE:
      return "Unable to read HFS+ allocation file";
      break;
    case UNALLOCATED_ALLOC_FILE_HAS_TOO_MUCH_EXTENDS:
      return "HFS+ allocation file has more then 8 extends. "
               "This is unsupported";
      break;
    case UNALLOCATED_INVALID_FAT_HEADER:
      return "Found invalid FAT volume header";
      break;
    default:
      return "Unknown error";
  }
}

/*
 * UnallocatedFreeBuffer
 */
static void UnallocatedFreeBuffer(void *p_buf) {
  free(p_buf);
}

/*******************************************************************************
 * Private helper functions
 ******************************************************************************/
/*
 * DetectFs
 */
static int DetectFs(pts_UnallocatedHandle p_unallocated_handle) {
  LOG_DEBUG("Trying to autodetect fs\n");

  // Probe all supported filesystems by trying to read their headers
  if(ReadHfsPlusHeader(p_unallocated_handle)==UNALLOCATED_OK) {
    LOG_DEBUG("Detected HFS+ fs\n");
    p_unallocated_handle->fs_type=UnallocatedFsType_HfsPlus;
    return UNALLOCATED_OK;
  } else if(ReadFatHeader(p_unallocated_handle)==UNALLOCATED_OK) {
    LOG_DEBUG("Detected FAT fs\n");
    return UNALLOCATED_OK;
  }

  LOG_DEBUG("Unable to autodetect fs\n");

  return UNALLOCATED_NO_SUPPORTED_FS_DETECTED;
}

