/*
 * Track value functions
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

#if !defined( _LIBODRAW_TRACK_VALUE_H )
#define _LIBODRAW_TRACK_VALUE_H

#include <common.h>
#include <types.h>

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct libodraw_track_value libodraw_track_value_t;

struct libodraw_track_value
{
	/* The start sector
	 */
	uint64_t start_sector;

	/* The end sector
	 */
	uint64_t end_sector;

	/* The number of sectors
	 */
	uint64_t number_of_sectors;

	/* The bytes per sector
	 */
	uint32_t bytes_per_sector;

	/* The type
	 */
	uint8_t type;

	/* The data file index
	 */
	int data_file_index;

	/* The start sector relative to the start of the data file
	 */
	uint64_t data_file_start_sector;

	/* The data file offset
	 */
	off64_t data_file_offset;
};

int libodraw_track_value_initialize(
     libodraw_track_value_t **track_value,
     liberror_error_t **error );

int libodraw_track_value_free(
     intptr_t *track_value,
     liberror_error_t **error );

int libodraw_track_value_get(
     libodraw_track_value_t *track_value,
     uint64_t *start_sector,
     uint64_t *number_of_sectors,
     uint8_t *type,
     int *data_file_index,
     uint64_t *data_file_start_sector,
     liberror_error_t **error );

int libodraw_track_value_set(
     libodraw_track_value_t *track_value,
     uint64_t start_sector,
     uint64_t number_of_sectors,
     uint8_t type,
     int data_file_index,
     uint64_t data_file_start_sector,
     liberror_error_t **error );

int libodraw_track_value_get_bytes_per_sector(
     libodraw_track_value_t *track_value,
     uint32_t *bytes_per_sector,
     liberror_error_t **error );

#if defined( __cplusplus )
}
#endif

#endif

