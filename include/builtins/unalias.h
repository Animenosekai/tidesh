#ifndef BUILTINS_UNALIAS_H
#define BUILTINS_UNALIAS_H

#include "session.h"

/**
 * The unalias builtin command
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return 0 on success, non-zero on failure
 */
int builtin_unalias(int argc, char **argv, Session *session);

#endif /* BUILTINS_UNALIAS_H */
