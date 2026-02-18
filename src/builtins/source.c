#include <stdio.h>  /* fprintf, fopen, fclose, FILE */
#include <stdlib.h> /* free */

#include "builtins/source.h" /* builtin_source */
#include "data/array.h"      /* Array, free_array, array_pop */
#include "data/files.h"      /* read_all */
#include "execute.h"         /* execute_string */
#include "expand.h"          /* full_expansion */
#include "session.h"         /* Session */

int builtin_source(int argc, char **argv, Session *session) {
    if (argc < 2) {
        fprintf(stderr, "source: missing filename argument\n");
        return 1;
    }

    // Expand the filename (handle variables, tilde, etc.)
    char  *filename = argv[1];
    Array *expanded = full_expansion(filename, session);
    if (!expanded || expanded->count == 0) {
        fprintf(stderr, "source: could not expand filename: %s\n", filename);
        if (expanded) {
            free_array(expanded);
            free(expanded);
        }
        return 1;
    }

    char *expanded_filename = array_pop(expanded, 0);
    free_array(expanded);
    free(expanded);

    // Open the file
    FILE *f = fopen(expanded_filename, "r");
    if (!f) {
        fprintf(stderr, "source: could not open file: %s\n", expanded_filename);
        free(expanded_filename);
        return 1;
    }

    // Read the entire file content
    char *content = read_all(f);
    fclose(f);

    if (!content) {
        fprintf(stderr, "source: could not read file: %s\n", expanded_filename);
        free(expanded_filename);
        return 1;
    }

    // Temporarily disable history for sourced commands
#ifndef TIDESH_DISABLE_HISTORY
    bool was_disabled          = session->history->disabled;
    session->history->disabled = true;
#endif

    // Execute the content
    int status = execute_string(content, session);

    // Restore history setting
#ifndef TIDESH_DISABLE_HISTORY
    session->history->disabled = was_disabled;
#endif

    free(content);
    free(expanded_filename);
    return status;
}
