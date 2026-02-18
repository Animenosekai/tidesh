/** builtins/hooks.h
 *
 * Hook management builtin command
 */

#ifndef BUILTINS_HOOKS_H
#define BUILTINS_HOOKS_H

typedef struct Session Session;

/**
 * builtin_hooks - Manage hooks
 *
 * Usage:
 *   hooks                  - List available hook files in current directory
 *   hooks list             - List available hook files in current directory
 *   hooks enable           - Enable hook execution
 *   hooks disable          - Disable hook execution
 *   hooks status           - Show hook execution status
 *   hooks run <hook_name>  - Manually run a specific hook
 *   hooks path             - Show the hooks directory path (.tidesh-hooks)
 *   hooks types            - List all available hook types
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param session Session context
 * @return Exit status (0 on success, non-zero on error)
 */
int builtin_hooks(int argc, char **argv, Session *session);

#endif /* BUILTINS_HOOKS_H */
