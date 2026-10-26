/** env.h
 *
 * Declarations for the 'env' built-in context manager.
 */

#ifndef CONTEXTS_ENV_H
#define CONTEXTS_ENV_H

#include <stddef.h>

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter handler for the 'env' context manager.
 *
 * @param session Pointer to current session
 * @param args Array of arguments
 * @param argc Number of arguments
 * @param state Out-parameter to store saved environment backup state
 * @return 0 on success, non-zero on error
 */
int context_enter_env(Session *session, ContextArg *args, size_t argc,
                      void **state);

/**
 * Exit handler for the 'env' context manager.
 *
 * @param session Pointer to current session
 * @param status Exit status of the enclosed block body
 * @param state Saved backup state from enter handler
 * @return 0 on success, non-zero on error
 */
int context_exit_env(Session *session, int status, void *state);

#endif /* CONTEXTS_ENV_H */
