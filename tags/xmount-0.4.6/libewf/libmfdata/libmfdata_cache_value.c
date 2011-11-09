/*
 * Cache value functions
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

#include "libmfdata_cache_value.h"
#include "libmfdata_date_time.h"
#include "libmfdata_definitions.h"

/* Creates a cache value
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_value_initialize(
     libmfdata_cache_value_t **cache_value,
     liberror_error_t **error )
{
	static char *function = "libmfdata_cache_value_initialize";

	if( cache_value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache value.",
		 function );

		return( -1 );
	}
	if( *cache_value == NULL )
	{
		*cache_value = memory_allocate_structure(
		                libmfdata_cache_value_t );

		if( *cache_value == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create cache value.",
			 function );

			goto on_error;
		}
		if( memory_set(
		     *cache_value,
		     0,
		     sizeof( libmfdata_cache_value_t ) ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_SET_FAILED,
			 "%s: unable to clear cache value.",
			 function );

			goto on_error;
		}
		( *cache_value )->file_io_pool_entry = -1;
		( *cache_value )->offset             = (off64_t) -1;
	}
	return( 1 );

on_error:
	if( *cache_value != NULL )
	{
		memory_free(
		 *cache_value );

		*cache_value = NULL;
	}
	return( -1 );
}

/* Frees the cache value
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_value_free(
     intptr_t *cache_value,
     liberror_error_t **error )
{
	static char *function = "libmfdata_cache_value_free";
	int result            = 1;

	if( cache_value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache value.",
		 function );

		return( -1 );
	}
	if( ( ( (libmfdata_cache_value_t *) cache_value )->flags & LIBMFDATA_CACHE_VALUE_FLAG_MANAGED ) != 0 )
	{
		if( ( (libmfdata_cache_value_t *) cache_value )->value != NULL )
		{
			if( ( (libmfdata_cache_value_t *) cache_value )->free_value == NULL )
			{
				memory_free(
				 ( (libmfdata_cache_value_t *) cache_value )->value );
			}
			else if( ( (libmfdata_cache_value_t *) cache_value )->free_value(
				  ( (libmfdata_cache_value_t *) cache_value )->value,
				  error ) != 1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
				 "%s: unable to free value.",
				 function );

				result = -1;
			}
		}
	}
	memory_free(
	 cache_value );

	return( result );
}

/* Retrieves the cache value identifier
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_value_get_identifier(
     libmfdata_cache_value_t *cache_value,
     int *file_io_pool_entry,
     off64_t *offset,
     time_t *timestamp,
     liberror_error_t **error )
{
	static char *function = "libmfdata_cache_value_get_identifier";

	if( cache_value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache value.",
		 function );

		return( -1 );
	}
	if( file_io_pool_entry == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file IO pool entry.",
		 function );

		return( -1 );
	}
	if( offset == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid offset.",
		 function );

		return( -1 );
	}
	if( timestamp == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid timestamp.",
		 function );

		return( -1 );
	}
	*file_io_pool_entry = cache_value->file_io_pool_entry;
	*offset             = cache_value->offset;
	*timestamp          = cache_value->timestamp;

	return( 1 );
}

/* Sets the cache value identifier
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_value_set_identifier(
     libmfdata_cache_value_t *cache_value,
     int file_io_pool_entry,
     off64_t offset,
     time_t timestamp,
     liberror_error_t **error )
{
	static char *function = "libmfdata_cache_value_set_identifier";

	if( cache_value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache value.",
		 function );

		return( -1 );
	}
	cache_value->file_io_pool_entry = file_io_pool_entry;
	cache_value->offset             = offset;
	cache_value->timestamp          = timestamp;

	return( 1 );
}

/* Retrieves the cache value
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_value_get_value(
     libmfdata_cache_value_t *cache_value,
     intptr_t **value,
     liberror_error_t **error )
{
	static char *function = "libmfdata_cache_value_get_value";

	if( cache_value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache value.",
		 function );

		return( -1 );
	}
	if( value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid value.",
		 function );

		return( -1 );
	}
	*value = cache_value->value;

	return( 1 );
}

/* Sets the cache value
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_value_set_value(
     libmfdata_cache_value_t *cache_value,
     intptr_t *value,
     int (*free_value)(
            intptr_t *value,
            liberror_error_t **error ),
     uint8_t flags,
     liberror_error_t **error )
{
	static char *function = "libmfdata_cache_value_set_value";

	if( cache_value == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache value.",
		 function );

		return( -1 );
	}
	if( ( cache_value->flags & LIBMFDATA_CACHE_VALUE_FLAG_MANAGED ) != 0 )
	{
		if( cache_value->value != NULL )
		{
			if( cache_value->free_value == NULL )
			{
				memory_free(
				 cache_value->value );
			}
			else if( cache_value->free_value(
				  cache_value->value,
				  error ) != 1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
				 "%s: unable to free value.",
				 function );

				return( -1 );
			}
		}
		cache_value->flags &= ~( LIBMFDATA_CACHE_VALUE_FLAG_MANAGED );
	}
	cache_value->value      = value;
	cache_value->free_value = free_value;
	cache_value->flags     |= flags;

	return( 1 );
}

