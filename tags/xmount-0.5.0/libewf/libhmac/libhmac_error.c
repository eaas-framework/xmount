/*
 * Error functions
 *
 * Copyright (c) 2011-2012, Joachim Metz <jbmetz@users.sourceforge.net>
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

#include <stdio.h>

#include "libhmac_error.h"

#if !defined( HAVE_LOCAL_LIBHMAC )

/* Free an error and its elements
 */
void libhmac_error_free(
      libhmac_error_t **error )
{
	liberror_error_free(
	 (liberror_error_t **) error );
}

/* Prints a descriptive string of the error to the stream
 * Returns the number of printed characters if successful or -1 on error
 */
int libhmac_error_fprint(
     libhmac_error_t *error,
     FILE *stream )
{
	return( liberror_error_fprint(
	         (liberror_error_t *) error,
	         stream ) );
}

/* Prints a descriptive string of the error to the string
 * The end-of-string character is not included in the return value
 * Returns the number of printed characters if successful or -1 on error
 */
int libhmac_error_sprint(
     libhmac_error_t *error,
     char *string,
     size_t size )
{
	return( liberror_error_sprint(
	         (liberror_error_t *) error,
	         string,
	         size ) );
}

/* Prints a backtrace of the error to the stream
 * Returns the number of printed characters if successful or -1 on error
 */
int libhmac_error_backtrace_fprint(
     libhmac_error_t *error,
      FILE *stream )
{
	return( liberror_error_backtrace_fprint(
	         (liberror_error_t *) error,
	         stream ) );
}

/* Prints a backtrace of the error to the string
 * The end-of-string character is not included in the return value
 * Returns the number of printed characters if successful or -1 on error
 */
int libhmac_error_backtrace_sprint(
     libhmac_error_t *error,
     char *string,
     size_t size )
{
	return( liberror_error_backtrace_sprint(
	         (liberror_error_t *) error,
	         string,
	         size ) );
}

#endif

