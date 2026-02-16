/* array.h

This file defines the `Array` structure and associated functions for dynamic
string arrays.
*/

#ifndef DATA_ARRAY_H
#define DATA_ARRAY_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

typedef struct Array {
    char **items;    // The actual array of strings
    size_t count;    // The number of strings stored
    size_t capacity; // The allocated capacity of the array
    size_t(          // The function which determines the new capacity
        *growing_strategy)(size_t current_capacity, size_t required_capacity);
} Array;

/**
 * Initialize an Array
 *
 * @param array The `Array` to initialize. If NULL, a new Array will be
 * allocated
 * @param growing_strategy Function pointer to the growing strategy function
 *     The function needs to return the default capacity when given a required
 * capacity <= 0.
 *     If NULL, a default doubling strategy is used
 */
Array *init_array_with_strategy(Array *array,
                                size_t (*growing_strategy)(size_t, size_t));

/**
 * Initialize an Array with the default growing strategy
 *
 * @param array The `Array` to initialize. If NULL, a new Array will be
 * allocated
 */
Array *init_array(Array *array);

/**
 * Add a string to the Array
 *
 * @param array The Array to add to
 * @param string The string to add
 */
bool array_add(Array *array, char *string);

/**
 * Extend a Array with another Array
 *
 * @param array The Array to extend
 * @param other The Array to copy from
 */
void array_extend(Array *array, Array *other);

/**
 * Insert an element at the given index
 *
 * @param array The Array to insert into
 * @param index The index to insert at
 * @param string The string to insert
 */
void array_insert(Array *array, size_t index, char *string);

/**
 * Remove the element at the given index and return it
 *
 * @param array The Array to pop from
 * @param index The index to pop
 * @return The popped string
 */
char *array_pop(Array *array, size_t index);

/**
 * Remove the element at the given index
 *
 * @param array The Array to remove from
 * @param index The index to remove
 */
void array_remove(Array *array, size_t index); /* discards string */

/**
 * Set the element at the given index
 *
 * @param array The Array to modify
 * @param index The index to set
 * @param string The new string to set
 * @param free_old Whether to free the old string at that index
 */
void array_set(Array *array, size_t index, char *string, bool free_old);

/**
 * Sort the Array in-place in lexicographical order
 *
 * @param array The Array to sort
 */
void array_sort(Array *array);

/**
 * Clear the Array without freeing its structure
 *
 * @param array The Array to clear
 */
void array_clear(Array *array);

/**
 * Copy the contents of one Array to another
 *
 * @param src The source Array
 * @param dest The destination Array, if NULL a new Array will be allocated
 * @return Pointer to the destination Array
 */
Array *array_copy(Array *src, Array *dest);

/**
 * Free the Array and its contents
 *
 * @param array The Array to free
 */
void free_array(Array *array);

#endif /* DATA_ARRAY_H */
