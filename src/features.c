/** features.c
 *
 * Implementation of feature flag management functions.
 */

#include <stddef.h> /* NULL */

#include "features.h" /* Features */

Features *init_features(Features *features) {
    if (features == NULL) {
        return NULL;
    }

    // Enable all expansion features by default
    features->variable_expansion = true;
    features->tilde_expansion    = true;
    features->brace_expansion    = true;
    features->filename_expansion = true;
    features->alias_expansion    = true;

    // Enable all shell features by default
    features->job_control     = true;
    features->history         = true;
    features->directory_stack = true;

    // Enable advanced features
    features->prompt_expansion = true; // Custom prompt display
    features->completion       = true; // Tab completion

    // Enable control flow and redirection features by default
    features->pipes                = true;
    features->redirections         = true;
    features->sequences            = true;
    features->subshells            = true;
    features->command_substitution = true;
    features->assignments          = true;

    features_apply_compile_time_disables(features);

    return features;
}

Features *init_features_minimal(Features *features) {
    if (features == NULL) {
        return NULL;
    }

    // Disable all expansion features for minimal mode
    features->variable_expansion = false;
    features->tilde_expansion    = false;
    features->brace_expansion    = false;
    features->filename_expansion = false;
    features->alias_expansion    = false;

    // Disable shell features
    features->job_control     = false;
    features->history         = false;
    features->directory_stack = false;

    // Disable advanced features
    features->prompt_expansion = false;
    features->completion       = false;

    // Disable control flow and redirection features
    features->pipes                = false;
    features->redirections         = false;
    features->sequences            = false;
    features->subshells            = false;
    features->command_substitution = false;
    features->assignments          = false;

    return features;
}

void features_enable_all_expansions(Features *features) {
    if (features == NULL) {
        return;
    }

    features->variable_expansion = true;
    features->tilde_expansion    = true;
    features->brace_expansion    = true;
    features->filename_expansion = true;
}

void features_disable_all_expansions(Features *features) {
    if (features == NULL) {
        return;
    }

    features->variable_expansion = false;
    features->tilde_expansion    = false;
    features->brace_expansion    = false;
    features->filename_expansion = false;
}

void features_apply_compile_time_disables(Features *features) {
    if (features == NULL) {
        return;
    }

#ifdef TIDESH_DISABLE_EXPANSIONS
    features->variable_expansion = false;
    features->tilde_expansion    = false;
    features->brace_expansion    = false;
    features->filename_expansion = false;
#endif
#ifdef TIDESH_DISABLE_ALIASES
    features->alias_expansion = false;
#endif
#ifdef TIDESH_DISABLE_JOB_CONTROL
    features->job_control = false;
#endif
#ifdef TIDESH_DISABLE_HISTORY
    features->history = false;
#endif
#ifdef TIDESH_DISABLE_DIRSTACK
    features->directory_stack = false;
#endif
#ifdef TIDESH_DISABLE_PIPES
    features->pipes = false;
#endif
#ifdef TIDESH_DISABLE_REDIRECTIONS
    features->redirections = false;
#endif
#ifdef TIDESH_DISABLE_SEQUENCES
    features->sequences = false;
#endif
#ifdef TIDESH_DISABLE_SUBSHELLS
    features->subshells = false;
#endif
#ifdef TIDESH_DISABLE_COMMAND_SUBSTITUTION
    features->command_substitution = false;
#endif
#ifdef TIDESH_DISABLE_ASSIGNMENTS
    features->assignments = false;
#endif
}
