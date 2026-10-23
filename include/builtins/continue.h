/** continue.h
 *
 * Declarations for the 'continue' builtin command.
 */

#ifndef BUILTIN_CONTINUE_H
#define BUILTIN_CONTINUE_H

#include "session.h"

/**
 * Resume next iteration of enclosing for loop.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_continue(int argc, char **argv, Session *session);

#endif /* BUILTIN_CONTINUE_H */
