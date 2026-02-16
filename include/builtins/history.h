/** history.h
 *
 * Declarations for the 'history' builtin command.
 */

#ifndef BUILTIN_HISTORY_H
#define BUILTIN_HISTORY_H

#include "session.h"

/**
 * Change the current working directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_history(int argc, char **argv, Session *session);

#endif /* BUILTIN_HISTORY_H */
