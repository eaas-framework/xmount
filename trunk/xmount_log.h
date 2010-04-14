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

#ifndef XMOUNT_LOG_H
#define XMOUNT_LOG_H

#include <stdarg.h>

#define PACKAGE_VERSION "0.0.0"

/*
 * Macros to ease debugging and error reporting
 */
#define LOG_ERROR(...) \
  xmlog_message("ERROR",(char*)__FUNCTION__,__LINE__,__VA_ARGS__);

#define LOG_DEBUG(...) { \
  if(XMOptions.Debug) \
    xmlog_message("DEBUG",(char*)__FUNCTION__,__LINE__,__VA_ARGS__); \
}

/******************************** Public API **********************************/
 
/*
 * xmlog_message:
 *   Print error and debug messages to stdout
 *
 * Params:
 *  pMessageType: "ERROR" or "DEBUG"
 *  pCallingFunction: Name of calling function
 *  line: Line number of call
 *  pMessage: Message string
 *  ...: Variable params with values to include in message string
 *
 * Returns:
 *   n/a
 */
void xmlog_message(char *pMessageType,
                   char *pCallingFunction,
                   int line,
                   char *pMessage,
                   ...);

/*
 * xmlog_warn:
 *   Print warning messages to stdout
 *
 * Params:
 *  pMessage: Message string
 *  ...: Variable params with values to include in message string
 *
 * Returns:
 *   n/a
 */
void xmlog_warn(char *pMessage,
                ...);

#endif // #ifndef XMOUNT_LOG_H

