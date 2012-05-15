/*
 * Support functions
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

#include <common.h>
#include <memory.h>
#include <types.h>

#include <libcstring.h>
#include <liberror.h>

#include "libodraw_codepage.h"
#include "libodraw_definitions.h"
#include "libodraw_io_handle.h"
#include "libodraw_libbfio.h"
#include "libodraw_support.h"

#if !defined( HAVE_LOCAL_LIBODRAW )

/* Returns the library version
 */
const char *libodraw_get_version(
             void )
{
	return( (const char *) LIBODRAW_VERSION_STRING );
}

/* Returns the access flags for reading
 */
int libodraw_get_access_flags_read(
     void )
{
	return( (int) LIBODRAW_ACCESS_FLAG_READ );
}

/* Returns the access flags for reading and writing
 */
int libodraw_get_access_flags_read_write(
     void )
{
	return( (int) ( LIBODRAW_ACCESS_FLAG_READ | LIBODRAW_ACCESS_FLAG_WRITE ) );
}

/* Returns the access flags for writing
 */
int libodraw_get_access_flags_write(
     void )
{
	return( (int) LIBODRAW_ACCESS_FLAG_WRITE );
}

/* Retrieves the narrow system string codepage
 * A value of 0 represents no codepage, UTF-8 encoding is used instead
 * Returns 1 if successful or -1 on error
 */
int libodraw_get_codepage(
     int *codepage,
     liberror_error_t **error )
{
	static char *function = "libodraw_get_codepage";

	if( codepage == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid codepage.",
		 function );

		return( -1 );
	}
	*codepage = libcstring_narrow_system_string_codepage;

	return( 1 );
}

/* Sets the narrow system string codepage
 * A value of 0 represents no codepage, UTF-8 encoding is used instead
 * Returns 1 if successful or -1 on error
 */
int libodraw_set_codepage(
     int codepage,
     liberror_error_t **error )
{
	static char *function = "libodraw_set_codepage";

	if( ( codepage != LIBODRAW_CODEPAGE_ASCII )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_1 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_2 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_3 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_4 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_5 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_6 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_7 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_8 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_9 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_10 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_11 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_13 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_14 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_15 )
	 && ( codepage != LIBODRAW_CODEPAGE_ISO_8859_16 )
	 && ( codepage != LIBODRAW_CODEPAGE_KOI8_R )
	 && ( codepage != LIBODRAW_CODEPAGE_KOI8_U )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_874 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1250 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1251 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1252 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1253 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1254 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1256 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1257 )
	 && ( codepage != LIBODRAW_CODEPAGE_WINDOWS_1258 )
	 && ( codepage != 0 ) )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported codepage.",
		 function );

		return( -1 );
	}
	libcstring_narrow_system_string_codepage = codepage;

	return( 1 );
}

#endif /* !defined( HAVE_LOCAL_LIBODRAW ) */

