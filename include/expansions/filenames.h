/** filenames.h
 * This file provides filename expansion (globbing) functionality.
 *
 * Filename expansion examples:
 *  - *.c -> a.c b.c c.c
 */

#ifndef EXPANSIONS_FILENAMES_H
#define EXPANSIONS_FILENAMES_H

#include "data/array.h" /* Array */
#include "session.h"    /* Session */

/**
 * Perform filename expansion on the given input string
 *
 * @param input The input string containing filename patterns to expand
 * @param session The current session (not used in this function but included
 * for consistency)
 * @return An Array of expanded filenames
 */
Array *filename_expansion(char *input, Session *session);

#endif /* EXPANSIONS_FILENAMES_H */
