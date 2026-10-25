/** colon.h
 *
 * Declarations for the colon (:) builtin command.
 */

#ifndef BUILTINS_COLON_H
#define BUILTINS_COLON_H

#include "session.h"

/**
 * builtin_colon - Null command (does nothing and returns 0).
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param session Current shell session
 * @return 0
 */
int builtin_colon(int argc, char **argv, Session *session);

#endif /* BUILTINS_COLON_H */
