/** return.h
 *
 * Declarations for the return builtin command.
 */

#ifndef BUILTINS_RETURN_H
#define BUILTINS_RETURN_H

#include "session.h"

/**
 * builtin_return - Return from a function or sourced script.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param session Current shell session
 * @return 0 on success, non-zero on failure
 */
int builtin_return(int argc, char **argv, Session *session);

#endif /* BUILTINS_RETURN_H */
