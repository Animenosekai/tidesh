#include <stdio.h>  /* printf */
#include <stdlib.h> /* free */

#include "builtins/printenv.h"
#include "environ.h" /* environ_to_array, environ_get, free_array */
#include "session.h" /* Session */

int builtin_printenv(int argc, char **argv, Session *session) {
    if (argc > 1) {
        // printenv can take onée argument to print a specific variable
        char *value = environ_get(session->environ, argv[1]);
        if (value) {
            printf("%s\n", value);
            return 0;
        } else {
            return 1; // variable not found
        }
    }
    Array *environ = environ_to_array(session->environ);
    if (environ) {
        for (size_t i = 0; i < environ->count; i++) {
            printf("%s\n", environ->items[i]);
        }
        free_array(environ);
        free(environ);
    }
    return 0;
}