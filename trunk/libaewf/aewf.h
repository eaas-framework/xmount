/*******************************************************************************
* xmount Copyright (c) 2008-2014 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing EWF images created by Guymager and others.                         *
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

#ifndef AEWF_H
#define AEWF_H

typedef struct _t_Aewf *t_pAewf;

int AewfOptionHelp (const char **pHelp);
int AewfOpen       (t_pAewf *ppAewf, unsigned FilenameArrLen, const char **ppFilenameArr);
int AewfOptions    (t_pAewf   pAewf, char *pOptions, char **pError);
int AewfInfo       (t_pAewf   pAewf, const char **ppInfoBuff);
int AewfSize       (t_pAewf   pAewf, unsigned long long *pSize);
int AewfRead       (t_pAewf   pAewf, unsigned long long Seek, unsigned char *pBuffer, unsigned int Count);
int AewfClose      (t_pAewf *ppAewf);


// Possible error codes for the above functions
enum
{
   AEWF_OK = 0,
   AEWF_FOUND,

   AEWF_MEMALLOC_FAILED=100,           // 100
   AEWF_FILE_OPEN_FAILED,
   AEWF_FILE_CLOSE_FAILED,
   AEWF_FILE_SEEK_FAILED,
   AEWF_FILE_READ_FAILED,
   AEWF_READFILE_BAD_MEM,              // 105
   AEWF_INVALID_SEGMENT_NUMBER,
   AEWF_WRONG_SEGMENT_FILE_COUNT,
   AEWF_VOLUME_MUST_PRECEDE_TABLES,
   AEWF_SECTORS_MUST_PRECEDE_TABLES,
   AEWF_WRONG_CHUNK_COUNT,             // 110
   AEWF_READ_BEYOND_IMAGE_LENGTH,
   AEWF_CHUNK_NOT_FOUND,
   AEWF_VOLUME_MISSING,
   AEWF_ERROR_EWF_TABLE_NOT_READY,
   AEWF_ERROR_EWF_SEGMENT_NOT_READY,   // 115
   AEWF_CHUNK_TOO_BIG,
   AEWF_UNCOMPRESS_FAILED,
   AEWF_BAD_UNCOMPRESSED_LENGTH,
   AEWF_CHUNK_CRC_ERROR,
   AEWF_ERROR_IN_CHUNK_NUMBER,         // 120
   AEWF_VASPRINTF_FAILED,
   AEWF_UNCOMPRESS_HEADER_FAILED,
   AEWF_ASPRINTF_FAILED,
};


#endif
