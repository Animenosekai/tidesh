/** popd.h
 *
 * Declarations for the 'popd' builtin command.
 */

#ifndef BUILTIN_POPD_H
#define BUILTIN_POPD_H

#include "session.h"

/**
 * Pop directory from stack and change current directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_popd(int argc, char **argv, Session *session);

#endif /* BUILTIN_POPD_H */
