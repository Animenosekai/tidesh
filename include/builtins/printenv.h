/** printenv.h
 *
 * Declarations for the 'printenv' builtin command.
 */

#ifndef BUILTIN_PRINTENV_H
#define BUILTIN_PRINTENV_H

#include "session.h"

/**
 * Change the current working directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_printenv(int argc, char **argv, Session *session);

#endif /* BUILTIN_PRINTENV_H */
