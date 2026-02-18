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
