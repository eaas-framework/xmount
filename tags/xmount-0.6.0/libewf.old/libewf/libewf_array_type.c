/* 
 * Array type functions
 *
 * Copyright (c) 2006-2012, Joachim Metz <jbmetz@users.sourceforge.net>
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

#include "libewf_array_type.h"

/* Creates an array
 * Make sure the value array is pointing to is set to NULL
 * Returns 1 if successful or -1 on error
 */
int libewf_array_initialize(
     libewf_array_t **array,
     int number_of_entries,
     liberror_error_t **error )
{
	static char *function = "libewf_array_initialize";
	size_t entries_size   = 0;

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( *array != NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid array value already set.",
		 function );

		return( -1 );
	}
	if( number_of_entries < 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid number of entries.",
		 function );

		return( -1 );
	}
	*array = memory_allocate_structure(
	          libewf_array_t );

	if( *array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create array.",
		 function );

		goto on_error;
	}
	if( memory_set(
	     *array,
	     0,
	     sizeof( libewf_array_t ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear array.",
		 function );

		goto on_error;
	}
	if( number_of_entries > 0 )
	{
		entries_size = sizeof( intptr_t * ) * number_of_entries;

		if( entries_size > (size_t) SSIZE_MAX )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM,
			 "%s: invalid entries size value exceeds maximum.",
			 function );

			goto on_error;
		}
		( *array )->entries = (intptr_t **) memory_allocate(
		                                     entries_size );

		if( ( *array )->entries == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create array entries.",
			 function );

			goto on_error;
		}
		if( memory_set(
		     ( *array )->entries,
		     0,
		     entries_size ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_SET_FAILED,
			 "%s: unable to clear array entries.",
			 function );

			goto on_error;
		}
		( *array )->number_of_allocated_entries = number_of_entries;
		( *array )->number_of_entries           = number_of_entries;
	}
	return( 1 );

on_error:
	if( *array != NULL )
	{
		if( ( *array )->entries != NULL )
		{
			memory_free(
			 ( *array )->entries );
		}
		memory_free(
		 *array );

		*array = NULL;
	}
	return( -1 );
}

/* Frees an array
 * The entries are freed using the entry_free_function
 * Returns 1 if successful or -1 on error
 */
int libewf_array_free(
     libewf_array_t **array,
     int (*entry_free_function)(
            intptr_t **entry,
            liberror_error_t **error ),
     liberror_error_t **error )
{
	static char *function = "libewf_array_free";
	int result            = 1;

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( *array != NULL )
	{
		if( ( *array )->entries != NULL )
		{
			if( libewf_array_clear(
			     *array,
			     entry_free_function,
			     error ) != 1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
				 "%s: unable to clear array.",
				 function );

				result = -1;
			}
			memory_free(
			 ( *array )->entries );
		}
		memory_free(
		 *array );

		*array = NULL;
	}
	return( result );
}

/* Empties an array and frees its entries
 * The entries are freed using the entry_free_function
 * Returns 1 if successful or -1 on error
 */
int libewf_array_empty(
     libewf_array_t *array,
     int (*entry_free_function)(
            intptr_t **entry,
            liberror_error_t **error ),
     liberror_error_t **error )
{
	static char *function = "libewf_array_empty";

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( libewf_array_clear(
	     array,
	     entry_free_function,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
		 "%s: unable to clear array.",
		 function );

		return( -1 );
	}
	array->number_of_entries = 0;

	return( 1 );
}

/* Clears an array and frees its entries
 * The entries are freed using the entry_free_function
 * Returns 1 if successful or -1 on error
 */
