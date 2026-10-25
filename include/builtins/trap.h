/** trap.h
 *
 * Implementation of the trap builtin command.
 */

#ifndef BUILTIN_TRAP_H
#define BUILTIN_TRAP_H

#include "session.h" /* Session */

/**
 * Execute the trap builtin command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @param session Pointer to Session
 * @return 0 on success, >0 on failure
 */
int builtin_trap(int argc, char **argv, Session *session);

/**
 * Check and execute any pending signal traps.
 *
 * @param session Pointer to Session
 */
void check_pending_traps(Session *session);

/**
 * Run the EXIT trap if configured.
 *
 * @param session Pointer to Session
 */
void session_run_exit_trap(Session *session);

/**
 * Reset non-ignored traps for a subshell.
 *
 * @param session Pointer to Session
 */
void traps_reset_for_subshell(Session *session);

/**
 * Convert a condition name or number to a signal number (0 for EXIT, -1 for
 * invalid).
 *
 * @param cond Condition string (e.g. "EXIT", "0", "INT", "SIGINT", "2")
 * @return Signal number, 0 for EXIT, or -1 if invalid
 */
int trap_condition_to_signo(const char *cond);

/**
 * Convert a signal number to a condition name (e.g. 0 -> "EXIT", 2 -> "INT").
 *
 * @param signo Signal number
 * @return Signal name string, or NULL if unknown
 */
const char *trap_signo_to_name(int signo);

#endif /* BUILTIN_TRAP_H */
