/*
 * List type functions
 *
 * Copyright (C) 2008-2009, Joachim Metz <forensics@hoffmannbv.nl>,
 * Hoffmann Investigations. All rights reserved.
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <memory.h>
#include <types.h>

#include <liberror.h>

#include "libewf_list_type.h"
#include "libewf_notify.h"

/* Frees a list including the elements
 * Uses the value_free_function to free the element value
 * Returns 1 if successful or -1 on error
 */
int libewf_list_free(
     libewf_list_t *list,
     int (*value_free_function)( intptr_t *value ),
     liberror_error_t **error )
{
	static char *function = "libewf_list_free";
	int result            = 0;

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	result = libewf_list_empty(
	          list,
	          value_free_function,
	          error );

	if( result != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
		 "%s: unable to empty list.",
		 function );
	}
	memory_free(
	 list );

	return( result );
}

/* Empties a list and frees the elements
 * Uses the value_free_function to free the element value
 * Returns 1 if successful or -1 on error
 */
int libewf_list_empty(
     libewf_list_t *list,
     int (*value_free_function)( intptr_t *value ),
     liberror_error_t **error )
{
	libewf_list_element_t *list_element = NULL;
	static char *function               = "libewf_list_empty";
	int amount_of_elements              = 0;
	int iterator                        = 0;
	int result                          = 1;

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	if( list->amount_of_elements > 0 )
	{
		amount_of_elements = list->amount_of_elements;

		for( iterator = 0; iterator < amount_of_elements; iterator++ )
		{
			list_element = list->first;

			if( list_element == NULL )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
				 "%s: corruption detected in element: %d.",
				 function,
				 iterator + 1 );

				return( -1 );
			}
			list->first = list_element->next;

			if( list->last == list_element )
			{
				list->last = list_element->next;
			}
			list->amount_of_elements -= 1;

			if( list_element->next != NULL )
			{
				list_element->next->previous = NULL;
			}
			list_element->next = NULL;

			if( ( value_free_function != NULL )
			 && ( value_free_function(
			       list_element->value ) != 1 ) )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
				 "%s: unable to free value in element: %d.",
				 function,
				 iterator + 1 );

				result = -1;
			}
			memory_free(
			 list_element );
		}
	}
	return( result );
}

/* Prepend an element to the list
 * Returns 1 if successful or -1 on error
 */
int libewf_list_prepend_element(
     libewf_list_t *list,
     libewf_list_element_t *element,
     liberror_error_t **error )
{
	static char *function = "libewf_list_prepend_element";

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid element.",
		 function );

		return( -1 );
	}
	if( list->first != NULL )
	{
		list->first->previous = element;
		element->next         = list->first;
	}
	if( list->last == NULL )
	{
		list->last = element;
	}
	list->first               = element;
	list->amount_of_elements += 1;

	return( 1 );
}

/* Prepend a value to the list
 * Creates a new list element
 * Returns 1 if successful or -1 on error
 */
int libewf_list_prepend_value(
     libewf_list_t *list,
     intptr_t *value,
     liberror_error_t **error )
{
	libewf_list_element_t *element = NULL;
	static char *function          = "libewf_list_prepend_value";

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
	element = (libewf_list_element_t *) memory_allocate(
	                                     sizeof( libewf_list_element_t ) );

	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create element.",
		 function );

		return( -1 );
	}
	if( memory_set(
	     element,
	     0,
	     sizeof( libewf_list_element_t ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear element.",
		 function );

		memory_free(
		 element );

		return( -1 );
	}
	element->value = value;

	if( libewf_list_prepend_element(
	     list,
	     element,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
		 "%s: unable to prepend element to list.",
		 function );

		memory_free(
		 element );

		return( -1 );
	}
	return( 1 );
}

/* Append an element to the list
 * Returns 1 if successful or -1 on error
 */