int libewf_array_clear(
     libewf_array_t *array,
     int (*entry_free_function)(
            intptr_t **entry,
            liberror_error_t **error ),
     liberror_error_t **error )
{
	static char *function = "libewf_array_clear";
	int entry_iterator    = 0;
	int result            = 1;

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( array->entries != NULL )
	{
		for( entry_iterator = 0;
		     entry_iterator < array->number_of_entries;
		     entry_iterator++ )
		{
			if( array->entries[ entry_iterator ] != NULL )
			{
				if( entry_free_function != NULL )
				{
					if( entry_free_function(
					     &( array->entries[ entry_iterator ] ),
					     error ) != 1 )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_RUNTIME,
						 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
						 "%s: unable to free array entry: %d.",
						 function,
						 entry_iterator );

						result = -1;
					}
				}
				array->entries[ entry_iterator ] = NULL;
			}
		}
	}
	return( result );
}

/* Clones the array and its entries
 *
 * The entries are cloned using the entry_clone_function
 * On error the entries are freed using the entry_free_function
 *
 * Returns 1 if successful or -1 on error
 */
int libewf_array_clone(
     libewf_array_t **destination_array,
     libewf_array_t *source_array,
     int (*entry_free_function)(
            intptr_t **entry,
            liberror_error_t **error ),
     int (*entry_clone_function)(
            intptr_t **destination,
            intptr_t *source,
            liberror_error_t **error ),
     liberror_error_t **error )
{
	static char *function = "libewf_array_clone";
	int entry_iterator    = 0;

	if( destination_array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid destination array.",
		 function );

		return( -1 );
	}
	if( *destination_array != NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: invalid destination array already set.",
		 function );

		return( -1 );
	}
	if( entry_free_function == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid entry free function.",
		 function );

		return( -1 );
	}
	if( entry_clone_function == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid entry clone function.",
		 function );

		return( -1 );
	}
	if( source_array == NULL )
	{
		*destination_array = NULL;

		return( 1 );
	}
	if( libewf_array_initialize(
	     destination_array,
	     source_array->number_of_entries,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create destination array.",
		 function );

		goto on_error;
	}
	if( *destination_array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: missing destination array.",
		 function );

		goto on_error;
	}
	if( source_array->entries != NULL )
	{
		for( entry_iterator = 0;
		     entry_iterator < source_array->number_of_entries;
		     entry_iterator++ )
		{
			if( source_array->entries[ entry_iterator ] != NULL )
			{
				if( entry_clone_function(
				     &( ( *destination_array )->entries[ entry_iterator ] ),
				     source_array->entries[ entry_iterator ],
				     error ) != 1 )
				{
					liberror_error_set(
					 error,
					 LIBERROR_ERROR_DOMAIN_RUNTIME,
					 LIBERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
					 "%s: unable to clone array entry: %d.",
					 function,
					 entry_iterator );

					goto on_error;
				}
			}
		}
	}
	return( 1 );

on_error:
	if( *destination_array != NULL )
	{
		libewf_array_free(
		 destination_array,
		 entry_free_function,
		 NULL );
	}
	return( -1 );
}

/* Resizes an array
 * Returns 1 if successful or -1 on error
 */
