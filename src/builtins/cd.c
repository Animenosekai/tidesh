#include <stdio.h>  /* perror */
#include <unistd.h> /* chdir */

#include "builtins/cd.h"
#include "environ.h" /* environ_get_default */
#include "session.h" /* Session, update_working_dir */

#ifndef TIDESH_DISABLE_DIRSTACK

int builtin_cd(int argc, char **argv, Session *session) {
    if (!session->features.directory_stack) {
        fprintf(stderr, "tidesh: directory stack not enabled\n");
        return 127;
    }
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

#endif
