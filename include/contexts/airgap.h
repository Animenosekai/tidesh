/** airgap.h
 *
 * Declarations for the 'airgap' built-in context manager.
 */

#ifndef CONTEXT_AIRGAP_H
#define CONTEXT_AIRGAP_H

#include <stddef.h> /* size_t */

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter airgap context manager.
 * Disables all external network connectivity within the block.
 *
 * @param session Pointer to Session
 * @param args Array of ContextArg arguments (e.g. loopback=true)
 * @param argc Number of arguments
 * @param state Out pointer to saved context state
 * @return 0 on success, non-zero on failure
 */
int context_enter_airgap(Session *session, ContextArg *args, size_t argc,
                         void **state);

/**
 * Exit airgap context manager.
 * Restores previous network policy.
 *
 * @param session Pointer to Session
 * @param status Exit status of the with block
 * @param state Saved state from context_enter_airgap
 * @return 0 on success, non-zero on failure
 */
int context_exit_airgap(Session *session, int status, void *state);

#endif /* CONTEXT_AIRGAP_H */