int libewf_list_append_element(
     libewf_list_t *list,
     libewf_list_element_t *element,
     liberror_error_t **error )
{
	static char *function = "libewf_list_append_element";

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid element.",
		 function );

		return( -1 );
	}
	if( list->first == NULL )
	{
		list->first = element;
	}
	if( list->last != NULL )
	{
		list->last->next  = element;
		element->previous = list->last;
	}
	list->last                = element;
	list->amount_of_elements += 1;

	return( 1 );
}

/* Append a value to the list
 * Creates a new list element
 * Returns 1 if successful or -1 on error
 */
int libewf_list_append_value(
     libewf_list_t *list,
     intptr_t *value,
     liberror_error_t **error )
{
	libewf_list_element_t *element = NULL;
	static char *function          = "libewf_list_append_value";

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
	element = (libewf_list_element_t *) memory_allocate(
	                                     sizeof( libewf_list_element_t ) );

	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create element.",
		 function );

		return( -1 );
	}
	if( memory_set(
	     element,
	     0,
	     sizeof( libewf_list_element_t ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear element.",
		 function );

		memory_free(
		 element );

		return( -1 );
	}
	element->value = value;

	if( libewf_list_append_element(
	     list,
	     element,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
		 "%s: unable to append element to list.",
		 function );

		memory_free(
		 element );

		return( -1 );
	}
	return( 1 );
}

/* Inserts a list element into the list
 * Uses the value_compare_function to determine the order of the child nodes
 * Returns 1 if successful, 0 if the node already exists or -1 on error
 */
int libewf_list_insert_element(
     libewf_list_t *list,
     libewf_list_element_t *element,
     int (*value_compare_function)( intptr_t *first_value, intptr_t *second_value ),
     liberror_error_t **error )
{
	libewf_list_element_t *list_element = NULL;
	static char *function               = "libewf_list_insert_element";
	int result                          = 0;
	int iterator                        = 0;

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid element.",
		 function );

		return( -1 );
	}
	if( ( element->previous != NULL )
	 || ( element->next != NULL ) )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
		 "%s: element already part of a list.",
		 function );

		return( -1 );
	}
	if( value_compare_function == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid value compare function.",
		 function );

		return( -1 );
	}
	if( list->amount_of_elements == 0 )
	{
		if( list->first != NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: corruption detected - first already set.",
			 function );

			return( -1 );
		}
		if( list->last != NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: corruption detected - last already set.",
			 function );

			return( -1 );
		}
		list->first = element;
		list->last  = element;
	}
	else
	{
		if( list->first == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: corruption detected - missing first.",
			 function );

			return( -1 );
		}
		if( list->last == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
			 "%s: corruption detected - missing last.",
			 function );

			return( -1 );
		}
		list_element = list->first;

		for( iterator = 0; iterator < list->amount_of_elements; iterator++ )
		{
			result = value_compare_function(
			          element->value,
			          list_element->value );

			if( result == 0 )
			{
				return( 0 );
			}
			else if( result <= -1 )
			{
				element->previous = list_element->previous;
				element->next     = list_element;

				if( list_element == list->first )
				{
					list->first = element;
				}
				else if( list_element->previous == NULL )
				{
					liberror_error_set(
					 error,
					 LIBERROR_ERROR_DOMAIN_RUNTIME,
					 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
					 "%s: corruption detected - missing previous in list element: %d.",
					 function,
					 iterator + 1 );

					return( -1 );
				}
				else
				{
					list_element->previous->next = element;
				}
				list_element->previous = element;

				break;
			}
			list_element = list_element->next;
		}
		if( result >= 1 )
		{
			element->previous = list->last;
			list->last->next  = element;
			list->last        = element;
		}
	}
	list->amount_of_elements += 1;

	return( 1 );
}

/* Inserts a value to the list
 * Creates a new list element
 * Uses the value_compare_function to determine the order of the child nodes
 * Returns 1 if successful, 0 if the node already exists or -1 on error
 */
