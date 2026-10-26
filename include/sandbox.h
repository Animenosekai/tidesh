/** sandbox.h
 *
 * Master header for filesystem and network sandboxing in tidesh.
 * Coordinates filesystem isolation and network restrictions across
 * UNIX-like systems and macOS.
 */

#ifndef SANDBOX_H
#define SANDBOX_H

#include "sandbox/fs.h"
#include "sandbox/limits.h"
#include "sandbox/net.h"

/**
 * Apply all active sandbox restrictions (filesystem and network) to a
 * forked child process before execve.
 *
 * @param session Pointer to Session
 */
void sandbox_apply_child(Session *session);

/**
 * Check whether any sandbox layer (filesystem or network) is active.
 *
 * @param session Pointer to Session
 * @return true if sandboxing is active, false otherwise
 */
bool sandbox_is_active(const Session *session);

/**
 * Free all active sandbox stacks (both filesystem and network) for a session.
 *
 * @param session Pointer to Session
 */
void sandbox_free_all(Session *session);

#endif /* SANDBOX_H */