int libewf_array_resize(
     libewf_array_t *array,
     int number_of_entries,
     int (*entry_free_function)(
            intptr_t **entry,
            liberror_error_t **error ),
     liberror_error_t **error )
{
	void *reallocation    = NULL;
	static char *function = "libewf_array_resize";
	size_t entries_size   = 0;
	int entry_iterator    = 0;
	int result            = 1;

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( number_of_entries < 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid number of entries.",
		 function );

		return( -1 );
	}
	if( number_of_entries > array->number_of_allocated_entries )
	{
		entries_size = sizeof( intptr_t * ) * number_of_entries;

		if( entries_size > (size_t) SSIZE_MAX )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_EXCEEDS_MAXIMUM,
			 "%s: invalid entries size value exceeds maximum.",
			 function );

			return( -1 );
		}
		reallocation = memory_reallocate(
		                array->entries,
		                entries_size );

		if( reallocation == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to resize array entries.",
			 function );

			return( -1 );
		}
		array->entries = (intptr_t **) reallocation;

		if( memory_set(
		     &( array->entries[ array->number_of_allocated_entries ] ),
		     0,
		     sizeof( intptr_t * ) * ( number_of_entries - array->number_of_allocated_entries ) ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_SET_FAILED,
			 "%s: unable to clear array entries.",
			 function );

			result = -1;
		}
		array->number_of_allocated_entries = number_of_entries;
		array->number_of_entries           = number_of_entries;
	}
	else if( number_of_entries > array->number_of_entries )
	{
		array->number_of_entries = number_of_entries;
	}
	else if( array->entries != NULL )
	{
		if( entry_free_function == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
			 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
			 "%s: invalid entry free function.",
			 function );

			return( -1 );
		}
		for( entry_iterator = number_of_entries;
		     entry_iterator < array->number_of_entries;
		     entry_iterator++ )
		{
			if( array->entries[ entry_iterator ] != NULL )
			{
				if( entry_free_function != NULL )
				{
					if( entry_free_function(
					     &( array->entries[ entry_iterator ] ),
					     error ) != 1 )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_RUNTIME,
						 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
						 "%s: unable to free array entry: %d.",
						 function,
						 entry_iterator );

						result = -1;
					}
				}
				array->entries[ entry_iterator ] = NULL;
			}
		}
		array->number_of_entries = number_of_entries;
	}
	return( result );
}

/* Retrieves the number of entries in the array
 * Returns 1 if successful or -1 on error
 */
int libewf_array_get_number_of_entries(
     libewf_array_t *array,
     int *number_of_entries,
     liberror_error_t **error )
{
	static char *function = "libewf_array_get_number_of_entries";

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( number_of_entries == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid number of entries.",
		 function );

		return( -1 );
	}
	*number_of_entries = array->number_of_entries;

	return( 1 );
}

/* Retrieves a specific entry from the array
 * Returns 1 if successful or -1 on error
 */
int libewf_array_get_entry_by_index(
     libewf_array_t *array,
     int entry_index,
     intptr_t **entry,
     liberror_error_t **error )
{
	static char *function = "libewf_array_get_entry_by_index";

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( array->entries == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid array - missing entries.",
		 function );

		return( -1 );
	}
	if( ( entry_index < 0 )
	 || ( entry_index >= array->number_of_entries ) )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid entry index value out of bounds.",
		 function );

		return( -1 );
	}
	if( entry == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid entry.",
		 function );

		return( -1 );
	}
	*entry = array->entries[ entry_index ];

	return( 1 );
}

/* Sets a specific entry in the array
 * Returns 1 if successful or -1 on error
 */
int libewf_array_set_entry_by_index(
     libewf_array_t *array,
     int entry_index,
     intptr_t *entry,
     liberror_error_t **error )
{
	static char *function = "libewf_array_set_entry_by_index";

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( array->entries == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid array - missing entries.",
		 function );

		return( -1 );
	}
	if( ( entry_index < 0 )
	 || ( entry_index >= array->number_of_entries ) )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid entry index value out of bounds.",
		 function );

		return( -1 );
	}
	array->entries[ entry_index ] = entry;

	return( 1 );
}

/* Appends an entry
 * Sets the entry index to the newly appended entry
 * Returns 1 if successful or -1 on error
 */
int libewf_array_append_entry(
     libewf_array_t *array,
     int *entry_index,
     intptr_t *entry,
     liberror_error_t **error )
{
	static char *function = "libewf_array_append_entry";

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( entry_index == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid entry index.",
		 function );

		return( -1 );
	}
	*entry_index = array->number_of_entries;

	if( libewf_array_resize(
	     array,
	     array->number_of_entries + 1,
	     NULL,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_RESIZE_FAILED,
		 "%s: unable to resize array.",
		 function );

		return( -1 );
	}
	if( array->entries == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: invalid array - missing entries.",
		 function );

		return( -1 );
	}
	array->entries[ *entry_index ] = entry;

	return( 1 );
}

