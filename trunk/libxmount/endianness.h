/*******************************************************************************
* Copyright (c) 2008-2015 by Gillen Daniel <gillen.dan@pinguin.lu>             *
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

/*
 * This file aims at making endianness conversions using the following
 * functions portable.
 *
 * Conversion from big endian to host / from host to big endian
 * be16toh(uint16_t)
 * be32toh(uint32_t)
 * be64toh(uint64_t)
 * htobe16(uint16_t)
 * htobe32(uint32_t)
 * htobe64(uint64_t)
 *
 * Conversion from little endian to host / from host to little endian
 * le16toh(uint16_t)
 * le32toh(uint32_t)
 * le64toh(uint64_t)
 * htole16(uint16_t)
 * htole32(uint32_t)
 * htole64(uint64_t)
 *
 * In addition, including this file will define HOST_BYTEORDER_IS_BE or
 * HOST_BYTEORDER_IS_LE according to the current byte order of the host.
 */

#ifndef ENDIANNESS_H
#define ENDIANNESS_H

#include <config.h>

#ifdef HAVE_STDINT_H
  #include <stdint.h>
#else
  #error "Including this file requires you to have stdint.h"
#endif

#ifdef HAVE_ENDIAN_H
  #include <endian.h>
#endif

// First we need to have the bswap functions
#if defined(HAVE_BYTESWAP_H)
  #include <byteswap.h>
#elif defined(__APPLE__) && defined(HAVE_LIBKERN_OSBYTEORDER_H)
  #include <libkern/OSByteOrder.h>
  #define bswap_16 OSSwapInt16
  #define bswap_32 OSSwapInt32
  #define bswap_64 OSSwapInt64
#else
  #define	bswap_16(value) {                    \
    ((((value) & 0xff) << 8) | ((value) >> 8)) \
  }
  #define	bswap_32(value)	{                                     \
 	  (((uint32_t)bswap_16((uint16_t)((value) & 0xffff)) << 16) | \
 	  (uint32_t)bswap_16((uint16_t)((value) >> 16)))              \
 	}
  #define	bswap_64(value)	{                                         \
 	  (((uint64_t)bswap_32((uint32_t)((value) & 0xffffffff)) << 32) | \
 	  (uint64_t)bswap_32((uint32_t)((value) >> 32)))                  \
 	}
#endif

// Next we need to know what endianness is used
#if defined(__LITTLE_ENDIAN__)
  #define HOST_BYTEORDER_IS_LE
#elif defined(__BIG_ENDIAN__)
  #define HOST_BYTEORDER_IS_BE
#elif defined(__BYTE_ORDER)
  #if __BYTE_ORDER == __LITTLE_ENDIAN
    #define HOST_BYTEORDER_IS_LE
  #else
    #define HOST_BYTEORDER_IS_BE
  #endif
#elif defined(__BYTE_ORDER__)
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define HOST_BYTEORDER_IS_LE
  #else
    #define HOST_BYTEORDER_IS_BE
  #endif
#else
  #error "Unable to determine host byteorder"
#endif

// And finally we can define the endianness conversion macros
#ifdef HOST_BYTEORDER_IS_LE
  #ifndef be16toh
    #define be16toh(x) bswap_16(x)
  #endif
  #ifndef htobe16
    #define htobe16(x) bswap_16(x)
  #endif
  #ifndef be32toh
    #define be32toh(x) bswap_32(x)
  #endif
  #ifndef htobe32
    #define htobe32(x) bswap_32(x)
  #endif
  #ifndef be64toh
    #define be64toh(x) bswap_64(x)
  #endif
  #ifndef htobe64
    #define htobe64(x) bswap_64(x)
  #endif
  #ifndef le16toh
    #define le16toh(x) (x)
  #endif
  #ifndef htole16
    #define htole16(x) (x)
  #endif
  #ifndef le32toh
    #define le32toh(x) (x)
  #endif
  #ifndef htole32
    #define htole32(x) (x)
  #endif
  #ifndef le64toh
    #define le64toh(x) (x)
  #endif
  #ifndef htole64
    #define htole64(x) (x)
  #endif
#else
  #ifndef be16toh
    #define be16toh(x) (x)
  #endif
  #ifndef htobe16
    #define htobe16(x) (x)
  #endif
  #ifndef be32toh
    #define be32toh(x) (x)
  #endif
  #ifndef htobe32
    #define htobe32(x) (x)
  #endif
  #ifndef be64toh
    #define be64toh(x) (x)
  #endif
  #ifndef htobe64
    #define htobe64(x) (x)
  #endif
  #ifndef le16toh
    #define le16toh(x) bswap_16(x)
  #endif
  #ifndef htole16
    #define htole16(x) bswap_16(x)
  #endif
  #ifndef le32toh
    #define le32toh(x) bswap_32(x)
  #endif
  #ifndef htole32
    #define htole32(x) bswap_32(x)
  #endif
  #ifndef le64toh
    #define le64toh(x) bswap_64(x)
  #endif
  #ifndef htole64
    #define htole64(x) bswap_64(x)
  #endif
#endif

#endif //ENDIANNESS_H

