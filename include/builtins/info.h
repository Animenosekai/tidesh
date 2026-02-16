/** info.h
 *
 * Declaration of the info builtin command.
 */

#ifndef BUILTINS_INFO_H
#define BUILTINS_INFO_H

#include "session.h"

/**
 * The info builtin command shows information about the running shell.
 *
 * @param argc Number of arguments
 * @param argv Array of arguments
 * @param session Pointer to current Session
 * @return 0 on success, non-zero on failure
 */
int builtin_info(int argc, char **argv, Session *session);

#endif /* BUILTINS_INFO_H */
