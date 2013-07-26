/*******************************************************************************
* xmount Copyright (c) 2008,2009, 2010, 2011, 2012                             *
*                      by Gillen Daniel <gillen.dan@pinguin.lu>                *
*                                                                              *
* This module has been written by Guy Voncken. It contains the functions for   *
* accessing simple AFF images created by Guymager.                             *
*                                                                              *
* xmount is a small tool to "fuse mount" various image formats as dd or vdi    *
* files and enable virtual write access.                                       *
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

#ifndef AAFF_H
#define AAFF_H

typedef struct _t_Aaff *t_pAaff;

int AaffOpen  (t_pAaff *ppAaff, const char *pFilename, unsigned long long MaxPageArrMem);
int AaffInfo  (t_pAaff   pAaff, char **ppInfoBuff);
int AaffRead  (t_pAaff   pAaff, unsigned long long Seek, unsigned char *pBuffer, unsigned int Count);
int AaffClose (t_pAaff *ppAaff);


// Possible error codes
enum
{
    AAFF_OK = 0,
    AAFF_FOUND,

    AAFF_MEMALLOC_FAILED=100,
    AAFF_FILE_OPEN_FAILED,
    AAFF_INVALID_SIGNATURE,
    AAFF_CANNOT_READ_DATA,
    AAFF_INVALID_HEADER,
    AAFF_INVALID_FOOTER,                // 105
    AAFF_TOO_MANY_HEADER_SEGEMENTS,
    AAFF_NOT_CREATED_BY_GUYMAGER,
    AAFF_INVALID_PAGE_NUMBER,
    AAFF_UNEXPECTED_PAGE_NUMBER,
    AAFF_CANNOT_CLOSE_FILE,             // 110
    AAFF_CANNOT_SEEK,
    AAFF_WRONG_SEGMENT,
    AAFF_UNCOMPRESS_FAILED,
    AAFF_INVALID_PAGE_ARGUMENT,
    AAFF_SEEKARR_CORRUPT,               // 115
    AAFF_PAGE_NOT_FOUND,
    AAFF_READ_BEYOND_IMAGE_LENGTH,
    AAFF_READ_BEYOND_LAST_PAGE
};


#endif
