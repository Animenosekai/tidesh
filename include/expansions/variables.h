/** variables.h
 * This file provides variable expansion functionality.
 *
 * Variables expansion examples:
 *  - $VAR -> value of VAR
 *  - ${VAR} -> value of VAR
 *  - ${VAR:-default} -> value of VAR or default if unset
 *  - ${VAR:=default} -> value of VAR or set to default if unset
 *  - ${VAR:+alt} -> alt if VAR is set, else empty
 *  - ${VAR:?error} -> error if VAR is unset
 *  - ${#VAR} -> length of VAR
 *  - $=VAR -> split VAR
 *  - ${=VAR} -> split VAR
 */

#ifndef EXPANSIONS_VARIABLES_H
#define EXPANSIONS_VARIABLES_H

#include "data/array.h" /* Array */
#include "session.h"    /* Session */

/**
 * Perform variable expansion on the given input string
 *
 * @param input The input string containing variables to expand
 * @param session The current session (used for environment variables)
 * @return An Array of expanded strings
 */
Array *variable_expansion(char *input, Session *session);

#endif /* EXPANSIONS_VARIABLES_H */
