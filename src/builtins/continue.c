/** continue.c
 *
 * Builtin command to resume the next iteration of enclosing loop.
 */

#include <stdio.h>
#include <stdlib.h>

#include "builtins/continue.h"
#include "session.h"

int builtin_continue(int argc, char **argv, Session *session) {
    int count = 1;
    if (argc > 1) {
        char *endptr = NULL;
        long  val    = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0' || val <= 0) {
            fprintf(stderr, "tidesh: continue: %s: numeric argument required\n",
                    argv[1]);
            return 1;
        }
        count = (int)val;
    }

    if (!session || session->loop_depth <= 0) {
        return 0;
    }

    session->loop_continue = count;
    return 0;
}
