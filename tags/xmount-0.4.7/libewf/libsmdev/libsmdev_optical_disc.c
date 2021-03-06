/*
 * Optical disk functions
 *
 * Copyright (c) 2010-2011, Joachim Metz <jbmetz@users.sourceforge.net>
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <byte_stream.h>
#include <memory.h>
#include <types.h>

#include <liberror.h>
#include <libnotify.h>

#if defined( HAVE_SYS_IOCTL_H )
#include <sys/ioctl.h>
#endif

#if defined( HAVE_LINUX_CDROM_H )
#include <linux/cdrom.h>
#endif

#include "libsmdev_definitions.h"
#include "libsmdev_handle.h"
#include "libsmdev_metadata.h"
#include "libsmdev_optical_disc.h"
#include "libsmdev_sector_range.h"
#include "libsmdev_scsi.h"
#include "libsmdev_track_value.h"

#if defined( HAVE_LINUX_CDROM_H )

#define libsmdev_optical_disc_copy_absolute_msf_to_lba( minutes, seconds, frames, lba ) \
	lba  = minutes; \
	lba *= CD_SECS; \
	lba += seconds; \
	lba *= CD_FRAMES; \
	lba += frames;

#define libsmdev_optical_disc_copy_msf_to_lba( minutes, seconds, frames, lba ) \
	lba  = minutes; \
	lba *= CD_SECS; \
	lba += seconds; \
	lba *= CD_FRAMES; \
	lba += frames; \
	lba -= CD_MSF_OFFSET;

/* Retrieves the table of contents (toc) from the optical disk
 * Returns 1 if successful or -1 on error
 */
int libsmdev_optical_disc_get_table_of_contents(
     int file_descriptor,
     libsmdev_internal_handle_t *internal_handle,
     liberror_error_t **error )
{
	static char *function = "libsmdev_optical_disc_get_table_of_contents";
	int result            = 0;

	result = libsmdev_optical_disc_get_table_of_contents_scsi(
	          file_descriptor,
	          internal_handle,
	          error );

	if( result == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable retrieve table of contents using SCSI commands.",
		 function );

		return( -1 );
	}
	else if( result == 0 )
	{
		if( libsmdev_optical_disc_get_table_of_contents_ioctl(
		     file_descriptor,
		     internal_handle,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_GET_FAILED,
			 "%s: unable retrieve table of contents using IO control.",
			 function );

			return( -1 );
		}
	}
	return( 1 );
}

/* Retrieves the table of contents from the optical disk using the SCSI READ TOC command
 * Returns 1 if successful, 0 if no TOC could be found or -1 on error
 */
int libsmdev_optical_disc_get_table_of_contents_scsi(
     int file_descriptor,
     libsmdev_internal_handle_t *internal_handle,
     liberror_error_t **error )
{
	uint8_t track_info_data[ 64 ];

	uint8_t *toc_data            = NULL;
	uint8_t *toc_entries         = NULL;
	static char *function        = "libsmdev_optical_disc_get_table_of_contents_scsi";
	void *reallocation           = NULL;
	size_t toc_data_offset       = 0;
	size_t toc_data_size         = 0;
	ssize_t response_count       = 0;
	uint32_t lead_out_size       = 0;
	uint32_t lead_out_offset     = 0;
	uint32_t last_track_offset   = 0;
	uint32_t track_offset        = 0;
	uint32_t session_size        = 0;
	uint32_t session_offset      = 0;
	uint32_t next_session_offset = 0;
	uint16_t entry_iterator      = 0;
	uint8_t first_track_number   = 0;
	uint8_t last_track_number    = 0;
	uint8_t lead_out_index       = 0;
	uint8_t number_of_sessions   = 0;
	uint8_t session_index        = 0;
	uint8_t track_index          = 0;
	uint8_t track_number         = 0;
	uint8_t track_type           = 0;
	int result                   = 0;

	if( file_descriptor == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file descriptor.",
		 function );

		return( -1 );
	}
	toc_data_size = 1024;

	toc_data = (uint8_t *) memory_allocate(
	                        sizeof( uint8_t ) * toc_data_size );

	if( toc_data == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create TOC data.",
		 function );

		goto on_error;
	}
	if( memory_set(
	     toc_data,
	     0,
	     sizeof( uint8_t ) * toc_data_size ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear TOC data.",
		 function );

		goto on_error;
	}
	response_count = libsmdev_scsi_read_toc(
	                  file_descriptor,
	                  LIBSMDEV_SCSI_TOC_CDB_FORMAT_RAW_TOC,
	                  toc_data,
	                  toc_data_size,
	                  error );

	if( response_count == -1 )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( ( error != NULL )
		 && ( *error != NULL ) )
		{
			libnotify_print_error_backtrace(
			 *error );
		}
