/*
 * Crypographic digest hash
 *
 * Copyright (c) 2006-2009, Joachim Metz <forensics@hoffmannbv.nl>,
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

#include <common.h>
#include <types.h>

#include <liberror.h>

#include "digest_hash.h"
#include "system_string.h"

/* Converts the EWF digest hash to a printable string
 * Returns 1 if successful or -1 on error
 */
int digest_hash_copy_to_string(
     digest_hash_t *digest_hash,
     size_t digest_hash_size,
     system_character_t *string,
     size_t string_size,
     liberror_error_t **error )
{
	static char *function       = "digest_hash_copy_to_string";
	size_t string_iterator      = 0;
	size_t digest_hash_iterator = 0;
	uint8_t digest_digit        = 0;

	if( digest_hash == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid digest hash.",
		 function );

		return( -1 );
	}
	if( digest_hash_size > (size_t) SSIZE_MAX )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid digest hash size value exceeds maximum.",
		 function );

		return( -1 );
	}
	if( string == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid string.",
		 function );

		return( -1 );
	}
	if( string_size > (size_t) SSIZE_MAX )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_EXCEEDS_MAXIMUM,
		 "%s: invalid string size value exceeds maximum.",
		 function );

		return( -1 );
	}
	/* The string requires space for 2 characters per digest hash digit and a end of string
	 */
	if( string_size < ( ( 2 * digest_hash_size ) + 1 ) )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_TOO_SMALL,
		 "%s: string too small.",
		 function );

		return( -1 );
	}
	for( digest_hash_iterator = 0; digest_hash_iterator < digest_hash_size; digest_hash_iterator++ )
	{
		digest_digit = digest_hash[ digest_hash_iterator ] / 16;

		if( digest_digit <= 9 )
		{
			string[ string_iterator++ ] = (system_character_t) ( (uint8_t) '0' + digest_digit );
		}
		else
		{
			string[ string_iterator++ ] = (system_character_t) ( (uint8_t) 'a' + ( digest_digit - 10 ) );
		}
		digest_digit = digest_hash[ digest_hash_iterator ] % 16;

		if( digest_digit <= 9 )
		{
			string[ string_iterator++ ] = (system_character_t) ( (uint8_t) '0' + digest_digit );
		}
		else
		{
			string[ string_iterator++ ] = (system_character_t) ( (uint8_t) 'a' + ( digest_digit - 10 ) );
		}
	}
	string[ string_iterator ] = 0;

	return( 1 );
}

