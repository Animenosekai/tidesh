#ifndef BUILTINS_ALIAS_H
#define BUILTINS_ALIAS_H

#include "session.h"

/**
 * The alias builtin command
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return 0 on success, non-zero on failure
 */
int builtin_alias(int argc, char **argv, Session *session);

#endif /* BUILTINS_ALIAS_H */
