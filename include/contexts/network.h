/** network.h
 *
 * Declarations for the 'network' built-in context manager.
 */

#ifndef CONTEXT_NETWORK_H
#define CONTEXT_NETWORK_H

#include <stddef.h> /* size_t */

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter network context manager.
 * Restricts network access to specified domains, allowed ports,
 * blocked ports, and offline/airgap modes.
 *
 * @param session Pointer to Session
 * @param args Array of ContextArg arguments
 * @param argc Number of arguments
 * @param state Out pointer to saved context state
 * @return 0 on success, non-zero on failure
 */
int context_enter_network(Session *session, ContextArg *args, size_t argc,
                          void **state);

/**
 * Exit network context manager.
 * Restores previous network policy.
 *
 * @param session Pointer to Session
 * @param status Exit status of the with block
 * @param state Saved state from context_enter_network
 * @return 0 on success, non-zero on failure
 */
int context_exit_network(Session *session, int status, void *state);

#endif /* CONTEXT_NETWORK_H */
