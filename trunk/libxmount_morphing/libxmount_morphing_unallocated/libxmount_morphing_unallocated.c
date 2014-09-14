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

  // Alloc new handle
  p_unallocated_handle=malloc(sizeof(ts_UnallocatedHandle));
  if(p_unallocated_handle==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Init handle values
  p_unallocated_handle->debug=debug;
  p_unallocated_handle->fs_type=UnallocatedFsType_Unknown;
  // DEBUG
  //p_unallocated_handle->fs_type=UnallocatedFsType_HfsPlus;
  p_unallocated_handle->p_input_functions=NULL;
  p_unallocated_handle->morphed_image_size=0;
  p_unallocated_handle->hfsplus.p_vh=NULL;
  p_unallocated_handle->hfsplus.p_alloc_file=NULL;
  p_unallocated_handle->hfsplus.free_block_map_size=0;
  p_unallocated_handle->hfsplus.p_free_block_map=NULL;

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

  // Free fs data
  switch(p_unallocated_handle->fs_type) {
    case UnallocatedFsType_HfsPlus: {
      if(p_unallocated_handle->hfsplus.p_vh!=NULL)
        free(p_unallocated_handle->hfsplus.p_vh);
      if(p_unallocated_handle->hfsplus.p_alloc_file!=NULL)
        free(p_unallocated_handle->hfsplus.p_alloc_file);
      if(p_unallocated_handle->hfsplus.p_free_block_map!=NULL)
        free(p_unallocated_handle->hfsplus.p_free_block_map);
      break;
    }
    case UnallocatedFsType_Unknown:
    default:
      break;
  }

  // Free handle
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

  // Make sure unallocated_fs was given
  if(p_unallocated_handle->fs_type==UnallocatedFsType_Unknown) {
    return UNALLOCATED_NO_FS_SPECIFIED;
  }

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

  // Extract unallocated blocks from input image
  switch(p_unallocated_handle->fs_type) {
    case UnallocatedFsType_HfsPlus: {
      // Read HFS+ VH
      ret=UnallocatedReadHfsPlusHeader(p_unallocated_handle);
      if(ret!=UNALLOCATED_OK) return ret;
      // Read HFS+ alloc file
      ret=UnallocatedReadHfsPlusAllocFile(p_unallocated_handle);
      if(ret!=UNALLOCATED_OK) return ret;
      // Build free block map
      ret=UnallocatedBuildHfsPlusBlockMap(p_unallocated_handle);
      if(ret!=UNALLOCATED_OK) return ret;
      // Calculate morphed image size
      p_unallocated_handle->morphed_image_size=
        p_unallocated_handle->hfsplus.p_vh->block_size*
        p_unallocated_handle->hfsplus.free_block_map_size;
      break;
    }
    case UnallocatedFsType_Unknown:
    default:
      return UNALLOCATED_UNSUPPORTED_FS_SPECIFIED;
  }

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
  int ret;

  LOG_DEBUG("Reading %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset>=p_unallocated_handle->morphed_image_size ||
     offset+count>p_unallocated_handle->morphed_image_size)
  {
    return UNALLOCATED_READ_BEYOND_END_OF_IMAGE;
  }

  // Read data
  switch(p_unallocated_handle->fs_type) {
    case UnallocatedFsType_HfsPlus: {
      ret=UnallocatedReadHfsPlusBlock(p_unallocated_handle,
                                    p_buf,
                                    offset,
                                    count,
                                    p_read);
      if(ret!=UNALLOCATED_OK) return ret;
      break;
    }
    case UnallocatedFsType_Unknown:
    default:
      return UNALLOCATED_UNSUPPORTED_FS_SPECIFIED;
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
                "unallocated blocks from. Supported filesystems are: 'hfs+'\n");
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
  pts_UnallocatedHfsPlusData p_hfs_data=&(p_unallocated_handle->hfsplus);
  int ret;
  char *p_buf;

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
               p_hfs_data->p_vh->signature,
               p_hfs_data->p_vh->version,
               p_hfs_data->p_vh->block_size,
               p_hfs_data->p_vh->total_blocks,
               p_hfs_data->p_vh->free_blocks,
               p_hfs_data->p_vh->alloc_file_size,
               p_hfs_data->p_vh->alloc_file_total_blocks,
               p_hfs_data->free_block_map_size,
               p_hfs_data->free_block_map_size*p_hfs_data->p_vh->block_size,
               (p_hfs_data->free_block_map_size*p_hfs_data->p_vh->block_size)/
                 (1024.0*1024.0*1024.0));
  if(ret<0 || *pp_info_buf==NULL) return UNALLOCATED_MEMALLOC_FAILED;

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
    case UNALLOCATED_NO_FS_SPECIFIED:
      return "No filesystem specified using option unallocated_fs";
      break;
    case UNALLOCATED_UNSUPPORTED_FS_SPECIFIED:
      return "Unsupported fs specified";
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
 * UnallocatedReadHfsPlusHeader
 */
