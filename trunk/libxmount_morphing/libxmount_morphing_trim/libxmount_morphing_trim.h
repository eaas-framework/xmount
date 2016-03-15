#ifndef LIBXMOUNT_MORPHING_TRIM_H
#define LIBXMOUNT_MORPHING_TRIM_H

enum {
  TRIM_OK=0,
  TRIM_MEMALLOC_FAILED,
  TRIM_CANNOT_GET_IMAGECOUNT,
  TRIM_INVALID_IMAGECOUNT,
  TRIM_CANNOT_GET_IMAGESIZE,
  TRIM_INVALID_SIZE_OFFSET,
  TRIM_READ_BEYOND_END_OF_IMAGE,
  TRIM_WRITE_BEYOND_END_OF_IMAGE,
  TRIM_CANNOT_READ_DATA,
  TRIM_CANNOT_WRITE_DATA
};

typedef struct s_TrimHandle {
  uint8_t debug;
  pts_LibXmountMorphingInputFunctions p_input_functions;
  uint64_t offset;
  uint64_t size;
} ts_TrimHandle, *pts_TrimHandle;

static int TrimCreateHandle(void **pp_handle,
                            const char *p_format,
                            uint8_t debug);
static int TrimDestroyHandle(void **pp_handle);
static int TrimMorph(void *p_handle,
                     pts_LibXmountMorphingInputFunctions p_input_functions);
static int TrimSize(void *p_handle,
                    uint64_t *p_size);
static int TrimRead(void *p_handle,
                    char *p_buf,
                    off_t offset,
                    size_t count,
                    size_t *p_read);
static int TrimWrite(void *p_handle,
                     const char *p_buf,
                     off_t offset,
                     size_t count,
                     size_t *p_written);
static int TrimOptionsHelp(const char **pp_help);
static int TrimOptionsParse(void *p_handle,
                            uint32_t options_count,
                            const pts_LibXmountOptions *pp_options,
                            const char **pp_error);
static int TrimGetInfofileContent(void *p_handle,
                                  const char **pp_info_buf);
static const char* TrimGetErrorMessage(int err_num);
static void TrimFreeBuffer(void *p_buf);

#endif
