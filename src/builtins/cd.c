#include <stdio.h>  /* perror */
#include <unistd.h> /* chdir */

#include "builtins/cd.h"
#include "environ.h" /* environ_get_default */
#include "sandbox.h"
#include "session.h" /* Session, update_working_dir */

#ifndef TIDESH_DISABLE_DIRSTACK

int builtin_cd(int argc, char **argv, Session *session) {
    char *dir = NULL;
    if (argc > 1) {
        dir = argv[1];
    } else {
        dir =
            environ_get_default(session ? session->environ : NULL, "HOME", "/");
    }

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    if (!fs_sandbox_check_access(session, dir, FS_SANDBOX_TRAVERSE)) {
        fprintf(stderr, "cd: %s: Permission denied by sandbox\n", dir);
        return 1;
    }
#endif

    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    } else {
        if (session) {
            update_working_dir(session);
        }
    }
    return 0;
}

#endif