static int UnallocatedReadHfsPlusHeader(
  pts_UnallocatedHandle p_unallocated_handle)
{
  pts_UnallocatedHfsPlusData p_hfs_data=&(p_unallocated_handle->hfsplus);
  int ret;
  size_t bytes_read;
  pts_UnallocatedHfsPlusExtend p_extend;

  LOG_DEBUG("Reading HFS+ volume header\n");

  // Alloc buffer for header
  p_hfs_data->p_vh=calloc(1,sizeof(ts_UnallocatedHfsPlusVH));
  if(p_hfs_data->p_vh==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Read VH from input image
  ret=p_unallocated_handle->
        p_input_functions->
          Read(0,
               (char*)(p_hfs_data->p_vh),
               UNALLOCATED_HFSPLUS_VH_OFFSET,
               sizeof(ts_UnallocatedHfsPlusVH),
               &bytes_read);
  if(ret!=0 || bytes_read!=sizeof(ts_UnallocatedHfsPlusVH)) {
    free(p_hfs_data->p_vh);
    p_hfs_data->p_vh=NULL;
    return UNALLOCATED_CANNOT_READ_HFSPLUS_HEADER;
  }

  // Convert VH to host endianness
  p_hfs_data->p_vh->signature=be16toh(p_hfs_data->p_vh->signature);
  p_hfs_data->p_vh->version=be16toh(p_hfs_data->p_vh->version);
  p_hfs_data->p_vh->block_size=be32toh(p_hfs_data->p_vh->block_size);
  p_hfs_data->p_vh->total_blocks=be32toh(p_hfs_data->p_vh->total_blocks);
  p_hfs_data->p_vh->free_blocks=be32toh(p_hfs_data->p_vh->free_blocks);
  p_hfs_data->p_vh->alloc_file_size=be64toh(p_hfs_data->p_vh->alloc_file_size);
  p_hfs_data->p_vh->alloc_file_clump_size=
    be32toh(p_hfs_data->p_vh->alloc_file_clump_size);
  p_hfs_data->p_vh->alloc_file_total_blocks=
    be32toh(p_hfs_data->p_vh->alloc_file_total_blocks);
  for(int i=0;i<8;i++) {
    p_extend=&(p_hfs_data->p_vh->alloc_file_extends[i]);
    p_extend->start_block=be32toh(p_extend->start_block);
    p_extend->block_count=be32toh(p_extend->block_count);
  }

  LOG_DEBUG("HFS+ VH signature: 0x%04X\n",p_hfs_data->p_vh->signature);
  LOG_DEBUG("HFS+ VH version: %" PRIu16 "\n",p_hfs_data->p_vh->version);
  LOG_DEBUG("HFS+ block size: %" PRIu32 " bytes\n",p_hfs_data->p_vh->block_size);
  LOG_DEBUG("HFS+ total blocks: %" PRIu32 "\n",p_hfs_data->p_vh->total_blocks);
  LOG_DEBUG("HFS+ free blocks: %" PRIu32 "\n",p_hfs_data->p_vh->free_blocks);
  LOG_DEBUG("HFS+ allocation file size: %" PRIu64 " bytes\n",
            p_hfs_data->p_vh->alloc_file_size);
  LOG_DEBUG("HFS+ allocation file blocks: %" PRIu32 "\n",
            p_hfs_data->p_vh->alloc_file_total_blocks);

  // Check header signature and version
  if(p_hfs_data->p_vh->signature!=UNALLOCATED_HFSPLUS_VH_SIGNATURE ||
     p_hfs_data->p_vh->version!=UNALLOCATED_HFSPLUS_VH_VERSION)
  {
    free(p_hfs_data->p_vh);
    p_hfs_data->p_vh=NULL;
    return UNALLOCATED_INVALID_HFSPLUS_HEADER;
  }

  return UNALLOCATED_OK;
}

/*
 * UnallocatedReadHfsPlusAllocFile
 */
static int UnallocatedReadHfsPlusAllocFile(
  pts_UnallocatedHandle p_unallocated_handle)
{
  pts_UnallocatedHfsPlusData p_hfs_data=&(p_unallocated_handle->hfsplus);
  pts_UnallocatedHfsPlusExtend p_extend;
  int ret;
  char *p_buf;
  size_t bytes_read;
  uint64_t total_bytes_read=0;

  LOG_DEBUG("Reading HFS+ allocation file\n");

  // Alloc buffer for file
  p_hfs_data->p_alloc_file=calloc(1,p_hfs_data->p_vh->alloc_file_size);
  if(p_hfs_data->p_alloc_file==NULL) return UNALLOCATED_MEMALLOC_FAILED;

  // Loop over extends and read data
  p_buf=(char*)(p_hfs_data->p_alloc_file);
  for(int i=0;i<8;i++) {
    p_extend=&(p_hfs_data->p_vh->alloc_file_extends[i]);

    // If start_block and block_count are zero, we parsed last extend
    if(p_extend->start_block==0 && p_extend->block_count==0) break;

    LOG_DEBUG("Extend %d contains %" PRIu32
                " block(s) starting with block %" PRIu32 "\n",
              i,
              p_extend->block_count,
              p_extend->start_block);

    // Read data
    for(uint32_t ii=0;ii<p_extend->block_count;ii++) {
      LOG_DEBUG("Reading %" PRIu32 " bytes from block %" PRIu32
                  " at offset %" PRIu64 "\n",
                p_hfs_data->p_vh->block_size,
                p_extend->start_block+ii,
                (uint64_t)((p_extend->start_block+ii)*
                  p_hfs_data->p_vh->block_size));

      ret=p_unallocated_handle->
            p_input_functions->
              Read(0,
                   p_buf,
                   (p_extend->start_block+ii)*p_hfs_data->p_vh->block_size,
                   p_hfs_data->p_vh->block_size,
                   &bytes_read);
      if(ret!=0 || bytes_read!=p_hfs_data->p_vh->block_size) {
        free(p_hfs_data->p_alloc_file);
        p_hfs_data->p_alloc_file=NULL;
        return UNALLOCATED_CANNOT_READ_HFSPLUS_ALLOC_FILE;
      }
      p_buf+=p_hfs_data->p_vh->block_size;
      total_bytes_read+=p_hfs_data->p_vh->block_size;
    }
  }

  // Alloc files with more then 8 extends aren't supported yet
  if(total_bytes_read!=p_hfs_data->p_vh->alloc_file_size) {
    free(p_hfs_data->p_alloc_file);
    p_hfs_data->p_alloc_file=NULL;
    return UNALLOCATED_ALLOC_FILE_HAS_TOO_MUCH_EXTENDS;
  }

  return UNALLOCATED_OK;
}

/*
 * UnallocatedBuildHfsPlusBlockMap
 */
static int UnallocatedBuildHfsPlusBlockMap(
  pts_UnallocatedHandle p_unallocated_handle)
{
  pts_UnallocatedHfsPlusData p_hfs_data=&(p_unallocated_handle->hfsplus);

  LOG_DEBUG("Searching unallocated HFS+ blocks\n");

  // Save offset of every unallocated block in block map
  for(uint32_t cur_block=0;
      cur_block<p_hfs_data->p_vh->total_blocks;
      cur_block++)
  {
    if((p_hfs_data->p_alloc_file[cur_block/8] & (1<<(7-(cur_block%8))))==0) {
      p_hfs_data->p_free_block_map=realloc(p_hfs_data->p_free_block_map,
                                           (p_hfs_data->free_block_map_size+1)*
                                             sizeof(uint64_t));
      if(p_hfs_data->p_free_block_map==NULL) {
        p_hfs_data->free_block_map_size=0;
        return UNALLOCATED_MEMALLOC_FAILED;
      }
      p_hfs_data->p_free_block_map[p_hfs_data->free_block_map_size]=
        cur_block*p_hfs_data->p_vh->block_size;
      p_hfs_data->free_block_map_size++;
    }
  }

  LOG_DEBUG("Found %" PRIu64 " unallocated HFS+ blocks\n",
            p_hfs_data->free_block_map_size);

  if(p_hfs_data->p_vh->free_blocks!=p_hfs_data->free_block_map_size) {
      LOG_WARNING("According to VH, there should be %" PRIu64
                    " unallocated blocks but I found %" PRIu64 "\n",
                  p_hfs_data->p_vh->free_blocks,
                  p_hfs_data->free_block_map_size);
  }

  return UNALLOCATED_OK;
}

/*
 * UnallocatedReadHfsPlusBlock
 */
static int UnallocatedReadHfsPlusBlock(
  pts_UnallocatedHandle p_unallocated_handle,
  char *p_buf,
  off_t offset,
  size_t count,
  size_t *p_read)
{
  pts_UnallocatedHfsPlusData p_hfs_data=&(p_unallocated_handle->hfsplus);
  uint64_t cur_block;
  off_t cur_block_offset;
  off_t cur_image_offset;
  size_t cur_count;
  int ret;
  size_t bytes_read;

  // Calculate starting block and block offset
  cur_block=offset/p_hfs_data->p_vh->block_size;
  cur_block_offset=offset-(cur_block*p_hfs_data->p_vh->block_size);

  // Init p_read
  *p_read=0;

  while(count!=0) {
    // Calculate input image offset to read from
    cur_image_offset=p_hfs_data->p_free_block_map[cur_block]+cur_block_offset;

    // Calculate how many bytes to read from current block
    if(cur_block_offset+count>p_hfs_data->p_vh->block_size) {
      cur_count=p_hfs_data->p_vh->block_size-cur_block_offset;
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

