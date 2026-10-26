/** options.h
 *
 * Declarations for the 'options' built-in context manager.
 */

#ifndef CONTEXTS_OPTIONS_H
#define CONTEXTS_OPTIONS_H

#include <stddef.h>

typedef struct Session    Session;
typedef struct ContextArg ContextArg;

/**
 * Enter handler for the 'options' context manager.
 *
 * @param session Pointer to current session
 * @param args Array of option flags (e.g. "-e", "+x")
 * @param argc Number of arguments
 * @param state Out-parameter to store saved ShellOptions
 * @return 0 on success, non-zero on error
 */
int context_enter_options(Session *session, ContextArg *args, size_t argc,
                          void **state);

/**
 * Exit handler for the 'options' context manager.
 *
 * @param session Pointer to current session
 * @param status Exit status of the enclosed block body
 * @param state Saved ShellOptions from enter handler
 * @return 0 on success, non-zero on error
 */
int context_exit_options(Session *session, int status, void *state);

#endif /* CONTEXTS_OPTIONS_H */
