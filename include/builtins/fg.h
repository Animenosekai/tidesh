/** fg.h
 *
 * Declarations for the 'fg' builtin command.
 */

#ifndef BUILTIN_FG_H
#define BUILTIN_FG_H

#include "session.h"

/**
 * The fg builtin command.
 *
 * Brings a background job to the foreground.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return The exit status of the job
 */
int builtin_fg(int argc, char **argv, Session *session);

#endif /* BUILTIN_FG_H */
