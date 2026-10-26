/** session.h
 *
 * Declarations for the 'session' built-in context manager and session snapshots.
 */

#ifndef CONTEXTS_SESSION_H
#define CONTEXTS_SESSION_H

#include <stddef.h>

typedef struct Session Session;
typedef struct ContextArg ContextArg;

/**
 * An opaque snapshot of a shell session state.
 */
typedef struct SessionSnapshot SessionSnapshot;

/**
 * Create a snapshot of the current session state.
 *
 * @param session Pointer to Session to snapshot
 * @return Pointer to newly allocated SessionSnapshot, or NULL on failure
 */
SessionSnapshot *session_snapshot_create(Session *session);

/**
 * Restore the session state from a previously created snapshot.
 *
 * @param session Pointer to Session to restore
 * @param snapshot Pointer to SessionSnapshot to restore from
 */
void session_snapshot_restore(Session *session, SessionSnapshot *snapshot);

/**
 * Free a SessionSnapshot and all its resources.
 *
 * @param snapshot Pointer to SessionSnapshot to free
 */
void session_snapshot_free(SessionSnapshot *snapshot);

/**
 * Enter handler for the 'session' context manager.
 * Snapshots the entire session state.
 *
 * @param session Pointer to current session
 * @param args Array of arguments (unused)
 * @param argc Number of arguments (unused)
 * @param state Out-parameter to store allocated SessionSnapshot
 * @return 0 on success, non-zero on error
 */
int context_enter_session(Session *session, ContextArg *args, size_t argc,
                          void **state);

/**
 * Exit handler for the 'session' context manager.
 * Restores the entire session state from the snapshot.
 *
 * @param session Pointer to current session
 * @param status Exit status of the enclosed block body
 * @param state SessionSnapshot pointer from enter handler
 * @return 0 on success, non-zero on error
 */
int context_exit_session(Session *session, int status, void *state);

#endif /* CONTEXTS_SESSION_H */
