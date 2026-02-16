/* braces.h
 * This module provides functionality for performing brace expansions
 * within user input strings.
 *
 * Brace expansion examples:
 *  - abc -> abc
 *  - a{b,c,d}e -> abe ace ade
 *  - a{b,c,d}e -> abe ace ade
 *  - a{{,c}d,e} -> ad acd ae
 */

#ifndef EXPANSIONS_BRACES_H
#define EXPANSIONS_BRACES_H

#include "data/array.h" /* Array */
#include "session.h"    /* Session */

/**
 * Perform brace expansion on the given input string.
 *
 * @param input The input string containing braces to expand
 * @param session The current session (not used in this function but included
 * for consistency)
 * @return An Array of expanded strings
 */
Array *brace_expansion(char *input, Session *session);

#endif /* EXPANSIONS_BRACES_H */
