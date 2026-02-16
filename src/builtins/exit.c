#include <stdlib.h> /* exit, atoi */

#include "builtins/exit.h"
#include "session.h" /* Session */

int builtin_exit(int argc, char **argv, Session *session) {
    int exit_code = 0;
    if (argc > 1) {
        exit_code = atoi(argv[1]);
    }
    exit(exit_code);
}