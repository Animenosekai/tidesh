#include <stdio.h>  /* printf, fprintf */
#include <string.h> /* strcmp */

#include "builtins/terminal.h"
#include "environ.h"         /* environ_get */
#include "prompt/terminal.h" /* Terminal */
#include "session.h"         /* Session */

int builtin_terminal(int argc, char **argv, Session *session) {
    if (argc > 1) {
        if (strcmp(argv[1], "colors") == 0) {
            if (argc > 2) {
                if (strcmp(argv[2], "disable") == 0) {
                    session->terminal->supports_colors = false;
                    return 0;
                } else if (strcmp(argv[2], "enable") == 0) {
                    session->terminal->supports_colors = true;
                    return 0;
                } else {
                    fprintf(stderr, "terminal: unknown colors subcommand: %s\n",
                            argv[2]);
                    fprintf(stderr,
                            "Usage: terminal colors [enable|disable]\n");
                    return 1;
                }
            } else {
                printf("%s\n", session->terminal->supports_colors ? "enabled"
                                                                  : "disabled");
                return 0;
            }
        } else {
            fprintf(stderr, "terminal: unknown subcommand: %s\n", argv[1]);
            fprintf(stderr, "Usage: terminal [colors [enable|disable]]\n");
            return 1;
        }
    }

    char *term_env = environ_get(session->environ, "TERM");
    printf("Name:   %s\n", term_env ? term_env : "unknown");
    printf("Size:   %zu columns, %zu rows\n", session->terminal->cols,
           session->terminal->rows);
    printf("Colors? %s\n",
           session->terminal->supports_colors ? "Supported" : "Not Supported");
    printf("Raw?    %s\n", session->terminal->is_raw ? "Yes" : "No");

    return 0;
}