/* Inserts an entry in the array
 *
 * Uses the entry_compare_function to determine the order of the entries
 * The entry_compare_function should return LIBEWF_ARRAY_COMPARE_LESS,
 * LIBEWF_ARRAY_COMPARE_EQUAL, LIBEWF_ARRAY_COMPARE_GREATER if successful or -1 on error
 *
 * Duplicate entries are allowed by default and inserted after the last duplicate entry.
 * Only allowing unique entries can be enforced by setting the flag LIBEWF_ARRAY_INSERT_FLAG_UNIQUE_ENTRIES
 *
 * Returns 1 if successful, 0 if the node already exists or -1 on error
 */
int libewf_array_insert_entry(
     libewf_array_t *array,
     int *entry_index,
     intptr_t *entry,
     int (*entry_compare_function)(
            intptr_t *first_entry,
            intptr_t *second_entry,
            liberror_error_t **error ),
     uint8_t insert_flags,
     liberror_error_t **error )
{
	static char *function = "libewf_tree_node_insert_node";
	int entry_iterator    = 0;
	int result            = -1;

	if( array == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid array.",
		 function );

		return( -1 );
	}
	if( entry_index == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid entry index.",
		 function );

		return( -1 );
	}
	if( entry_compare_function == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid entry compare function.",
		 function );

		return( -1 );
	}
	if( ( insert_flags & ~( LIBEWF_ARRAY_INSERT_FLAG_UNIQUE_ENTRIES ) ) != 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported insert flags: 0x%02" PRIx8 ".",
		 function,
		 insert_flags );

		return( -1 );
	}
	if( array->entries != NULL )
	{
		for( entry_iterator = 0;
		     entry_iterator < array->number_of_entries;
		     entry_iterator++ )
		{
			result = entry_compare_function(
			          entry,
			          array->entries[ entry_iterator ],
			          error );

			if( result == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable to compare entry: %d.",
				 function,
				 entry_iterator );

				return( -1 );
			}
			else if( result == LIBEWF_ARRAY_COMPARE_EQUAL )
			{
				if( ( insert_flags & LIBEWF_ARRAY_INSERT_FLAG_UNIQUE_ENTRIES ) != 0 )
				{
					return( 0 );
				}
			}
			else if( result == LIBEWF_ARRAY_COMPARE_LESS )
			{
				break;
			}
			else if( result != LIBEWF_ARRAY_COMPARE_GREATER )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
				 LIBERROR_ARGUMENT_ERROR_UNSUPPORTED_VALUE,
				 "%s: unsupported entry compare function return value: %d.",
				 function,
				 result );

				return( -1 );
			}
		}
	}
	if( ( array->entries != NULL )
	 && ( result == LIBEWF_ARRAY_COMPARE_LESS ) )
	{
		*entry_index = entry_iterator;

		if( libewf_array_resize(
		     array,
		     array->number_of_entries + 1,
		     NULL,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_RESIZE_FAILED,
			 "%s: unable to resize array.",
			 function );

			return( -1 );
		}
		if( array->entries == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: invalid array - missing entries.",
			 function );

			return( -1 );
		}
		for( entry_iterator = array->number_of_entries - 1;
		     entry_iterator > *entry_index;
		     entry_iterator-- )
		{
			array->entries[ entry_iterator ] = array->entries[ entry_iterator - 1 ];
		}
		array->entries[ *entry_index ] = entry;
	}
	else
	{
		*entry_index = array->number_of_entries;

		if( libewf_array_resize(
		     array,
		     array->number_of_entries + 1,
		     NULL,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_RESIZE_FAILED,
			 "%s: unable to resize array.",
			 function );

			return( -1 );
		}
		if( array->entries == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: invalid array - missing entries.",
			 function );

			return( -1 );
		}
		array->entries[ *entry_index ] = entry;
	}
	return( 1 );
}

