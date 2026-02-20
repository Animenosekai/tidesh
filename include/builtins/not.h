/** type.h
 *
 * Declarations for the 'type' builtin command.
 */

#ifndef BUILTIN_NOT_H
#define BUILTIN_NOT_H

#include "session.h"

/**
 * The not builtin command.
 *
 * Returns the logical negation of the exit status of a command. If the command
 * succeeds (exit status 0), not returns 1. If the command fails (non-zero exit
 * status), not returns 0.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return The exit status (0 if all found, 1 if any not found)
 */
int builtin_not(int argc, char **argv, Session *session);

#endif /* BUILTIN_NOT_H */
