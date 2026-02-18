/* aliases.h
 *
 * This module provides functionality for expanding command aliases
 * within user input strings.
 */

#ifndef EXPANSIONS_ALIASES_H
#define EXPANSIONS_ALIASES_H

#ifndef TIDESH_DISABLE_ALIASES

#include "data/array.h" /* Array */
#include "session.h"    /* Session */

/**
 * Expand an alias for the given input string
 *
 * @param input The input string to expand
 * @param session The current session containing alias definitions
 * @return An Array of expanded strings (or the original input if no alias
 * found)
 */
Array *alias_expansion(char *input, Session *session);

#endif /* TIDESH_DISABLE_ALIASES */

#endif /* EXPANSIONS_ALIASES_H */
