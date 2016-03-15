#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../libxmount_morphing.h"
#include "libxmount_morphing_trim.h"

#define LOG_DEBUG(...) {                                    \
  LIBXMOUNT_LOG_DEBUG(p_trim_handle->debug,__VA_ARGS__); \
}

uint8_t LibXmount_Morphing_GetApiVersion() {
  return LIBXMOUNT_MORPHING_API_VERSION;
}

const char* LibXmount_Morphing_GetSupportedTypes() {
  return "trim\0";
}

void LibXmount_Morphing_GetFunctions(ts_LibXmountMorphingFunctions *p_functions)
{
  p_functions->CreateHandle=&TrimCreateHandle;
  p_functions->DestroyHandle=&TrimDestroyHandle;
  p_functions->Morph=&TrimMorph;
  p_functions->Size=&TrimSize;
  p_functions->Read=&TrimRead;
  p_functions->Write=&TrimWrite;
  p_functions->OptionsHelp=&TrimOptionsHelp;
  p_functions->OptionsParse=&TrimOptionsParse;
  p_functions->GetInfofileContent=&TrimGetInfofileContent;
  p_functions->GetErrorMessage=&TrimGetErrorMessage;
  p_functions->FreeBuffer=&TrimFreeBuffer;
}

static int TrimCreateHandle(void **pp_handle,
                            const char *p_format,
                            uint8_t debug)
{
  pts_TrimHandle p_trim_handle;

  // Alloc new handle
  p_trim_handle = malloc(sizeof(ts_TrimHandle));
  if(p_trim_handle==NULL)
    return TRIM_MEMALLOC_FAILED;
  memset(p_trim_handle, 0, sizeof(ts_TrimHandle));

  // Init handle values
  p_trim_handle->debug = debug;

  LOG_DEBUG("Created new LibXmount_Morphing_Trim handle\n");

  // Return new handle
  *pp_handle = p_trim_handle;
  return TRIM_OK;
}

static int TrimDestroyHandle(void **pp_handle) {
  pts_TrimHandle p_trim_handle = (pts_TrimHandle)*pp_handle;

  LOG_DEBUG("Destroying LibXmount_Morphing_Trim handle\n");

  // Free handle
  free(p_trim_handle);

  *pp_handle = NULL;
  return TRIM_OK;
}

static int TrimMorph(void *p_handle,
                        pts_LibXmountMorphingInputFunctions p_input_functions)
{
  pts_TrimHandle p_trim_handle = (pts_TrimHandle)p_handle;
  int ret;
  uint64_t input_image_count;
  uint64_t input_image_size;

  LOG_DEBUG("Initializing LibXmount_Morphing_Trim\n");

  // Set input functions and get image count
  p_trim_handle->p_input_functions=p_input_functions;
  if(p_trim_handle->p_input_functions->ImageCount(&input_image_count)!=0)
  {
    return TRIM_CANNOT_GET_IMAGECOUNT;
  }

  if (input_image_count != 1) {
    return TRIM_INVALID_IMAGECOUNT;
  }

  // Calculate morphed image size
  ret=p_trim_handle->p_input_functions->Size(0,&input_image_size);
  if (ret!=0) {
    return TRIM_CANNOT_GET_IMAGESIZE;
  }
  if (p_trim_handle->size == 0) {
    p_trim_handle->size = input_image_size;
  }

  if (input_image_size < p_trim_handle->offset + p_trim_handle->size) {
    return TRIM_INVALID_SIZE_OFFSET;
  }

  return TRIM_OK;
}

static int TrimSize(void *p_handle, uint64_t *p_size) {
  *p_size=((pts_TrimHandle)(p_handle))->size;
  return TRIM_OK;
}

static int TrimRead(void *p_handle,
                    char *p_buf,
                    off_t offset,
                    size_t count,
                    size_t *p_read) {
  pts_TrimHandle p_trim_handle=(pts_TrimHandle)p_handle;
  off_t real_offset = p_trim_handle->offset + offset;
  int ret;
  size_t read;

  LOG_DEBUG("Reading %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset >= p_trim_handle->size ||
     offset + count > p_trim_handle->size)
  {
    return TRIM_READ_BEYOND_END_OF_IMAGE;
  }

  ret = p_trim_handle->p_input_functions->Read(0,
                                               p_buf,
                                               real_offset,
                                               count,
                                               &read);
  if (ret != 0 || read != count) {
    return TRIM_CANNOT_READ_DATA;
  }
  *p_read = read;

  return TRIM_OK;
}

