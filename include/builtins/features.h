/** builtins/features.h
 *
 * Feature flag management builtin command.
 */

#ifndef BUILTINS_FEATURES_H
#define BUILTINS_FEATURES_H

typedef struct Session Session;

/**
 * builtin_features - Manage runtime feature flags.
 *
 * Usage:
 *   features                       - List all features and their status
 *   features list                  - List all features and their status
 *   features status [name]         - Show status for a feature or all
 *   features enable <name|all>     - Enable a feature or all features
 *   features disable <name|all>    - Disable a feature or all features
 *   features enable expansions     - Enable variable/tilde/brace/filename
 *   features disable expansions    - Disable variable/tilde/brace/filename
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param session Session context
 * @return Exit status (0 on success, non-zero on error)
 */
int builtin_features(int argc, char **argv, Session *session);

#endif /* BUILTINS_FEATURES_H */
