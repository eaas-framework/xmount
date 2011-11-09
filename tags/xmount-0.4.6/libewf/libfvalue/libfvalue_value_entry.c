/*
 * Value entry functions
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

#include <liberror.h>

#include "libfvalue_value_entry.h"

/* Initialize a value entry
 * Returns 1 if successful or -1 on error
 */
int libfvalue_value_entry_initialize(
     libfvalue_value_entry_t **value_entry,
     liberror_error_t **error )
{
	static char *function = "libfvalue_value_entry_initialize";

	if( value_entry == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid value entry.",
		 function );

		return( -1 );
	}
	if( *value_entry == NULL )
	{
		*value_entry = (libfvalue_value_entry_t *) memory_allocate(
		                                            sizeof( libfvalue_value_entry_t ) );

		if( *value_entry == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create value entry.",
			 function );

			return( -1 );
		}
		if( memory_set(
		     *value_entry,
		     0,
		     sizeof( libfvalue_value_entry_t ) ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_SET_FAILED,
			 "%s: unable to clear value entry.",
			 function );

			memory_free(
			 *value_entry );

			*value_entry = NULL;

			return( -1 );
		}
	}
	return( 1 );
}

/* Frees a value entry
 * Returns 1 if successful or -1 on error
 */
int libfvalue_value_entry_free(
     intptr_t *value_entry,
     liberror_error_t **error )
{
	static char *function = "libfvalue_value_entry_free";

	if( value_entry == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid value entry.",
		 function );

		return( -1 );
	}
	memory_free(
	 value_entry );

	return( 1 );
}

/* Clones a value entry
 * Returns 1 if successful or -1 on error
 */
int libfvalue_value_entry_clone(
     intptr_t **destination_value_entry,
     intptr_t *source_value_entry,
     liberror_error_t **error )
{
	static char *function = "libfvalue_value_entry_clone";

	if( destination_value_entry == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid destination value entry.",
		 function );

		return( -1 );
	}
	if( *destination_value_entry != NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: destination value entry already set.",
		 function );

		return( -1 );
	}
	if( source_value_entry == NULL )
	{
		*destination_value_entry = NULL;

		return( 1 );
	}
	*destination_value_entry = (intptr_t *) memory_allocate(
	                                         sizeof( libfvalue_value_entry_t ) );

	if( *destination_value_entry == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create destination value entry.",
		 function );

		return( -1 );
	}
	if( memory_copy(
	     *destination_value_entry,
	     source_value_entry,
	     sizeof( libfvalue_value_entry_t ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_COPY_FAILED,
		 "%s: unable to copy value entry.",
		 function );

		memory_free(
		 *destination_value_entry );

		*destination_value_entry = NULL;

		return( -1 );
	}
	return( 1 );
}

