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

#include "xmount_log.h"

#include <stdio.h>

/*
 * xmlog_message
 */
void xmlog_message(char *pMessageType,
                   char *pCallingFunction,
                   int line,
                   char *pMessage,
                   ...)
{
  va_list VaList;

  // Print message "header"
  printf("%s: %s.%s@%u : ",pMessageType,pCallingFunction,PACKAGE_VERSION,line);
  // Print message with variable parameters
  va_start(VaList,pMessage);
  vprintf(pMessage,VaList);
  va_end(VaList);
}

/*
 * xmlog_warn
 */
void xmlog_warn(char *pMessage,...) {
  va_list VaList;

  // Print message "header"
  printf("WARNING: ");
  // Print message with variable parameters
  va_start(VaList,pMessage);
  vprintf(pMessage,VaList);
  va_end(VaList);
}

