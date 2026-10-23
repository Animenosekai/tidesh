/** break.h
 *
 * Declarations for the 'break' builtin command.
 */

#ifndef BUILTIN_BREAK_H
#define BUILTIN_BREAK_H

#include "session.h"

/**
 * Break out of enclosing for loop.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_break(int argc, char **argv, Session *session);

#endif /* BUILTIN_BREAK_H */
