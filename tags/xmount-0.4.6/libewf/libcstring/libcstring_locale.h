/*
 * Locale functions
 *
 * Copyright (c) 2010-2011, Joachim Metz <jbmetz@users.sourceforge.net>
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

#if !defined( _LIBCSTRING_LOCALE_H )
#define _LIBCSTRING_LOCALE_H

#include <common.h>
#include <types.h>

#if defined( __cplusplus )
extern "C" {
#endif

#if defined( WINAPI ) && ( WINVER < 0x0500 )
int libcstring_GetLocaleInfo(
     LCID locale,
     LCTYPE lctype,
     LPSTR buffer,
     int size );
#endif

int libcstring_locale_get_codepage(
     void );

int libcstring_locale_get_decimal_point(
     void );

#if defined( __cplusplus )
}
#endif

#endif

