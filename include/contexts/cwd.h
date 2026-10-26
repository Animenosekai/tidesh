/** cwd.h
 *
 * Declarations for the 'cwd' (and 'cd') built-in context manager.
 */

#ifndef CONTEXTS_CWD_H
#define CONTEXTS_CWD_H

#include <stddef.h>

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter handler for the 'cwd' context manager.
 *
 * @param session Pointer to current session
 * @param args Array of arguments
 * @param argc Number of arguments
 * @param state Out-parameter to store saved state (original working directory)
 * @return 0 on success, non-zero on error
 */
int context_enter_cwd(Session *session, ContextArg *args, size_t argc,
                      void **state);

/**
 * Exit handler for the 'cwd' context manager.
 *
 * @param session Pointer to current session
 * @param status Exit status of the enclosed block body
 * @param state Saved state from enter handler
 * @return 0 on success, non-zero on error
 */
int context_exit_cwd(Session *session, int status, void *state);

#endif /* CONTEXTS_CWD_H */
