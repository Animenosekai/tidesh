/** chroot.h
 *
 * Declarations for the 'chroot' (and 'jail') built-in context manager.
 */

#ifndef CONTEXT_CHROOT_H
#define CONTEXT_CHROOT_H

#include <stddef.h> /* size_t */

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter chroot context manager.
 * Jails filesystem access to the specified directory.
 *
 * @param session Pointer to Session
 * @param args Array of ContextArg arguments
 * @param argc Number of arguments
 * @param state Out pointer to saved context state
 * @return 0 on success, non-zero on failure
 */
int context_enter_chroot(Session *session, ContextArg *args, size_t argc,
                         void **state);

/**
 * Exit chroot context manager.
 * Restores previous root and working directory.
 *
 * @param session Pointer to Session
 * @param status Exit status of the with block
 * @param state Saved state from context_enter_chroot
 * @return 0 on success, non-zero on failure
 */
int context_exit_chroot(Session *session, int status, void *state);

#endif /* CONTEXT_CHROOT_H */
