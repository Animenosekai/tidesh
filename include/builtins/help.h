/** help.h
 *
 * Declarations for the 'help' builtin command.
 */

#ifndef BUILTIN_HELP_H
#define BUILTIN_HELP_H

#include "session.h"

/**
 * Change the current working directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_help(int argc, char **argv, Session *session);

#endif /* BUILTIN_HELP_H */