#endif
		liberror_error_free(
		 error );

		byte_stream_copy_to_uint16_big_endian(
		 toc_data,
		 toc_data_size );

		if( toc_data_size > 1024 )
		{
			reallocation = memory_reallocate(
					toc_data,
					sizeof( uint8_t ) * toc_data_size );

			if( reallocation == NULL )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_MEMORY,
				 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
				 "%s: unable to resize TOC data.",
				 function );

				goto on_error;
			}
			toc_data = (uint8_t *) reallocation;

			response_count = libsmdev_scsi_read_toc(
					  file_descriptor,
					  LIBSMDEV_SCSI_TOC_CDB_FORMAT_RAW_TOC,
					  toc_data,
					  toc_data_size,
					  error );

			if( response_count == -1 )
			{
#if defined( HAVE_DEBUG_OUTPUT )
				if( ( error != NULL )
				 && ( *error != NULL ) )
				{
					libnotify_print_error_backtrace(
					 *error );
				}
#endif
				liberror_error_free(
				 error );
			}
		}
	}
	toc_data_size = response_count;

	if( toc_data_size > 4 )
	{
#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_printf(
			 "%s: header:\n",
			 function );
			libnotify_print_data(
			 toc_data,
			 4 );
		}
#endif
		number_of_sessions = (uint16_t) toc_data[ 3 ];

#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_printf(
			 "%s: number of sessions\t\t\t: %" PRIu8 "\n",
			 function,
			 number_of_sessions );

			libnotify_printf(
			 "\n" );
		}
