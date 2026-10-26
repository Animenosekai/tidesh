/** context_manager.h
 *
 * Declarations for the 'context-manager' builtin command.
 */

#ifndef BUILTINS_CONTEXT_MANAGER_H
#define BUILTINS_CONTEXT_MANAGER_H

typedef struct Session Session;

/**
 * Builtin command to register, unregister, and inspect context managers.
 *
 * @param argc Argument count
 * @param argv Argument array
 * @param session Pointer to current Session
 * @return 0 on success, non-zero on error
 */
int builtin_context_manager(int argc, char **argv, Session *session);

#endif /* BUILTINS_CONTEXT_MANAGER_H */
