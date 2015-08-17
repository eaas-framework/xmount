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

#include "ntfs_functions.h"
#include "libxmount_morphing_unallocated_retvalues.h"

#define LOG_DEBUG(...) {                                   \
    LIBXMOUNT_LOG_DEBUG(p_ntfs_handle->debug,__VA_ARGS__); \
}

/*
 * ReadNtfsHeader
 */
int ReadNtfsHeader(pts_NtfsHandle p_ntfs_handle,
                   pts_LibXmountMorphingInputFunctions p_input_functions,
                   uint8_t debug)
{
  
}

