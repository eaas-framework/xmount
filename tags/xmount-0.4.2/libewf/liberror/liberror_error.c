/*
 * Error functions
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

#include <common.h>
#include <memory.h>

#if defined( HAVE_STDARG_H )
#include <stdarg.h>
#elif defined( HAVE_VARARGS_H )
#include <varargs.h>
#else
#error Missing headers stdarg.h and varargs.h
#endif

#include "liberror_definitions.h"
#include "liberror_error.h"
#include "liberror_types.h"

#if defined( HAVE_STDARG_H )
#define VARARGS( function, error, error_domain, error_code, type, argument ) \
        function( error, error_domain, error_code, type argument, ... )
#define VASTART( argument_list, type, name ) \
        va_start( argument_list, name )
#define VAEND( argument_list ) \
        va_end( argument_list )

#elif defined( HAVE_VARARGS_H )
#define VARARGS( function, error, error_domain, error_code, type, argument ) \
        function( error, error_domain, error_code, va_alist ) va_dcl
#define VASTART( argument_list, type, name ) \
        { type name; va_start( argument_list ); name = va_arg( argument_list, type )
#define VAEND( argument_list ) \
        va_end( argument_list ); }

#endif

/* Set an error initializes the error
 * The error domain and code are set only the first time and the error message is appended for backtracing
 */
void VARARGS(
      liberror_error_set,
      liberror_error_t **error,
      int error_domain,
      int error_code,
      const char *,
      format )
{
	va_list argument_list;

	void *reallocation  = NULL;
	size_t message_size = 64;
	int print_count     = 0;

	if( error == NULL )
	{
		return;
	}
	if( *error == NULL )
	{
		*error = (liberror_error_t *) memory_allocate(
		                             sizeof( liberror_internal_error_t ) );

		if( *error == NULL )
		{
			return;
		}
		( (liberror_internal_error_t *) *error )->amount_of_messages = 0;
		( (liberror_internal_error_t *) *error )->message            = NULL;
		( (liberror_internal_error_t *) *error )->domain             = error_domain;
		( (liberror_internal_error_t *) *error )->code               = error_code;
	}
	reallocation = memory_reallocate(
	                ( (liberror_internal_error_t *) *error )->message,
	                sizeof( char * ) * ( ( (liberror_internal_error_t *) *error )->amount_of_messages + 1 ) );

	if( reallocation == NULL )
	{
		return;
	}
	( (liberror_internal_error_t *) *error )->amount_of_messages                                                       += 1;
	( (liberror_internal_error_t *) *error )->message                                                                   = (char **) reallocation;
	( (liberror_internal_error_t *) *error )->message[ ( (liberror_internal_error_t *) *error )->amount_of_messages - 1 ] = NULL;

	do
	{
		reallocation = memory_reallocate(
		                ( (liberror_internal_error_t *) *error )->message[ ( (liberror_internal_error_t *) *error )->amount_of_messages - 1 ],
		                sizeof( char ) * message_size );

		if( reallocation == NULL )
		{
			memory_free(
			 ( (liberror_internal_error_t *) *error )->message[ ( (liberror_internal_error_t *) *error )->amount_of_messages - 1 ] );

			( (liberror_internal_error_t *) *error )->message[ ( (liberror_internal_error_t *) *error )->amount_of_messages - 1 ] = NULL;

			return;
		}
		( (liberror_internal_error_t *) *error )->message[ ( (liberror_internal_error_t *) *error )->amount_of_messages - 1 ] = reallocation;

		VASTART(
		 argument_list,
		 const char *,
		 format );

		print_count = liberror_error_vsnprintf(
		               ( (liberror_internal_error_t *) *error )->message[ ( (liberror_internal_error_t *) *error )->amount_of_messages - 1 ],
		               message_size,
		               format,
		               argument_list );

		VAEND(
		 argument_list );

		if( print_count <= -1 )
		{
			message_size += 64;
		}
		else if( ( (size_t) print_count > message_size )
		      || ( ( (liberror_internal_error_t *) *error )->message[ ( (liberror_internal_error_t *) *error )->amount_of_messages - 1 ][ print_count ] != 0 ) )
		{
			message_size = (size_t) ( print_count + 1 );
			print_count  = -1;
		}
	}
	while( print_count <= -1 );
}

#undef VARARGS
#undef VASTART
#undef VAEND

/* Free an error and its elements
 */
void liberror_error_free(
      liberror_error_t **error )
{
	int message_iterator = 0;

	if( error == NULL )
	{
		return;
	}
	if( *error != NULL )
	{
		if( ( (liberror_internal_error_t *) *error )->message != NULL )
		{
			for( message_iterator = 0; message_iterator < ( (liberror_internal_error_t *) *error )->amount_of_messages; message_iterator++ )
			{
				if( ( (liberror_internal_error_t *) *error )->message[ message_iterator ] != NULL )
				{
					memory_free(
					 ( (liberror_internal_error_t *) *error )->message[ message_iterator ] );
				}
			}
			memory_free(
			 ( (liberror_internal_error_t *) *error )->message );
		}
		memory_free(
		 *error );

		*error = NULL;
	}
}

/* Determines if an error equals a certain error code of a domain
 * Returns 1 if error matches or 0 if not
 */
int liberror_error_matches(
     liberror_error_t *error,
     int error_domain,
     int error_code )
{
	if( error == NULL )
	{
		return( 0 );
	}
	if( ( ( (liberror_internal_error_t *) error )->domain == error_domain )
	 && ( ( (liberror_internal_error_t *) error )->code == error_code ) )
	{
		return( 1 );
	}
	return( 0 );
}

/* Prints a descriptive string of the error to the stream
 */
void liberror_error_fprint(
     liberror_error_t *error,
     FILE *stream )
{
	int message_iterator = 0;

	if( error == NULL )
	{
		return;
	}
	if( stream == NULL )
	{
		return;
	}
	if( ( (liberror_internal_error_t *) error )->message == NULL )
	{
		return;
	}
	message_iterator = ( (liberror_internal_error_t *) error )->amount_of_messages - 1;

	if( ( (liberror_internal_error_t *) error )->message[ message_iterator ] != NULL )
	{
		fprintf(
		 stream,
		 "%s\n",
		 ( (liberror_internal_error_t *) error )->message[ message_iterator ] );
	}
}

/* Prints a backtrace of the error to the stream
 */
void liberror_error_backtrace_fprint(
     liberror_error_t *error,
     FILE *stream )
{
	int message_iterator = 0;

	if( error == NULL )
	{
		return;
	}
	if( stream == NULL )
	{
		return;
	}
	if( ( (liberror_internal_error_t *) error )->message == NULL )
	{
		return;
	}
	for( message_iterator = 0; message_iterator < ( (liberror_internal_error_t *) error )->amount_of_messages; message_iterator++ )
	{
		if( ( (liberror_internal_error_t *) error )->message[ message_iterator ] != NULL )
		{
			fprintf(
			 stream,
			 "%s\n",
			 ( (liberror_internal_error_t *) error )->message[ message_iterator ] );
		}
	}
}

