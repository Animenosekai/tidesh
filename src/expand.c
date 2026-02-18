#include <stddef.h> /* size_t, NULL */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* strcmp, strlen */

#include "data/array.h" /* init_array, array_extend, free_array, Array */
#include "expand.h"     /* Array */
#ifndef TIDESH_DISABLE_ALIASES
#include "expansions/aliases.h" /* alias_expansion */
#endif
#include "expansions/braces.h"    /* brace_expansion */
#include "expansions/filenames.h" /* filename_expansion */
#include "expansions/tildes.h"    /* tilde_expansion */
#include "expansions/variables.h" /* variable_expansion */
#include "session.h"              /* Session */

/* Helper to apply an expansion function to all items in an Array */
static Array *apply(Array *inputs, Array *(*expansion_func)(char *, Session *),
                    Session *session) {
    Array *results = init_array(NULL);

    for (size_t i = 0; i < inputs->count; i++) {
        Array *expanded = expansion_func(inputs->items[i], session);
        if (expanded == NULL) {
            free_array(results);
            free(results);
            return NULL;
        }
        array_extend(results, expanded);
        free_array(expanded);
        free(expanded);
    }

    return results;
}

Array *full_expansion(char *input, Session *session) {
    // Variable expansion (always first)
    Array *after_variables = NULL;
    if (session->features.variable_expansion) {
        after_variables = variable_expansion(input, session);
        if (after_variables == NULL) {
            return NULL;
        }
    } else {
        // Skip variable expansion, create single-item array
        after_variables = init_array(NULL);
        array_add(after_variables, input);
    }

    // Tilde expansion
    Array *after_tildes = NULL;
    if (session->features.tilde_expansion) {
        after_tildes = apply(after_variables, tilde_expansion, session);
        free_array(after_variables);
        free(after_variables);
        if (after_tildes == NULL) {
            return NULL;
        }
    } else {
        after_tildes = after_variables;
    }

    // Brace expansion
    Array *after_braces = NULL;
    if (session->features.brace_expansion) {
        after_braces = apply(after_tildes, brace_expansion, session);
        free_array(after_tildes);
        free(after_tildes);
        if (after_braces == NULL) {
            return NULL;
        }
    } else {
        after_braces = after_tildes;
    }

    // Filename expansion (globbing)
    Array *after_filenames = NULL;
    if (session->features.filename_expansion) {
        after_filenames = apply(after_braces, filename_expansion, session);
        free_array(after_braces);
        free(after_braces);
    } else {
        after_filenames = after_braces;
    }

    return after_filenames;
}
