#include <stdlib.h> /* exit, atoi */

#include "builtins/exit.h"
#include "builtins/trap.h" /* session_run_exit_trap */
#include "environ.h"       /* environ_get_exit_status */
#include "session.h"       /* Session */

int builtin_exit(int argc, char **argv, Session *session) {
    int exit_code = 0;
    if (argc > 1) {
        exit_code = atoi(argv[1]);
    } else if (session && session->environ) {
        exit_code = environ_get_exit_status(session->environ);
    }
    if (session) {
        session_run_exit_trap(session);
        if (argc <= 1 && session->environ) {
            exit_code = environ_get_exit_status(session->environ);
        }
    }
    exit(exit_code);
}
