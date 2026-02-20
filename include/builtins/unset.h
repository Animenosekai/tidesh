#ifndef BUILTINS_UNSET_H
#define BUILTINS_UNSET_H

#include "session.h"

/**
 * The unset builtin command
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return 0 on success, non-zero on failure
 */
int builtin_unset(int argc, char **argv, Session *session);

#endif /* BUILTINS_UNSET_H */
