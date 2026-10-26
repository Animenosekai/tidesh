/** cwd.c
 *
 * Implementation of the 'cwd' (and 'cd') built-in context manager.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "context.h"
#include "contexts/cwd.h"
#include "environ.h"
#include "session.h"

int context_enter_cwd(Session *session, ContextArg *args, size_t argc,
                      void **state) {
    char *target = NULL;
    if (argc > 0 && args[0].value && args[0].value[0] != '\0') {
        target = args[0].value;
    } else {
        target = environ_get(session->environ, "HOME");
    }

    if (!target) {
        fprintf(
            stderr,
            "tidesh: cwd: target directory not specified and HOME not set\n");
        return 1;
    }

    char *saved_pwd = session->current_working_dir
                          ? strdup(session->current_working_dir)
                          : NULL;
    if (!saved_pwd) {
        char buf[4096];
        if (getcwd(buf, sizeof(buf))) {
            saved_pwd = strdup(buf);
        }
    }

    if (chdir(target) != 0) {
        fprintf(stderr, "tidesh: cwd: %s: %s\n", target, strerror(errno));
        free(saved_pwd);
        return 1;
    }

    update_working_dir(session);
    *state = saved_pwd;
    return 0;
}

int context_exit_cwd(Session *session, int status, void *state) {
    (void)status;
    char *saved_pwd = (char *)state;
    if (saved_pwd) {
        if (chdir(saved_pwd) != 0) {
            fprintf(stderr, "tidesh: cwd: failed to restore %s: %s\n",
                    saved_pwd, strerror(errno));
        }
        update_working_dir(session);
        free(saved_pwd);
    }
    return 0;
}