#endif
		toc_entries     = &( toc_data[ 4 ] );
		toc_data_offset = 4;

		while( toc_data_offset < (size_t) toc_data_size )
		{
#if defined( HAVE_DEBUG_OUTPUT )
			if( libnotify_verbose != 0 )
			{
				libnotify_printf(
				 "%s: entry: %02" PRIu16 ":\n",
				 function,
				 entry_iterator );
				libnotify_print_data(
				 toc_entries,
				 11 );
			}
#endif
			if( toc_entries[ 3 ] <= 0x63 )
			{
				libsmdev_optical_disc_copy_msf_to_lba(
				 toc_entries[ 8 ],
				 toc_entries[ 9 ],
				 toc_entries[ 10 ],
				 track_offset );
			}
			else if( toc_entries[ 3 ] == 0xa0 )
			{
				first_track_number = toc_entries[ 8 ];
			}
			else if( toc_entries[ 3 ] == 0xa1 )
			{
				last_track_number = toc_entries[ 8 ];
			}
			else if( toc_entries[ 3 ] == 0xa2 )
			{
				libsmdev_optical_disc_copy_msf_to_lba(
				 toc_entries[ 8 ],
				 toc_entries[ 9 ],
				 toc_entries[ 10 ],
				 lead_out_offset );
			}
			else if( toc_entries[ 3 ] == 0xb0 )
			{
				libsmdev_optical_disc_copy_absolute_msf_to_lba(
				 toc_entries[ 4 ],
				 toc_entries[ 5 ],
				 toc_entries[ 6 ],
				 next_session_offset );
			}
#if defined( HAVE_DEBUG_OUTPUT )
			if( libnotify_verbose != 0 )
			{
				if( toc_entries[ 3 ] <= 0x63 )
				{
					libnotify_printf(
					 "%s: session: %02" PRIu16 " track: %02" PRIu8 "\t\t\t: %02" PRIu8 ":%02" PRIu8 ".%02" PRIu8 " (offset: %" PRIu32 ")\n",
					 function,
					 toc_entries[ 0 ],
					 toc_entries[ 3 ],
					 toc_entries[ 4 ],
					 toc_entries[ 5 ],
					 toc_entries[ 6 ],
					 track_offset );
				}
				else if( toc_entries[ 3 ] == 0xa0 )
				{
					libnotify_printf(
					 "%s: session: %02" PRIu8 " first track number\t: %" PRIu8 "\n",
					 function,
					 toc_entries[ 0 ],
					 first_track_number );
				}
				else if( toc_entries[ 3 ] == 0xa1 )
				{
					libnotify_printf(
					 "%s: session: %02" PRIu8 " last track number\t\t: %" PRIu8 "\n",
					 function,
					 toc_entries[ 0 ],
					 last_track_number );
				}
				else if( toc_entries[ 3 ] == 0xa2 )
				{
					libnotify_printf(
					 "%s: session: %02" PRIu8 " lead out\t\t\t: %02" PRIu8 ":%02" PRIu8 ".%02" PRIu8 " (offset: %" PRIu32 ")\n",
					 function,
					 toc_entries[ 0 ],
					 toc_entries[ 8 ],
					 toc_entries[ 9 ],
					 toc_entries[ 10 ],
					 lead_out_offset );
				}
				else if( toc_entries[ 3 ] == 0xb0 )
				{
					libnotify_printf(
					 "%s: session: %02" PRIu16 " end\t\t\t: %02" PRIu8 ":%02" PRIu8 ".%02" PRIu8 " (offset: %" PRIu32 ")\n",
					 function,
					 toc_entries[ 0 ],
					 toc_entries[ 4 ],
					 toc_entries[ 5 ],
					 toc_entries[ 6 ],
					 next_session_offset );
				}
				libnotify_printf(
				 "\n" );
			}
#endif
			if( ( toc_entries[ 3 ] <= 0x63 )
			 || ( toc_entries[ 3 ] == 0xb0 ) )
			{
				if( track_number >= first_track_number )
				{
			 		if( toc_entries[ 3 ] == 0xb0 )
					{
						track_offset = lead_out_offset;
					}
					if( track_offset < last_track_offset )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
						 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
						 "%s: invalid track offset value out of bounds.",
						 function );

						goto on_error;
					}
					if( ( track_index + 1 ) != track_number )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
						 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
						 "%s: invalid track number value out of bounds.",
						 function );

						goto on_error;
					}
			 		if( toc_entries[ 3 ] == 0xa2 )
					{
						if( track_number != last_track_number )
						{
							liberror_error_set(
							 error,
							 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
							 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
							 "%s: invalid track number value out of bounds.",
							 function );

							goto on_error;
						}
					}
					if( memory_set(
					     track_info_data,
					     0,
					     64 ) == NULL )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_MEMORY,
						 LIBERROR_MEMORY_ERROR_SET_FAILED,
						 "%s: unable to clear track info data.",
						 function );

						goto on_error;
					}
					response_count = libsmdev_scsi_read_track_information(
							  file_descriptor,
							  last_track_offset,
							  track_info_data,
							  64,
							  error );

					if( response_count == -1 )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_RUNTIME,
						 LIBERROR_RUNTIME_ERROR_GET_FAILED,
						 "%s: unable retrieve track info data: %d.",
						 function,
						 track_index );

						goto on_error;
					}
#if defined( HAVE_DEBUG_OUTPUT )
					if( libnotify_verbose != 0 )
					{
						libnotify_printf(
						 "%s: track information data: %d:\n",
						 function,
						 track_index );
						libnotify_print_data(
						 track_info_data,
						 response_count );
					}
