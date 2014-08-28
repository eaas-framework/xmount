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
#include <stdint.h>
#include <errno.h>

#include "libxmount.h"

/*
 * StrToInt32
 */
int32_t StrToInt32(const char *p_value, int *p_ok) {
  long int num;
  char *p_tail;

  errno=0;
  num=strtol(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num<INT32_MIN || num>INT32_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (int32_t)num;
}

/*
 * StrToUint32
 */
uint32_t StrToUint32(const char *p_value, int *p_ok) {
  unsigned long int num;
  char *p_tail;

  errno=0;
  num=strtoul(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num>UINT32_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (uint32_t)num;
}

/*
 * StrToInt64
 */
int64_t StrToInt64(const char *p_value, int *p_ok) {
  long long int num;
  char *p_tail;

  errno=0;
  num=strtoll(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num<INT64_MIN || num>INT64_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (int64_t)num;
}

/*
 * StrToUint64
 */
uint64_t StrToUint64(const char *p_value, int *p_ok) {
  unsigned long long int num;
  char *p_tail;

  errno=0;
  num=strtoull(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num>UINT64_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (uint64_t)num;
}

