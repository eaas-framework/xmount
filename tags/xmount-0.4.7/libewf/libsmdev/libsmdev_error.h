/*
 * Error functions
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

#if !defined( _LIBSMDEV_INTERNAL_ERROR_H )
#define _LIBSMDEV_INTERNAL_ERROR_H

#include <common.h>
#include <types.h>

#include <stdio.h>

#if !defined( HAVE_LOCAL_LIBSMDEV )
#include <libsmdev/error.h>
#endif

#include "libsmdev_extern.h"

#if defined( __cplusplus )
extern "C" {
#endif

#if !defined( HAVE_LOCAL_LIBSMDEV )

LIBSMDEV_EXTERN \
void libsmdev_error_free(
      libsmdev_error_t **error );

LIBSMDEV_EXTERN \
int libsmdev_error_fprint(
     libsmdev_error_t *error,
     FILE *stream );

LIBSMDEV_EXTERN \
int libsmdev_error_sprint(
     libsmdev_error_t *error,
     char *string,
     size_t size );

LIBSMDEV_EXTERN \
int libsmdev_error_backtrace_fprint(
     libsmdev_error_t *error,
     FILE *stream );

LIBSMDEV_EXTERN \
int libsmdev_error_backtrace_sprint(
     libsmdev_error_t *error,
     char *string,
     size_t size );

#endif

#if defined( __cplusplus )
}
#endif

#endif