int libewf_list_insert_value(
     libewf_list_t *list,
     intptr_t *value,
     int (*value_compare_function)( intptr_t *first_value, intptr_t *second_value ),
     liberror_error_t **error )
{
	libewf_list_element_t *element = NULL;
	static char *function          = "libewf_list_insert_value";

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
	element = (libewf_list_element_t *) memory_allocate(
	                                     sizeof( libewf_list_element_t ) );

	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
		 "%s: unable to create element.",
		 function );

		return( -1 );
	}
	if( memory_set(
	     element,
	     0,
	     sizeof( libewf_list_element_t ) ) == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_MEMORY,
		 LIBERROR_MEMORY_ERROR_SET_FAILED,
		 "%s: unable to clear element.",
		 function );

		memory_free(
		 element );

		return( -1 );
	}
	element->value = value;

	if( libewf_list_insert_element(
	     list,
	     element,
	     value_compare_function,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_APPEND_FAILED,
		 "%s: unable to insert element to list.",
		 function );

		memory_free(
		 element );

		return( -1 );
	}
	return( 1 );
}

/* Removes an element from the list
 * Returns 1 if successful or -1 on error
 */
int libewf_list_remove_element(
     libewf_list_t *list,
     libewf_list_element_t *element,
     liberror_error_t **error )
{
	static char *function = "libewf_list_remove_element";

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid element.",
		 function );

		return( -1 );
	}
	if( element == list->first )
	{
		list->first = element->next;
	}
	if( element == list->last )
	{
		list->last = element->previous;
	}
	if( element->next != NULL )
	{
		element->next->previous = element->previous;
	}
	if( element->previous != NULL )
	{
		element->previous->next = element->next;
	}
	element->next             = NULL;
	element->previous         = NULL;
	list->amount_of_elements -= 1;

	return( 1 );
}

/* Retrieves the amount of elements in the list
 * Returns the amount of elements if successful or -1 on error
 */
int libewf_list_get_amount_of_elements(
     libewf_list_t *list,
     liberror_error_t **error )
{
	static char *function = "libewf_list_get_amount_of_elements";

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	return( list->amount_of_elements );
}

/* Retrieves a specific element from the list
 * Returns 1 if successful, 0 if not available or -1 on error
 */
int libewf_list_get_element(
     libewf_list_t *list,
     int element_index,
     libewf_list_element_t **element,
     liberror_error_t **error )
{
	libewf_list_element_t *list_element = NULL;
	static char *function               = "libewf_list_get_element";
	int iterator                        = 0;

	if( list == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid list.",
		 function );

		return( -1 );
	}
	if( ( element_index < 0 )
	 || ( element_index >= list->amount_of_elements ) )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_OUT_OF_RANGE,
		 "%s: invalid element index out of range.",
		 function );

		return( -1 );
	}
	if( element == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid element.",
		 function );

		return( -1 );
	}
	if( element_index < ( list->amount_of_elements / 2 ) )
	{
		list_element = list->first;

		for( iterator = 0; iterator < element_index; iterator++ )
		{
			if( list_element == NULL )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
				 "%s: corruption detected in element: %d.",
				 function,
				 iterator + 1 );

				return( -1 );
			}
			list_element = list_element->next;
		}
	}
	else
	{
		list_element = list->last;

		for( iterator = ( list->amount_of_elements - 1 ); iterator > element_index; iterator-- )
		{
			if( list_element == NULL )
			{
				liberror_error_set(
				 error,
				 LIBERROR_ERROR_DOMAIN_RUNTIME,
				 LIBERROR_RUNTIME_ERROR_VALUE_MISSING,
				 "%s: corruption detected in element: %d.",
				 function,
				 iterator + 1 );

				return( -1 );
			}
			list_element = list_element->previous;
		}
	}
	*element = list_element;

	if( list_element == NULL )
	{
		return( 0 );
	}
	return( 1 );
}

