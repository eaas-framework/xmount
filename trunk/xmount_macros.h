/*******************************************************************************
* xmount Copyright (c) 2008,2009 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#ifndef XMOUNT_MACROS_H
#define XMOUNT_MACROS_H

#include <stdlib.h>
#include <string.h>

/*
 * Macros to alloc or realloc memory and check whether it worked
 */
#define XMOUNT_MALLOC(var,var_type,size) { \
  (var)=(var_type)malloc(size); \
  if((var)==NULL) { \
    LOG_ERROR("Couldn't allocate memmory!\n"); \
    exit(1); \
  } \
}
#define XMOUNT_REALLOC(var,var_type,size) { \
  (var)=(var_type)realloc((var),size); \
  if((var)==NULL) { \
    LOG_ERROR("Couldn't allocate memmory!\n"); \
    exit(1); \
  } \
}

/*
 * Macros for some often used string functions
 */
#define XMOUNT_STRSET(var1,var2) { \
  XMOUNT_MALLOC(var1,char*,strlen(var2)+1) \
  strcpy(var1,var2); \
}
#define XMOUNT_STRNSET(var1,var2,size) { \
  XMOUNT_MALLOC(var1,char*,(size)+1) \
  strncpy(var1,var2,size); \
  (var1)[size]='\0'; \
}
#define XMOUNT_STRAPP(var1,var2) { \
  XMOUNT_REALLOC(var1,char*,strlen(var1)+strlen(var2)+1) \
  strcpy((var1)+strlen(var1),var2); \
}
#define XMOUNT_STRNAPP(var1,var2,size) { \
  XMOUNT_REALLOC(var1,char*,strlen(var1)+(size)+1) \
  (var1)[strlen(var1)+(size)]='\0'; \
  strncpy((var1)+strlen(var1),var2,size); \
}

#endif // #ifndef XMOUNT_MACROS_H

