#include <stdio.h>  /* perror */
#include <unistd.h> /* chdir */

#include "builtins/cd.h"
#include "environ.h" /* environ_get_default */
#include "session.h" /* update_working_dir, Session */

int builtin_cd(int argc, char **argv, Session *session) {
    char *dir = NULL;
    if (argc > 1) {
        dir = argv[1];
    } else {
        dir = environ_get_default(session->environ, "HOME", "/");
    }

    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    } else {
        update_working_dir(session);
    }
    return 0;
}