#endif
					if( track_info_data[ 2 ] != toc_entries[ 0 ] )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
						 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
						 "%s: invalid track information data - session number value out of bounds.",
						 function );

						goto on_error;
					}
					if( track_info_data[ 3 ] != track_number )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
						 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
						 "%s: invalid track information data - track number value out of bounds.",
						 function );

						goto on_error;
					}
					track_type = LIBSMDEV_TRACK_TYPE_UNKNOWN;

					if( ( track_info_data[ 5 ] & 0x04 ) != 0 )
					{
						if( ( track_info_data[ 5 ] & 0x08 ) == 0 )
						{
							if( ( track_info_data[ 6 ] & 0x0f ) == 1 )
							{
								track_type = LIBSMDEV_TRACK_TYPE_MODE1_2048;
							}
							else if( ( track_info_data[ 6 ] & 0x0f ) == 2 )
							{
								track_type = LIBSMDEV_TRACK_TYPE_MODE2_2048;
							}
						}
					}
					else
					{
						track_type = LIBSMDEV_TRACK_TYPE_AUDIO;
					}
					if( libsmdev_handle_append_track(
					     internal_handle,
					     last_track_offset,
					     track_offset - last_track_offset,
					     track_type,
					     error ) != 1 )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_RUNTIME,
						 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
						 "%s: unable to append track: %d.",
						 function,
						 track_index );

						goto on_error;
					}
					track_index++;
				}
				last_track_offset = track_offset;

			 	if( toc_entries[ 3 ] != 0xb0 )
				{
					track_number = toc_entries[ 3 ];
				}
			}
			if( toc_entries[ 3 ] == 0xb0 )
			{
				if( session_offset >= next_session_offset )
				{
					liberror_error_set(
					 error,
					 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
					 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
					 "%s: invalid session offset value out of bounds.",
					 function );

					goto on_error;
				}
				if( ( session_index + 1 ) != toc_entries[ 0 ] )
				{
					liberror_error_set(
					 error,
					 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
					 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
					 "%s: invalid session number value out of bounds.",
					 function );

					goto on_error;
				}
				lead_out_size = 0;

				if( ( lead_out_offset >= session_offset )
				 && ( lead_out_offset < next_session_offset ) )
				{
					lead_out_size = next_session_offset - lead_out_offset;

					if( libsmdev_handle_append_lead_out(
					     internal_handle,
					     lead_out_offset,
					     lead_out_size,
					     error ) != 1 )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_RUNTIME,
						 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
						 "%s: unable to append lead_out: %d.",
						 function,
						 lead_out_index );

						goto on_error;
					}
					lead_out_index++;
				}
				session_size = next_session_offset - session_offset;

				if( ( session_index + 1 ) == number_of_sessions )
				{
					session_size -= lead_out_size;
				}
				if( libsmdev_handle_append_session(
				     internal_handle,
				     session_offset,
				     session_size,
				     error ) != 1 )
				{
					liberror_error_set(
					 error,
					 LIBERROR_ERROR_DOMAIN_RUNTIME,
					 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
					 "%s: unable to append session: %d.",
					 function,
					 session_index );

					goto on_error;
				}
				session_offset = next_session_offset;

				session_index++;
			}
			toc_entries     += 11;
			toc_data_offset += 11;

			entry_iterator++;
		}
		if( ( track_index + 1 ) == track_number )
		{
			if( track_offset < last_track_offset )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
				 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid track offset value out of bounds.",
				 function );

				goto on_error;
			}
			if( memory_set(
			     track_info_data,
			     0,
			     64 ) == NULL )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_MEMORY,
				 LIBERROR_MEMORY_ERROR_SET_FAILED,
				 "%s: unable to clear track info data.",
				 function );

				goto on_error;
			}
			response_count = libsmdev_scsi_read_track_information(
					  file_descriptor,
					  last_track_offset,
					  track_info_data,
					  64,
					  error );

			if( response_count == -1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_GET_FAILED,
				 "%s: unable retrieve track info data: %d.",
				 function,
				 track_index );

				goto on_error;
			}
#if defined( HAVE_DEBUG_OUTPUT )
			if( libnotify_verbose != 0 )
			{
				libnotify_printf(
				 "%s: track information data: %d:\n",
				 function,
				 track_index );
				libnotify_print_data(
				 track_info_data,
				 response_count );
			}
