/** clear.h
 *
 * Declarations for the 'clear' builtin command.
 */

#ifndef BUILTIN_CLEAR_H
#define BUILTIN_CLEAR_H

#include "session.h"

/**
 * Change the current working directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_clear(int argc, char **argv, Session *session);

#endif /* BUILTIN_CLEAR_H */
