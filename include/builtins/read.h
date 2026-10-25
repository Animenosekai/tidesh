/** read.h
 *
 * Declarations for the read builtin command.
 */

#ifndef BUILTINS_READ_H
#define BUILTINS_READ_H

#include "session.h"

/**
 * builtin_read - Read a line from standard input into shell variables.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param session Current shell session
 * @return 0 on success, non-zero on EOF or error
 */
int builtin_read(int argc, char **argv, Session *session);

#endif /* BUILTINS_READ_H */
