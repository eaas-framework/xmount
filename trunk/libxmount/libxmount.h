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

#ifndef LIBXMOUNT_H
#define LIBXMOUNT_H

#include "endianness.h"

/*
 * Under OSx, fopen handles 64bit I/O too
 */
#ifdef __APPLE__
  #define fopen64 fopen
  #define fseeko64 fseeko
#endif

/*
 * Macros to ease debugging and error reporting
 */
#define LIBXMOUNT_LOG_ERROR(...) {                              \
  LogMessage("ERROR",(char*)__FUNCTION__,__LINE__,__VA_ARGS__); \
}
#define LIBXMOUNT_LOG_WARNING(...) {                              \
  LogMessage("WARNING",(char*)__FUNCTION__,__LINE__,__VA_ARGS__); \
}
#define LIBXMOUNT_LOG_DEBUG(debug,...) {                                  \
  if(debug) LogMessage("DEBUG",(char*)__FUNCTION__,__LINE__,__VA_ARGS__); \
}

//! Struct containing lib options
typedef struct s_LibXmountOptions {
  //! Option name
  char *p_key;
  //! Option value
  char *p_value;
  //! Set to 1 if key/value has been parsed and is valid
  uint8_t valid;
} ts_LibXmountOptions, *pts_LibXmountOptions;

//! Log messages
/*!
 * \param p_msg_type "ERROR", "DEBUG", etc...
 * \param p_calling_fun Name of calling function
 * \param line Line number of call
 * \param p_msg Message string
 * \param ... Variable params with values to include in message string
 */
void LogMessage(char *p_msg_type,
                char *p_calling_fun,
                int line,
                char *p_msg,
                ...);

int32_t StrToInt32(const char *p_value, int *p_ok);
uint32_t StrToUint32(const char *p_value, int *p_ok);
int64_t StrToInt64(const char *p_value, int *p_ok);
uint64_t StrToUint64(const char *p_value, int *p_ok);

#endif // LIBXMOUNT_H