static int TrimWrite(void *p_handle,
                     const char *p_buf,
                     off_t offset,
                     size_t count,
                     size_t *p_written) {
  pts_TrimHandle p_trim_handle=(pts_TrimHandle)p_handle;
  off_t real_offset = p_trim_handle->offset + offset;
  int ret;
  size_t written;

  LOG_DEBUG("Writing %zu bytes at offset %zu from morphed image\n",
            count,
            offset);

  // Make sure read parameters are within morphed image bounds
  if(offset >= p_trim_handle->size ||
     offset + count > p_trim_handle->size)
  {
    return TRIM_WRITE_BEYOND_END_OF_IMAGE;
  }

  ret = p_trim_handle->p_input_functions->Write(0,
                                                p_buf,
                                                real_offset,
                                                count,
                                                &written);
  if (ret != 0 || written != count) {
    return TRIM_CANNOT_WRITE_DATA;
  }
  *p_written = written;

  return TRIM_OK;
}

static int TrimOptionsHelp(const char **pp_help) {
  char *help = 0;
  int l = asprintf(&help,
                   "    offset         : Specified the offset within the input image where xmount should start reading. Default: 0.\n"
                   "    size           : Specified the size to which the output image will be truncated to. Default: input image size.\n");
  if (!help || l < 0) {
      return TRIM_MEMALLOC_FAILED;
  }

  *pp_help = help;
  return TRIM_OK;
}

static int TrimOptionsParse(void *p_handle,
                            uint32_t options_count,
                            const pts_LibXmountOptions *pp_options,
                            const char **pp_error)
{
  pts_TrimHandle p_trim_handle=(pts_TrimHandle)p_handle;

  for(uint32_t i=0;i<options_count;i++) {
    if(strcmp(pp_options[i]->p_key,"offset")==0) {
      uint64_t offset = strtoull(pp_options[i]->p_value, 0, 0);
      if (errno == ERANGE) {
        LOG_DEBUG("The given offset option is out of range for uint64_t.\n");
        return TRIM_INVALID_SIZE_OFFSET;
      }

      p_trim_handle->offset = offset;
      LOG_DEBUG("Setting offset to %"PRIu64".\n",p_trim_handle->offset);
      pp_options[i]->valid=1;
    }
    else if(strcmp(pp_options[i]->p_key,"size")==0) {
      uint64_t size = strtoull(pp_options[i]->p_value, 0, 0);
      if (errno == ERANGE) {
        LOG_DEBUG("The given size option is out of range for uint64_t.\n");
        return TRIM_INVALID_SIZE_OFFSET;
      }

      p_trim_handle->size = size;
      LOG_DEBUG("Setting size to %"PRIu64".\n",p_trim_handle->size);
      pp_options[i]->valid=1;
    }
  }

  if (p_trim_handle->size > UINT64_MAX - p_trim_handle->offset) {
    LOG_DEBUG("Given size is too large in combination with the offset.\n");
    return TRIM_INVALID_SIZE_OFFSET;
  }

  return TRIM_OK;
}

static int TrimGetInfofileContent(void *p_handle,
                                     const char **pp_info_buf)
{
  *pp_info_buf=NULL;
  return TRIM_OK;
}

static const char* TrimGetErrorMessage(int err_num) {
  switch(err_num) {
    case TRIM_OK:
    case TRIM_MEMALLOC_FAILED:
      return "Unable to allocate memory";
      break;
    case TRIM_CANNOT_GET_IMAGECOUNT:
      return "Unable to get input image count";
      break;
    case TRIM_INVALID_IMAGECOUNT:
      return "Unable to handle more than one image, currently";
      break;
    case TRIM_CANNOT_GET_IMAGESIZE:
      return "Unable to get input image size";
      break;
    case TRIM_INVALID_SIZE_OFFSET:
      return "Invalid size or offset";
      break;
    case TRIM_READ_BEYOND_END_OF_IMAGE:
      return "Unable to read data: Attempt to read past EOF";
      break;
    case TRIM_WRITE_BEYOND_END_OF_IMAGE:
      return "Unable to write data: Attempt to write past EOF";
      break;
    case TRIM_CANNOT_READ_DATA:
      return "Unable to read data";
      break;
    case TRIM_CANNOT_WRITE_DATA:
      return "Unable to write data";
      break;
    default:
      return "Unknown error";
  }
}

static void TrimFreeBuffer(void *p_buf) {
  free(p_buf);
}
