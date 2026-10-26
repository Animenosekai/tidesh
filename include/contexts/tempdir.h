/** tempdir.h
 *
 * Declarations for the 'tempdir' (and 'tmpdir', 'isolated_dir') built-in
 * context manager.
 */

#ifndef CONTEXT_TEMPDIR_H
#define CONTEXT_TEMPDIR_H

#include <stddef.h> /* size_t */

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter tempdir context manager.
 * Creates an isolated temporary directory, exports environment variables,
 * optionally changes into it, and tracks it for automatic cleanup on exit.
 *
 * @param session Pointer to Session
 * @param args Array of ContextArg arguments
 * @param argc Number of arguments
 * @param state Out pointer to saved context state
 * @return 0 on success, non-zero on failure
 */
int context_enter_tempdir(Session *session, ContextArg *args, size_t argc,
                          void **state);

/**
 * Exit tempdir context manager.
 * Restores previous working directory, restores environment variables,
 * and recursively deletes the temporary directory if configured.
 *
 * @param session Pointer to Session
 * @param status Exit status of the with block
 * @param state Saved state from context_enter_tempdir
 * @return 0 on success, non-zero on failure
 */
int context_exit_tempdir(Session *session, int status, void *state);

#endif /* CONTEXT_TEMPDIR_H */
