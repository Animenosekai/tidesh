/** sandbox.h
 *
 * Declarations for the 'sandbox' built-in context manager.
 */

#ifndef CONTEXT_SANDBOX_H
#define CONTEXT_SANDBOX_H

#include <stddef.h> /* size_t */

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter sandbox context manager.
 * Configures fine-grained filesystem sandboxing with read-only (ro),
 * read-write (rw), and denied (deny) paths.
 *
 * @param session Pointer to Session
 * @param args Array of ContextArg arguments
 * @param argc Number of arguments
 * @param state Out pointer to saved context state
 * @return 0 on success, non-zero on failure
 */
int context_enter_sandbox(Session *session, ContextArg *args, size_t argc,
                          void **state);

/**
 * Exit sandbox context manager.
 * Restores previous sandbox policy.
 *
 * @param session Pointer to Session
 * @param status Exit status of the with block
 * @param state Saved state from context_enter_sandbox
 * @return 0 on success, non-zero on failure
 */
int context_exit_sandbox(Session *session, int status, void *state);

#endif /* CONTEXT_SANDBOX_H */
