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
 * Perform command substitution $(...) on input string
 *
 * @param input Input string
 * @param session Pointer to current Session
 * @return Expanded string, or NULL on failure
 */
char *expand_command_substitution(const char *input, Session *session);

/**
 * Perform arithmetic expansion $((...)) on input string
 *
 * @param input Input string
 * @param session Pointer to current Session
 * @return Expanded string, or NULL on failure
 */
char *expand_arithmetic(const char *input, Session *session);

/**
 * Perform all expansions in order except aliases
 *
 * @param input Input string to expand
 * @param session Pointer to current Session
 * @return Array of expanded strings, or NULL on failure
 */
Array *full_expansion(char *input, Session *session);

#endif /* EXPAND_H */
