#include <stdlib.h> /* free, strdup */
#include <string.h> /* strchr */

#include "builtins/export.h"
#include "builtins/printenv.h" /* builtin_printenv */
#include "environ.h"           /* environ_set, environ_get */
#include "session.h"           /* Session */

int builtin_export(int argc, char **argv, Session *session) {
    if (argc == 1) {
        return builtin_printenv(argc, argv, session);
    }

    for (int i = 1; i < argc; i++) {
        char *token = strdup(argv[i]);
        char *eq    = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            environ_set(session->environ, token, eq + 1);
        } else {
            char *val = environ_get(session->environ, token);
            if (val) {
                // If the variable already exists, we just re-set it to mark it
                // as exported
                char *val_copy = strdup(val);
                environ_set(session->environ, token, val_copy);
                free(val_copy);
            }
        }
        free(token);
    }
    return 0;
}
