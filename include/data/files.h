/** files.h
 *
 * Utility functions for file operations.
 */

#ifndef DATA_FILES_H
#define DATA_FILES_H

#include <stdio.h>

/**
 * Read the entire content of a file into a dynamically allocated string.
 *
 * The returned string is null-terminated and must be freed by the caller.
 *
 * @param f The FILE pointer to read from
 * @return A dynamically allocated string containing the file contents, or NULL
 * on failure
 */
char *read_all(FILE *f);

#endif /* DATA_FILES_H */