#endif
			if( track_info_data[ 2 ] != toc_entries[ 0 ] )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
				 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid track information data - session number value out of bounds.",
				 function );

				goto on_error;
			}
			if( track_info_data[ 3 ] != toc_entries[ 3 ] )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
				 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid track information data - track number value out of bounds.",
				 function );

				goto on_error;
			}
			track_type = LIBSMDEV_TRACK_TYPE_UNKNOWN;

			if( ( track_info_data[ 5 ] & 0x04 ) != 0 )
			{
				if( ( track_info_data[ 5 ] & 0x08 ) == 0 )
				{
					if( ( track_info_data[ 6 ] & 0x0f ) == 1 )
					{
						track_type = LIBSMDEV_TRACK_TYPE_MODE1_2048;
					}
					else if( ( track_info_data[ 6 ] & 0x0f ) == 2 )
					{
						track_type = LIBSMDEV_TRACK_TYPE_MODE2_2048;
					}
				}
			}
			else
			{
				track_type = LIBSMDEV_TRACK_TYPE_AUDIO;
			}
			if( libsmdev_handle_append_track(
			     internal_handle,
			     last_track_offset,
			     track_offset - last_track_offset,
			     track_type,
			     error ) != 1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
				 "%s: unable to append last track: %d.",
				 function,
				 track_index );

				goto on_error;
			}
		}
		if( session_index != number_of_sessions )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
			 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
			 "%s: invalid session index value out of bounds.",
			 function );

			goto on_error;
		}
		result = 1;
	}
	memory_free(
	 toc_data );

	toc_data = NULL;

	return( result );

on_error:
	if( toc_data != NULL )
	{
		memory_free(
		 toc_data );
	}
	libsmdev_array_resize(
	 internal_handle->tracks_array,
	 0,
	 &libsmdev_track_value_free,
	 NULL );

	libsmdev_array_resize(
	 internal_handle->lead_outs_array,
	 0,
	 &libsmdev_sector_range_free,
	 NULL );

	libsmdev_array_resize(
	 internal_handle->sessions_array,
	 0,
	 &libsmdev_sector_range_free,
	 NULL );

	return( -1 );
}

/* Retrieves the table of contents from the optical disk using IOCTL
 * Returns 1 if successful or -1 on error
 */
int libsmdev_optical_disc_get_table_of_contents_ioctl(
     int file_descriptor,
     libsmdev_internal_handle_t *internal_handle,
     liberror_error_t **error )
{
	struct cdrom_tochdr toc_header;
	struct cdrom_tocentry toc_entry;

	static char *function        = "libsmdev_optical_disc_get_table_of_contents_ioctl";
	uint32_t last_session_size   = 0;
	uint32_t last_session_offset = 0;
	uint32_t last_track_size     = 0;
	uint32_t last_track_offset   = 0;
	uint32_t offset              = 0;
	uint16_t entry_iterator      = 0;
	uint8_t first_entry          = 0;
	uint8_t last_entry           = 0;
	uint8_t last_track_type      = 0;
	uint8_t track_type           = 0;
	int number_of_sessions       = 0;
	int number_of_tracks         = 0;
	uint8_t session_index        = 0;
	uint8_t track_index          = 0;

	if( file_descriptor == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid file descriptor.",
		 function );

		return( -1 );
	}
	if( libsmdev_handle_get_number_of_sessions(
	     (libsmdev_handle_t *) internal_handle,
	     &number_of_sessions,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable retrieve number of sessions.",
		 function );

		goto on_error;
	}
	if( libsmdev_handle_get_number_of_tracks(
	     (libsmdev_handle_t *) internal_handle,
	     &number_of_tracks,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable retrieve number of tracks.",
		 function );

		goto on_error;
	}
	if( ioctl(
	     file_descriptor,
	     CDROMREADTOCHDR,
	     &toc_header ) == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_IO,
		 LIBERROR_IO_ERROR_IOCTL_FAILED,
		 "%s: unable to query device for: CDROMREADTOCHDR.",
		 function );

		goto on_error;
	}
	first_entry = toc_header.cdth_trk0;
	last_entry  = toc_header.cdth_trk1;

#if defined( HAVE_DEBUG_OUTPUT )
	if( libnotify_verbose != 0 )
	{
		libnotify_printf(
		 "%s: number of entries\t: %" PRIu8 "\n",
		 function,
		 last_entry );
	}
