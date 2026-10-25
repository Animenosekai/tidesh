/** arithmetic.h
 * Declarations for integer arithmetic expansion $(( expression )).
 *
 * This module provides functions to parse, evaluate and expand integer
 * arithmetic expressions in POSIX shell format.
 */

#ifndef EXPANSIONS_ARITHMETIC_H
#define EXPANSIONS_ARITHMETIC_H

#include <stdbool.h> /* bool */
#include <stdint.h>  /* int64_t */

#include "session.h" /* Session */

/**
 * Perform arithmetic expansion $((...)) on input string.
 *
 * @param input Input string containing $(( expression ))
 * @param session Pointer to current Session
 * @return Newly-allocated expanded string, or NULL on unrecoverable failure
 */
char *expand_arithmetic(const char *input, Session *session);

/**
 * Evaluate an arithmetic expression string.
 *
 * @param expr The expression string to evaluate
 * @param session Current session (for variables and environment)
 * @param result Pointer to int64_t to store the result
 * @return true on success, false on syntax or evaluation error
 */
bool evaluate_arithmetic_expression(const char *expr, Session *session,
                                    int64_t *result);

#endif /* EXPANSIONS_ARITHMETIC_H */
