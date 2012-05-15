/*
 * The group functions
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

#include "libmfdata_group.h"

/* Initializes the group
 * Returns 1 if successful or -1 on error
 */
int libmfdata_group_initialize(
     libmfdata_group_t **group,
     liberror_error_t **error )
{
	static char *function = "libmfdata_group_initialize";

	if( group == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid group.",
		 function );

		return( -1 );
	}
	if( *group == NULL )
	{
		*group = memory_allocate_structure(
		          libmfdata_group_t );

		if( *group == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create group.",
			 function );

			goto on_error;
		}
		if( memory_set(
		     *group,
		     0,
		     sizeof( libmfdata_group_t ) ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_SET_FAILED,
			 "%s: unable to clear group.",
			 function );

			goto on_error;
		}
	}
	return( 1 );

on_error:
	if( *group != NULL )
	{
		memory_free(
		 *group );

		*group = NULL;
	}
	return( -1 );
}

/* Frees the group
 * Returns 1 if successful or -1 on error
 */
int libmfdata_group_free(
     libmfdata_group_t **group,
     liberror_error_t **error )
{
	static char *function = "libmfdata_group_free";

	if( group == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid group.",
		 function );

		return( -1 );
	}
	if( *group != NULL )
	{
		memory_free(
		 *group );

		*group = NULL;
	}
	return( 1 );
}

/* Clones (duplicates) the group
 * Returns 1 if successful or -1 on error
 */
int libmfdata_group_clone(
     libmfdata_group_t **destination_group,
     libmfdata_group_t *source_group,
     liberror_error_t **error )
{
	static char *function = "libmfdata_group_clone";

	if( destination_group == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid destination group.",
		 function );

		return( -1 );
	}
	if( *destination_group != NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: destination group already set.",
		 function );

		return( -1 );
	}
	if( source_group == NULL )
	{
		*destination_group = NULL;

		return( 1 );
	}
	*destination_group = memory_allocate_structure(
	                      libmfdata_group_t );

	if( *destination_group == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create destination group.",
		 function );

		goto on_error;
	}
	if( memory_copy(
	     *destination_group,
	     source_group,
	     sizeof( libmfdata_group_t ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_COPY_FAILED,
		 "%s: unable to copy source to destination group.",
		 function );

		goto on_error;
	}
	return( 1 );

on_error:
	if( *destination_group != NULL )
	{
		memory_free(
		 *destination_group );

		*destination_group = NULL;
	}
	return( -1 );
}

/* Retrieves the group values
 * Returns 1 if successful or -1 on error
 */
int libmfdata_group_get_values(
     libmfdata_group_t *group,
     int *number_of_elements,
     liberror_error_t **error )
{
	static char *function = "libmfdata_group_get_values";

	if( group == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid group.",
		 function );

		return( -1 );
	}
	if( number_of_elements == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid number of elements.",
		 function );

		return( -1 );
	}
	*number_of_elements = group->number_of_elements;

	return( 1 );
}

/* Sets the group values
 * Returns 1 if successful or -1 on error
 */
int libmfdata_group_set_values(
     libmfdata_group_t *group,
     int number_of_elements,
     liberror_error_t **error )
{
	static char *function = "libmfdata_group_set_values";

	if( group == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid group.",
		 function );

		return( -1 );
	}
	if( number_of_elements < 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_LESS_THAN_ZERO,
		 "%s: invalid number of elements value less than zero.",
		 function );

		return( -1 );
	}
	group->number_of_elements = number_of_elements;

	return( 1 );
}

