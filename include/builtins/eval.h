#ifndef BUILTINS_EVAL_H
#define BUILTINS_EVAL_H

#include "session.h"

/**
 * The eval builtin command
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return The exit status of the executed command
 */
int builtin_eval(int argc, char **argv, Session *session);

#endif /* BUILTINS_EVAL_H */
