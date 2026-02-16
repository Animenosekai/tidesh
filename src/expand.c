#include <stddef.h> /* size_t, NULL */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* strcmp, strlen */

#include "data/array.h" /* init_array, array_extend, free_array, Array */
#include "expand.h"     /* Array */
#include "expansions/aliases.h"   /* alias_expansion */
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
    Array *after_variables = variable_expansion(input, session);
    if (after_variables == NULL) {
        return NULL;
    }

    Array *after_tildes = apply(after_variables, tilde_expansion, session);
    free_array(after_variables);
    free(after_variables);
    if (after_tildes == NULL) {
        return NULL;
    }

    Array *after_braces = apply(after_tildes, brace_expansion, session);
    free_array(after_tildes);
    free(after_tildes);
    if (after_braces == NULL) {
        return NULL;
    }

    Array *after_filenames = apply(after_braces, filename_expansion, session);
    free_array(after_braces);
    free(after_braces);

    return after_filenames;
}
