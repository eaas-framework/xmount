/*******************************************************************************
* xmount Copyright (c) 2008-2013 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing dd images. Split dd is supported as well.                          *
*                                                                              *
* xmount is a small tool to "fuse mount" various harddisk image formats as dd, *
* vdi, vhd or vmdk files and enable virtual write access to them.              *
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

#ifndef LIBXMOUNT_INPUT_DD_H
#define LIBXMOUNT_INPUT_DD_H

/*******************************************************************************
 * Error codes etc...
 ******************************************************************************/
enum {
  DD_OK=0,
  DD_MEMALLOC_FAILED,
  DD_FILE_OPEN_FAILED,
  DD_CANNOT_READ_DATA,
  DD_CANNOT_CLOSE_FILE,
  DD_CANNOT_SEEK,
  DD_READ_BEYOND_END_OF_IMAGE
};

// ----------------------
//  Constant definitions
// ----------------------

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))

// ---------------------
//  Types and strutures
// ---------------------

typedef struct {
  char     *pFilename;
  uint64_t   FileSize;
  FILE     *pFile;
} t_Piece, *t_pPiece;

typedef struct {
  t_pPiece  pPieceArr;
  uint64_t   Pieces;
  uint64_t   TotalSize;
} t_dd, *t_pdd;

// ----------------
//  Error handling
// ----------------

#ifdef DD_DEBUG
   #define CHK(ChkVal)    \
   {                                                                  \
      int ChkValRc;                                                   \
      if ((ChkValRc=(ChkVal)) != DD_OK)                               \
      {                                                               \
         printf ("Err %d in %s, %d\n", ChkValRc, __FILE__, __LINE__); \
         return ChkValRc;                                             \
      }                                                               \
   }
   #define DEBUG_PRINTF(pFormat, ...) \
      printf (pFormat, ##__VA_ARGS__);
#else
   #define CHK(ChkVal)                      \
   {                                        \
      int ChkValRc;                         \
      if ((ChkValRc=(ChkVal)) != DD_OK)     \
         return ChkValRc;                   \
   }
   #define DEBUG_PRINTF(...)
#endif

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
static int DdCreateHandle(void **pp_handle, char *p_format);
static int DdDestroyHandle(void **pp_handle);
static int DdOpen(void **pp_handle,
                  const char **pp_filename_arr,
                  uint64_t filename_arr_len);
static int DdClose(void **pp_handle);
static int DdSize(void *p_handle,
                  uint64_t *p_size);
static int DdRead(void *p_handle,
                  uint64_t seek,
                  char *p_buf,
                  uint32_t count);
static const char* DdOptionsHelp();
static int DdOptionsParse(void *p_handle,
                          char *p_options,
                          char **pp_error);
static int DdGetInfofileContent(void *p_handle,
                                char **pp_info_buf);
static const char* DdGetErrorMessage(int err_num);
static void DdFreeBuffer(void *p_buf);

#endif // LIBXMOUNT_INPUT_DD_H

