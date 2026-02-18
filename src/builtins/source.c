#include <stdio.h>
#include <stdlib.h>

#include "builtins/source.h"
#include "data/array.h"
#include "data/files.h"
#include "execute.h"
#include "expand.h"
#include "session.h"

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
    bool was_disabled          = session->history->disabled;
    session->history->disabled = true;

    // Execute the content
    int status = execute_string(content, session);

    // Restore history setting
    session->history->disabled = was_disabled;

    free(content);
    free(expanded_filename);
    return status;
}
