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

#ifndef DD_H
#define DD_H

typedef struct _t_dd *t_pdd;

// Possible error codes
enum
{
    DD_OK = 0,
    DD_FOUND,

    DD_MEMALLOC_FAILED=100,
    DD_FILE_OPEN_FAILED,
    DD_CANNOT_READ_DATA,
    DD_CANNOT_CLOSE_FILE,
    DD_CANNOT_SEEK,
    DD_READ_BEYOND_END_OF_IMAGE
};

#endif

