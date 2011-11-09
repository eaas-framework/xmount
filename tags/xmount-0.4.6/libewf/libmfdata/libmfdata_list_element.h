/*
 * The list element functions
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

#if !defined( _LIBMFDATA_INTERNAL_LIST_ELEMENT_H )
#define _LIBMFDATA_INTERNAL_LIST_ELEMENT_H

#include <common.h>
#include <types.h>

#include <liberror.h>

#include "libmfdata_date_time.h"
#include "libmfdata_extern.h"
#include "libmfdata_group.h"
#include "libmfdata_libbfio.h"
#include "libmfdata_range.h"
#include "libmfdata_types.h"

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct libmfdata_internal_list_element libmfdata_internal_list_element_t;

struct libmfdata_internal_list_element
{
	/* The list reference
	 */
	libmfdata_list_t *list;

	/* The (list) element index
	 */
	int element_index;

	/* The (element) data range
	 */
	libmfdata_range_t *data_range;

	/* The (element) backup data range
	 */
	libmfdata_range_t *backup_data_range;

	/* The time stamp
	 */
	time_t timestamp;

	/* The (element) value offset
	 */
	off64_t value_offset;

	/* The (element) value size
	 */
	size64_t value_size;

	/* The (element) group values
	 */
	libmfdata_group_t *group_values;

	/* The flags
	 */
	uint8_t flags;
};

int libmfdata_list_element_initialize(
     libmfdata_list_element_t **element,
     libmfdata_list_t *list,
     int element_index,
     liberror_error_t **error );

int libmfdata_list_element_free(
     intptr_t *element,
     liberror_error_t **error );

int libmfdata_list_element_clone(
     libmfdata_list_element_t **destination_element,
     libmfdata_list_element_t *source_element,
     libmfdata_list_t *list,
     int element_index,
     liberror_error_t **error );

int libmfdata_list_element_get_element_index(
     libmfdata_list_element_t *element,
     int *element_index,
     liberror_error_t **error );

int libmfdata_list_element_set_element_index(
     libmfdata_list_element_t *element,
     int element_index,
     liberror_error_t **error );

int libmfdata_list_element_get_timestamp(
     libmfdata_list_element_t *element,
     time_t *timestamp,
     liberror_error_t **error );

int libmfdata_list_element_get_value_offset(
     libmfdata_list_element_t *element,
     off64_t *value_offset,
     liberror_error_t **error );

int libmfdata_list_element_set_value_offset(
     libmfdata_list_element_t *element,
     off64_t value_offset,
     liberror_error_t **error );

int libmfdata_list_element_get_value_size(
     libmfdata_list_element_t *element,
     size64_t *value_size,
     liberror_error_t **error );

int libmfdata_list_element_set_value_size(
     libmfdata_list_element_t *element,
     size64_t value_size,
     liberror_error_t **error );

/* Group functions
 */
int libmfdata_list_element_is_group(
     libmfdata_list_element_t *element,
     liberror_error_t **error );

int libmfdata_list_element_get_group_values(
     libmfdata_list_element_t *element,
     int *number_of_elements,
     liberror_error_t **error );

int libmfdata_list_element_set_group_values(
     libmfdata_list_element_t *element,
     int number_of_elements,
     liberror_error_t **error );

/* Data range functions
 */
int libmfdata_list_element_get_data_range(
     libmfdata_list_element_t *element,
     int *file_io_pool_entry,
     off64_t *offset,
     size64_t *size,
     uint32_t *flags,
     liberror_error_t **error );

int libmfdata_list_element_set_data_range(
     libmfdata_list_element_t *element,
     int file_io_pool_entry,
     off64_t offset,
     size64_t size,
     uint32_t flags,
     liberror_error_t **error );

/* Backup range functions
 */
int libmfdata_list_element_get_backup_data_range(
     libmfdata_list_element_t *element,
     int *file_io_pool_entry,
     off64_t *offset,
     size64_t *size,
     uint32_t *flags,
     liberror_error_t **error );

int libmfdata_list_element_set_backup_data_range(
     libmfdata_list_element_t *element,
     int file_io_pool_entry,
     off64_t offset,
     size64_t size,
     uint32_t flags,
     liberror_error_t **error );

/* Element value functions
 */
LIBMFDATA_EXTERN \
int libmfdata_list_element_get_element_value(
     libmfdata_list_element_t *element,
     libbfio_pool_t *file_io_pool,
     libmfdata_cache_t *cache,
     intptr_t **element_value,
     uint8_t read_flags,
     liberror_error_t **error );

LIBMFDATA_EXTERN \
int libmfdata_list_element_set_element_value(
     libmfdata_list_element_t *element,
     libmfdata_cache_t *cache,
     intptr_t *element_value,
     int (*free_element_value)(
            intptr_t *element_value,
            liberror_error_t **error ),
     uint8_t flags,
     liberror_error_t **error );

#if defined( __cplusplus )
}
#endif

#endif

