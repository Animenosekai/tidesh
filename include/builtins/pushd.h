/** pushd.h
 *
 * Declarations for the 'pushd' builtin command.
 */

#ifndef BUILTIN_PUSHD_H
#define BUILTIN_PUSHD_H

#include "session.h"

/**
 * Push directory to stack and change current directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_pushd(int argc, char **argv, Session *session);

#endif /* BUILTIN_PUSHD_H */
