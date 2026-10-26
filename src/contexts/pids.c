/** pids.c
 *
 * Implementation of the 'pids' (and 'procs', 'nproc') built-in context manager.
 * Prevents fork-bombs by capping the maximum simultaneous processes that the
 * shell can spawn.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/pids.h"
#include "sandbox/limits.h"
#include "session.h"

int context_enter_pids(Session *session, ContextArg *args, size_t argc,
                       void **state) {
    if (!session)
        return 1;

    LimitSandbox *sb = limit_sandbox_create();
    if (!sb)
        return 1;

    int max_p = 10; // default 10 processes

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "max") == 0 || strcmp(key, "limit") == 0 ||
                strcmp(key, "procs") == 0 || strcmp(key, "count") == 0 ||
                strcmp(key, "nproc") == 0) {
                max_p = atoi(val);
            }
        } else {
            // Positional
            if (strncmp(val, "max=", 4) == 0 ||
                strncmp(val, "limit=", 6) == 0) {
                const char *s = strchr(val, '=') + 1;
                max_p         = atoi(s);
            } else {
                max_p = atoi(val);
            }
        }
    }

    if (max_p <= 0) {
        max_p = 1;
    }

    sb->max_processes = max_p;
    limit_sandbox_push(session, sb);

    *state = sb;
    return 0;
}

int context_exit_pids(Session *session, int status, void *state) {
    (void)status;
    if (!session || !state)
        return 0;

    LimitSandbox *sb     = (LimitSandbox *)state;
    LimitSandbox *popped = limit_sandbox_pop(session);
    if (popped) {
        limit_sandbox_free(popped);
    }
    return 0;
}