#endif
	for( entry_iterator = (uint16_t) first_entry;
	     entry_iterator <= (uint16_t) last_entry;
	     entry_iterator++ )
	{
		if( memory_set(
		     &toc_entry,
		     0,
		     sizeof( struct cdrom_tocentry ) ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_SET_FAILED,
			 "%s: unable clear toc entry.",
			 function );

			goto on_error;
		}
		toc_entry.cdte_track  = (uint8_t) entry_iterator;
		toc_entry.cdte_format = CDROM_LBA;

		if( ioctl(
		     file_descriptor,
		     CDROMREADTOCENTRY,
		     &toc_entry ) == -1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_IO,
			 LIBERROR_IO_ERROR_IOCTL_FAILED,
			 "%s: unable to query device for: CDROMREADTOCENTRY.",
			 function );

			goto on_error;
		}
		if( toc_entry.cdte_format == CDROM_LBA )
		{
			offset = (uint32_t) toc_entry.cdte_addr.lba;
		}
		else if( toc_entry.cdte_format == CDROM_MSF )
		{
			libsmdev_optical_disc_copy_msf_to_lba(
			 toc_entry.cdte_addr.msf.minute,
			 toc_entry.cdte_addr.msf.second,
			 toc_entry.cdte_addr.msf.frame,
			 offset );
		}
		else
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
			 "%s: unsupported CDTE format.",
			 function );

			goto on_error;
		}
		if( ( toc_entry.cdte_ctrl & CDROM_DATA_TRACK ) == 0 )
		{
			track_type = LIBSMDEV_TRACK_TYPE_AUDIO;
		}
		else
		{
			track_type = LIBSMDEV_TRACK_TYPE_MODE1_2048;
		}
#if defined( HAVE_DEBUG_OUTPUT )
		if( libnotify_verbose != 0 )
		{
			libnotify_printf(
			 "%s: entry: %" PRIu16 "",
			 function,
			 entry_iterator );

			if( ( toc_entry.cdte_ctrl & CDROM_DATA_TRACK ) == 0 )
			{
				libnotify_printf(
				 " (audio)" );
			}
			else
			{
				libnotify_printf(
				 " (data)" );
			}
			if( toc_entry.cdte_format == CDROM_LBA )
			{
				libnotify_printf(
				 " start\t: %" PRIu32 "",
				 toc_entry.cdte_addr.lba );
			}
			else if( toc_entry.cdte_format == CDROM_MSF )
			{
				libnotify_printf(
				 " start\t: %02" PRIu8 ":%02" PRIu8 ".%02" PRIu8 "",
				 toc_entry.cdte_addr.msf.minute,
				 toc_entry.cdte_addr.msf.second,
				 toc_entry.cdte_addr.msf.frame );
			}
			libnotify_printf(
			 " (offset: %" PRIu32 ")\n",
			 offset );
		}
