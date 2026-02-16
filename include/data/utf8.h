/* unicode.h

Handling of Unicode characters and strings
*/

#ifndef DATA_UTF8_H
#define DATA_UTF8_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

/**
 * Get the length of a UTF-8 character from its first byte.
 *
 * @param c The first byte of the UTF-8 character.
 * @return The length in bytes of the UTF-8 character.
 */
unsigned char utf8_charlen(char c);

/**
 * Get the length of a UTF-8 encoded string in characters.
 *
 * @param str Pointer to the UTF-8 encoded string.
 * @return Number of UTF-8 characters in the string.
 */
size_t utf8_strlen(char *str);

/**
 * Get the next UTF-8 character.
 *
 * @param str Pointer to the current position in the string.
 * @return Pointer to the start of the next character.
 */
char *utf8_next_char(char *str);

/**
 * Get the previous UTF-8 character.
 *
 * @param current Pointer to the current position in the string.
 * @param start Pointer to the start of the string (lower bound).
 * @return Pointer to the start of the previous character, or NULL if at start.
 */
char *utf8_prev_char(char *current, char *start);

#endif /* DATA_UTF8_H */
