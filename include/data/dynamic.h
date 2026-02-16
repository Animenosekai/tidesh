/* dynamic.h

This file defines the `Dynamic` structure and associated functions for
dynamically growing strings.
*/

#ifndef DATA_DYNAMIC_H
#define DATA_DYNAMIC_H

#include <stddef.h> /* size_t */

/* A structure to hold a dynamically growing string value */
typedef struct Dynamic {
    char  *value;    // The actual string value
    size_t length;   // The current length of the string
    size_t capacity; // The allocated capacity of the string
    size_t (
        *growing_strategy)( // The function which determines the new capacity
        size_t current_capacity, size_t required_capacity);
} Dynamic;

/**
 * Initialize a Dynamic
 *
 * @param value The Dynamic to initialize. If NULL, a new Dynamic will be
 * allocated
 * @param growing_strategy Function pointer to the growing strategy function
 *     The function needs to return the default capacity when given a required
 * capacity <= 0.
 *     If NULL, a default doubling strategy is used
 */
Dynamic *init_dynamic_with_strategy(Dynamic *value,
                                    size_t (*growing_strategy)(size_t, size_t));

/**
 * Initialize a Dynamic with the default growing strategy
 *
 * @param value The Dynamic to initialize. If NULL, a new Dynamic will be
 * allocated
 */
Dynamic *init_dynamic(Dynamic *value);

/**
 * Append a character to a Dynamic
 *
 * @param value The Dynamic to append to
 * @param character The character to append
 */
void dynamic_append(Dynamic *value, char character);

/**
 * Append a whole string to a Dynamic
 *
 * @param value The Dynamic to append to
 * @param string The string to append
 */
void dynamic_extend(Dynamic *value, char *string);

/**
 * Prepend a character to a Dynamic
 *
 * @param value The Dynamic to prepend to
 * @param character The character to prepend
 */
void dynamic_prepend(Dynamic *value, char character);

/**
 * Delete the last character from a Dynamic
 *
 * @param value The Dynamic to delete from
 */
void dynamic_delete_last(Dynamic *value);

/**
 * Insert a string at a specific position in a Dynamic
 *
 * @param value The Dynamic to insert into
 * @param position The position to insert at (0-based)
 * @param string The string to insert
 */
void dynamic_insert(Dynamic *value, size_t position, char *string);

/**
 * Remove a segment from a Dynamic
 *
 * @param value The Dynamic to remove from
 * @param position The position to start removing from (0-based)
 * @param length The number of characters to remove
 */
void dynamic_remove(Dynamic *value, size_t position, size_t length);

/**
 * Clear the contents of a Dynamic
 *
 * @param value The Dynamic to clear
 */
void dynamic_clear(Dynamic *value);

/**
 * Free a Dynamic
 *
 * @param value The Dynamic to free
 */
void free_dynamic(Dynamic *value);

/**
 * Convert a Dynamic to a string
 *
 * @param value The Dynamic to convert
 * @return The string representation of the Dynamic
 */
char *dynamic_to_string(Dynamic *value);

/**
 * Copy a Dynamic
 *
 * @param src The source Dynamic to copy from
 * @param dest The destination Dynamic to copy to, or NULL to allocate a new one
 * @return Pointer to the copied Dynamic, or NULL on failure
 */
Dynamic *dynamic_copy(Dynamic *src, Dynamic *dest);

#endif /* DATA_DYNAMIC_H */
