/** features.h
 *
 * Feature flags for conditional compilation and runtime configuration.
 * This module allows users to enable/disable shell features for improved
 * performance when certain features are not needed.
 */

#ifndef FEATURES_H
#define FEATURES_H

#include <stdbool.h> /* bool */

/**
 * Compile-time feature disable flags.
 * Define at compile time (e.g., -DTIDESH_DISABLE_PIPES) to disable features:
 *
 * Existing compile-time flags:
 *  - TIDESH_DISABLE_JOB_CONTROL: Disable background jobs, fg, bg, jobs
 *  - TIDESH_DISABLE_HISTORY: Disable command history
 *  - TIDESH_DISABLE_ALIASES: Disable alias/unalias
 *  - TIDESH_DISABLE_DIRSTACK: Disable pushd, popd, dirs
 *  - TIDESH_DISABLE_EXPANSIONS: Disable all expansions
 *
 * Control Flow & Redirection compile-time flags:
 *  - TIDESH_DISABLE_PIPES: Disable pipe operator |
 *  - TIDESH_DISABLE_REDIRECTIONS: Disable redirections >, <, >>, etc.
 *  - TIDESH_DISABLE_SEQUENCES: Disable ; && || operators
 *  - TIDESH_DISABLE_SUBSHELLS: Disable subshells ( ... )
 *  - TIDESH_DISABLE_COMMAND_SUBSTITUTION: Disable $(...)
 *  - TIDESH_DISABLE_ASSIGNMENTS: Disable VAR=value assignments
 */

/**
 * Runtime feature flags stored in each session.
 * Users can disable features per-session for improved performance.
 */
typedef struct Features {
    // Expansion features
    bool variable_expansion; // $VAR, ${VAR}, etc.
    bool tilde_expansion;    // ~, ~/path, ~user/path
    bool brace_expansion;    // {a,b,c}, {1..10}
    bool filename_expansion; // *, ?, [...] globbing
    bool alias_expansion;    // Alias substitution

    // Shell features
    bool job_control;     // Background jobs, fg, bg, jobs
    bool history;         // Command history
    bool directory_stack; // pushd, popd, dirs

    // Advanced features
    bool prompt_expansion; // Prompt customization (future)
    bool completion;       // Tab completion (future)

    // Control flow and redirection features
    bool pipes;                // Pipe operator |
    bool redirections;         // Input/output redirection >, <, >>, etc.
    bool sequences;            // Command sequences ;, &&, ||
    bool subshells;            // Subshells ( ... )
    bool command_substitution; // Command substitution $(...) and <(...)
    bool assignments;          // Variable assignments VAR=VAL in commands
} Features;

/**
 * Initialize a Features struct with default values.
 * All features enabled by default.
 *
 * @param features Pointer to Features struct to initialize
 * @return Pointer to initialized Features struct
 */
Features *init_features(Features *features);

/**
 * Create a Features struct with all features disabled.
 * Useful for minimal/fast mode.
 *
 * @param features Pointer to Features struct to initialize
 * @return Pointer to initialized Features struct
 */
Features *init_features_minimal(Features *features);

/**
 * Enable all expansion features.
 *
 * @param features Pointer to Features struct to modify
 */
void features_enable_all_expansions(Features *features);

/**
 * Disable all expansion features.
 *
 * @param features Pointer to Features struct to modify
 */
void features_disable_all_expansions(Features *features);

/**
 * Apply compile-time feature disables to a Features struct.
 *
 * @param features Pointer to Features struct to modify
 */
void features_apply_compile_time_disables(Features *features);

#endif /* FEATURES_H */
