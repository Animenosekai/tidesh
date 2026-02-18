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
 * Comment out or define at compile time to disable features:
 *  - TIDESH_DISABLE_JOB_CONTROL: Disable background jobs, fg, bg, jobs
 *  - TIDESH_DISABLE_HISTORY: Disable command history
 *  - TIDESH_DISABLE_ALIASES: Disable alias/unalias
 *  - TIDESH_DISABLE_DIRSTACK: Disable pushd, popd, dirs
 *  - TIDESH_DISABLE_EXPANSIONS: Disable all expansions
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

#endif /* FEATURES_H */
