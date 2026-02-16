/** expand.h
 * Declarations for command expansions.
 *
 * This module provides functions to perform various shell expansions on input
 * strings, such as variable expansion, command substitution, tilde expansion,
 * etc.
 */

#ifndef EXPAND_H
#define EXPAND_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

#include "session.h" /* Session */

/**
 * Perform all expansions in order except aliases
 *
 * @param input Input string to expand
 * @param session Pointer to current Session
 * @return Array of expanded strings, or NULL on failure
 */
Array *full_expansion(char *input, Session *session);

#endif /* EXPAND_H */
