/** umask.h
 *
 * Declarations for the 'umask' built-in context manager.
 */

#ifndef CONTEXTS_UMASK_H
#define CONTEXTS_UMASK_H

#include <stddef.h>

typedef struct Session Session;
typedef struct ContextArg ContextArg;

/**
 * Enter handler for the 'umask' context manager.
 *
 * @param session Pointer to current session
 * @param args Array containing octal umask argument
 * @param argc Number of arguments
 * @param state Out-parameter to store previous umask
 * @return 0 on success, non-zero on error
 */
int context_enter_umask(Session *session, ContextArg *args, size_t argc,
                        void **state);

/**
 * Exit handler for the 'umask' context manager.
 *
 * @param session Pointer to current session
 * @param status Exit status of the enclosed block body
 * @param state Pointer to previous umask from enter handler
 * @return 0 on success, non-zero on error
 */
int context_exit_umask(Session *session, int status, void *state);

#endif /* CONTEXTS_UMASK_H */
