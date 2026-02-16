/** tildes.h
 * This file provides tilde expansion functionality.
 *
 * Tilde expansion examples:
 *  - ~ -> /home/current_user
 *  - ~user -> /home/user
 *  - ~+ -> current directory
 *  - ~- -> previous directory
 *  - ~N -> Nth directory in directory stack
 */

#ifndef EXPANSIONS_TILDES_H
#define EXPANSIONS_TILDES_H

#include "data/array.h" /* Array */
#include "session.h"    /* Session */

/**
 * Perform tilde expansion on the given input string
 *
 * @param input The input string containing tildes to expand
 * @param session The current session (used for directory and user info)
 * @return An Array of expanded strings
 */
Array *tilde_expansion(char *input, Session *session);

#endif /* EXPANSIONS_TILDES_H */
