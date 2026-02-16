/** cd.h
 *
 * Declarations for the 'cd' builtin command.
 */

#ifndef BUILTIN_CD_H
#define BUILTIN_CD_H

#include "session.h"

/**
 * Change the current working directory.
 *
 * @param argc Number of arguments
 * @param argv Argument vector
 * @param session The session context
 * @return Exit status
 */
int builtin_cd(int argc, char **argv, Session *session);

#endif /* BUILTIN_CD_H */
