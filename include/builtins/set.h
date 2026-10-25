/** set.h
 *
 * Declarations for the set builtin command.
 */

#ifndef BUILTINS_SET_H
#define BUILTINS_SET_H

#include "session.h"

/**
 * builtin_set - Set or unset shell options and positional parameters,
 *               or display shell variables.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param session Current shell session
 * @return 0 on success, non-zero on failure
 */
int builtin_set(int argc, char **argv, Session *session);

#endif /* BUILTINS_SET_H */
