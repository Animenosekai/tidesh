/** return.c
 *
 * Builtin command to return from a shell function or sourced script.
 */

#include <stdio.h>
#include <stdlib.h>

#include "builtins/return.h"
#include "environ.h"
#include "session.h"

int builtin_return(int argc, char **argv, Session *session) {
    if (!session || session->function_depth <= 0) {
        fprintf(stderr,
                "tidesh: return: can only `return' from a function or sourced "
                "script\n");
        return 1;
    }

    int status = 0;
    if (argc > 1) {
        if (argc > 2) {
            fprintf(stderr, "tidesh: return: too many arguments\n");
            session->return_requested = true;
            session->return_status    = 1;
            return 1;
        }

        char *endptr = NULL;
        long  val    = strtol(argv[1], &endptr, 10);
        if (argv[1][0] == '\0' || *endptr != '\0') {
            fprintf(stderr, "tidesh: return: %s: numeric argument required\n",
                    argv[1]);
            session->return_requested = true;
            session->return_status    = 2;
            return 2;
        }
        status = (int)(val & 0xFF);
    } else {
        status = environ_get_exit_status(session->environ);
    }

    session->return_requested = true;
    session->return_status    = status;
    return status;
}
