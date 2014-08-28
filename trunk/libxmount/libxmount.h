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

#ifndef LIBXMOUNT_H
#define LIBXMOUNT_H

//! Struct containing lib options
typedef struct s_LibXmountOptions {
  //! Option name
  char *p_key;
  //! Option value
  char *p_value;
  //! Set to 1 if key/value has been parsed and is valid
  uint8_t valid;
} ts_LibXmountOptions, *pts_LibXmountOptions;

int32_t StrToInt32(const char *p_value, int *p_ok);
uint32_t StrToUint32(const char *p_value, int *p_ok);
int64_t StrToInt64(const char *p_value, int *p_ok);
uint64_t StrToUint64(const char *p_value, int *p_ok);

#endif // LIBXMOUNT_H

