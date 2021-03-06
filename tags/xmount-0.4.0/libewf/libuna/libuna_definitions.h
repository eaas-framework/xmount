/*
 * The internal definitions
 *
 * Copyright (c) 2008-2009, Joachim Metz <forensics@hoffmannbv.nl>,
 * Hoffmann Investigations. All rights reserved.
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined( _LIBUNA_INTERNAL_DEFINITIONS_H )
#define _LIBUNA_INTERNAL_DEFINITIONS_H

#include <common.h>

/* Define HAVE_LOCAL_LIBUNA for local use of libuna
 */
#if !defined( HAVE_LOCAL_LIBUNA )
#include <libuna/definitions.h>

/* The definitions in <libuna/definitions.h> are copied here
 * for local use of libuna
 */
#else
#define LIBUNA_VERSION					20090512

/* The libuna version string
 */
#define LIBUNA_VERSION_STRING				"20090512"

/* The endian definitions
 */
enum LIBUNA_ENDIAN
{
	LIBUNA_ENDIAN_BIG                               = (int) 'b',
	LIBUNA_ENDIAN_LITTLE                            = (int) 'l'
};

/* The codepage definitions
 */
enum LIBUNA_CODEPAGE
{
	LIBUNA_CODEPAGE_ASCII                           = (int) 'A',
	LIBUNA_CODEPAGE_WINDOWS_1250                    = 1250,
	LIBUNA_CODEPAGE_WINDOWS_1251                    = 1251,
	LIBUNA_CODEPAGE_WINDOWS_1252                    = 1252,
	LIBUNA_CODEPAGE_WINDOWS_1253                    = 1253,
	LIBUNA_CODEPAGE_WINDOWS_1254                    = 1254,
	LIBUNA_CODEPAGE_WINDOWS_1255                    = 1255,
	LIBUNA_CODEPAGE_WINDOWS_1256                    = 1256,
	LIBUNA_CODEPAGE_WINDOWS_1257                    = 1257,
	LIBUNA_CODEPAGE_WINDOWS_1258                    = 1258
};

#define LIBUNA_CODEPAGE_WINDOWS_CENTRAL_EUROPEAN	LIBUNA_CODEPAGE_WINDOWS_1250
#define LIBUNA_CODEPAGE_WINDOWS_CYRILLIC		LIBUNA_CODEPAGE_WINDOWS_1251
#define LIBUNA_CODEPAGE_WINDOWS_WESTERN_EUROPEAN	LIBUNA_CODEPAGE_WINDOWS_1252
#define LIBUNA_CODEPAGE_WINDOWS_GREEK			LIBUNA_CODEPAGE_WINDOWS_1253
#define LIBUNA_CODEPAGE_WINDOWS_TURKISH			LIBUNA_CODEPAGE_WINDOWS_1254
#define LIBUNA_CODEPAGE_WINDOWS_HEBREW			LIBUNA_CODEPAGE_WINDOWS_1255
#define LIBUNA_CODEPAGE_WINDOWS_ARABIC			LIBUNA_CODEPAGE_WINDOWS_1256
#define LIBUNA_CODEPAGE_WINDOWS_BALTIC			LIBUNA_CODEPAGE_WINDOWS_1257
#define LIBUNA_CODEPAGE_WINDOWS_VIETNAMESE		LIBUNA_CODEPAGE_WINDOWS_1258

#endif

/* Character definitions
 */
#define LIBUNA_UNICODE_REPLACEMENT_CHARACTER		0x0000fffd
#define LIBUNA_UNICODE_BASIC_MULTILINGUAL_PLANE_MAX	0x0000ffff
#define LIBUNA_UNICODE_SURROGATE_LOW_RANGE_START	0x0000dc00
#define LIBUNA_UNICODE_SURROGATE_LOW_RANGE_END		0x0000dfff
#define LIBUNA_UNICODE_SURROGATE_HIGH_RANGE_START	0x0000d800
#define LIBUNA_UNICODE_SURROGATE_HIGH_RANGE_END		0x0000dbff
#define LIBUNA_UNICODE_CHARACTER_MAX			0x0010ffff

#define LIBUNA_UTF16_CHARACTER_MAX			0x0010ffff
#define LIBUNA_UTF32_CHARACTER_MAX			0x7fffffff

#define LIBUNA_ASCII_REPLACEMENT_CHARACTER		0x1a

#endif

