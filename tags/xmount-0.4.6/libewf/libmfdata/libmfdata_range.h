/*
 * The data range functions
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

#if !defined( _LIBMFDATA_RANGE_H )
#define _LIBMFDATA_RANGE_H

#include <common.h>
#include <types.h>

#include <liberror.h>

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct libmfdata_range libmfdata_range_t;

struct libmfdata_range
{
	/* The file IO pool entry
	 */
	int file_io_pool_entry;

	/* The offset
	 */
	off64_t offset;

	/* The size
	 */
	size64_t size;

	/* The flags
	 */
	uint32_t flags;
};

int libmfdata_range_initialize(
     libmfdata_range_t **range,
     liberror_error_t **error );

int libmfdata_range_free(
     intptr_t *range,
     liberror_error_t **error );

int libmfdata_range_clone(
     libmfdata_range_t **destination_range,
     libmfdata_range_t *source_range,
     liberror_error_t **error );

int libmfdata_range_get_values(
     libmfdata_range_t *range,
     int *file_io_pool_entry,
     off64_t *offset,
     size64_t *size,
     uint32_t *flags,
     liberror_error_t **error );

int libmfdata_range_set_values(
     libmfdata_range_t *range,
     int file_io_pool_entry,
     off64_t offset,
     size64_t size,
     uint32_t flags,
     liberror_error_t **error );

#if defined( __cplusplus )
}
#endif

#endif

