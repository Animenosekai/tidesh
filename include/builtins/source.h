/** source.h
 *
 * Declarations for the 'source' builtin command.
 */

#ifndef BUILTIN_SOURCE_H
#define BUILTIN_SOURCE_H

#include "session.h"

/**
 * The source builtin command.
 *
 * Executes the commands in a file in the current shell context (not in a
 * subshell). Can be invoked as 'source' or '.'.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return The exit status of the executed file
 */
int builtin_source(int argc, char **argv, Session *session);

#endif /* BUILTIN_SOURCE_H */