#endif
		if( entry_iterator > first_entry )
		{
			if( ( offset < last_track_offset )
			 || ( offset < last_session_offset ) )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
				 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
				 "%s: invalid offset value out of bounds.",
				 function );

				goto on_error;
			}
			last_track_size = offset - last_track_offset;

			if( ( last_track_type == LIBSMDEV_TRACK_TYPE_MODE1_2048 )
			 || ( last_track_type != track_type ) )
			{
				if( session_index == 0 )
				{
					if( last_track_size < 11400 )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
						 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
						 "%s: invalid last track size value out of bounds.",
						 function );

						goto on_error;
					}
					last_track_size -= 11400;
				}
				else
				{
					if( last_track_size < 6900 )
					{
						liberror_error_set(
						 error,
						 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
						 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
						 "%s: invalid last track size value out of bounds.",
						 function );

						goto on_error;
					}
					last_track_size -= 6900;
				}
			}
			if( libsmdev_handle_append_track(
			     internal_handle,
			     last_track_offset,
			     last_track_size,
			     last_track_type,
			     error ) != 1 )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
				 "%s: unable to append track: %" PRIu8 ".",
				 function,
				 track_index );

				goto on_error;
			}
			track_index++;

			if( ( last_track_type == LIBSMDEV_TRACK_TYPE_MODE1_2048 )
			 || ( last_track_type != track_type ) )
			{
				last_session_size = offset - last_session_offset;

				if( libsmdev_handle_append_session(
				     internal_handle,
				     last_session_offset,
				     last_session_size,
				     error ) != 1 )
				{
					liberror_error_set(
					 error,
					 LIBERROR_ERROR_DOMAIN_RUNTIME,
					 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
					 "%s: unable to append session: %" PRIu8 ".",
					 function,
					 session_index );

					goto on_error;
				}
				session_index++;

				last_session_offset = offset;
			}
		}
		last_track_offset = offset;
		last_track_type   = track_type;
	}
	if( memory_set(
	     &toc_entry,
	     0,
	     sizeof( struct cdrom_tocentry ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable clear toc entry.",
		 function );

		goto on_error;
	}
	toc_entry.cdte_track  = CDROM_LEADOUT;
	toc_entry.cdte_format = CDROM_LBA;

	if( ioctl(
	     file_descriptor,
	     CDROMREADTOCENTRY,
	     &toc_entry ) == -1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_IO,
		 LIBERROR_IO_ERROR_IOCTL_FAILED,
		 "%s: unable to query device for: CDROMREADTOCENTRY.",
		 function );

		goto on_error;
	}
	if( toc_entry.cdte_format == CDROM_LBA )
	{
		offset = (uint32_t) toc_entry.cdte_addr.lba;
	}
	else if( toc_entry.cdte_format == CDROM_MSF )
	{
		libsmdev_optical_disc_copy_msf_to_lba(
		 toc_entry.cdte_addr.msf.minute,
		 toc_entry.cdte_addr.msf.second,
		 toc_entry.cdte_addr.msf.frame,
		 offset );
	}
	else
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_UNSUPPORTED_VALUE,
		 "%s: unsupported CDTE format.",
		 function );

		goto on_error;
	}
#if defined( HAVE_DEBUG_OUTPUT )
	if( libnotify_verbose != 0 )
	{
		libnotify_printf(
		 "\tLead out" );

		if( ( toc_entry.cdte_ctrl & CDROM_DATA_TRACK ) == 0 )
		{
			libnotify_printf(
			 " (audio)" );
		}
		else
		{
			libnotify_printf(
			 " (data)" );
		}
		if( toc_entry.cdte_format == CDROM_LBA )
		{
			libnotify_printf(
			 " start:\t%" PRIu32 "",
			 toc_entry.cdte_addr.lba );
		}
		else if( toc_entry.cdte_format == CDROM_MSF )
		{
			libnotify_printf(
			 " start:\t%02" PRIu8 ":%02" PRIu8 ".02%" PRIu8 "",
			 toc_entry.cdte_addr.msf.minute,
			 toc_entry.cdte_addr.msf.second,
			 toc_entry.cdte_addr.msf.frame );
		}
		libnotify_printf(
		 " (offset: %" PRIu32 ")\n\n",
		 offset );
	}
#endif
	if( ( offset < last_track_offset )
	 || ( offset < last_session_offset ) )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_BOUNDS,
		 "%s: invalid offset value out of bounds.",
		 function );

		goto on_error;
	}
	last_track_size = offset - last_track_offset;

	if( libsmdev_handle_append_track(
	     internal_handle,
	     last_track_offset,
	     last_track_size,
	     last_track_type,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
		 "%s: unable to append last track: %" PRIu8 ".",
		 function,
		 track_index );

		goto on_error;
	}
	last_session_size = offset - last_session_offset;

	if( libsmdev_handle_append_session(
	     internal_handle,
	     last_session_offset,
	     last_session_size,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
		 "%s: unable to append session: %" PRIu8 ".",
		 function,
		 session_index );

		goto on_error;
	}
	return( 1 );

on_error:
	libsmdev_array_resize(
	 internal_handle->tracks_array,
	 0,
	 &libsmdev_track_value_free,
	 NULL );

	return( -1 );
}

#endif

