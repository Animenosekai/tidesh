/** exit.h
 *
 * Declarations for the 'exit' builtin command.
 */

#ifndef BUILTIN_EXIT_H
#define BUILTIN_EXIT_H

#include "session.h"

/**
 * Change the current working directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_exit(int argc, char **argv, Session *session);

#endif /* BUILTIN_EXIT_H */
