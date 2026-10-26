/** limits.c
 *
 * Implementation of the 'limits' (and 'rlimit', 'limit') built-in context
 * manager. Controls CPU time, CPU cores, maximum memory, process counts,
 * and execution timeouts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/limits.h"
#include "sandbox/limits.h"
#include "session.h"

int context_enter_limits(Session *session, ContextArg *args, size_t argc,
                         void **state) {
    if (!session)
        return 1;

    LimitSandbox *sb = limit_sandbox_create();
    if (!sb)
        return 1;

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "memory") == 0 || strcmp(key, "mem") == 0 ||
                strcmp(key, "max_memory") == 0 || strcmp(key, "ram") == 0) {
                sb->max_memory_bytes = limit_sandbox_parse_bytes(val);
            } else if (strcmp(key, "cpu") == 0 || strcmp(key, "max_cpu") == 0 ||
                       strcmp(key, "cpu_time") == 0) {
                sb->max_cpu_seconds =
                    (uint64_t)limit_sandbox_parse_duration(val);
            } else if (strcmp(key, "cores") == 0 ||
                       strcmp(key, "cpu_cores") == 0) {
                sb->max_cpu_cores = atoi(val);
            } else if (strcmp(key, "procs") == 0 ||
                       strcmp(key, "processes") == 0 ||
                       strcmp(key, "max_procs") == 0 ||
                       strcmp(key, "max_processes") == 0 ||
                       strcmp(key, "nproc") == 0) {
                sb->max_processes = atoi(val);
            } else if (strcmp(key, "timeout") == 0 ||
                       strcmp(key, "duration") == 0 ||
                       strcmp(key, "time") == 0) {
                sb->timeout_seconds = limit_sandbox_parse_duration(val);
            } else if (strcmp(key, "signal") == 0 || strcmp(key, "sig") == 0) {
                sb->timeout_signal = limit_sandbox_parse_signal(val);
            }
        } else {
            // Positional argument
            if (strncmp(val, "memory=", 7) == 0 ||
                strncmp(val, "mem=", 4) == 0) {
                const char *s        = strchr(val, '=') + 1;
                sb->max_memory_bytes = limit_sandbox_parse_bytes(s);
            } else if (strncmp(val, "cpu=", 4) == 0) {
                const char *s       = strchr(val, '=') + 1;
                sb->max_cpu_seconds = (uint64_t)limit_sandbox_parse_duration(s);
            } else if (strncmp(val, "cores=", 6) == 0) {
                const char *s     = strchr(val, '=') + 1;
                sb->max_cpu_cores = atoi(s);
            } else if (strncmp(val, "procs=", 6) == 0 ||
                       strncmp(val, "max_procs=", 10) == 0 ||
                       strncmp(val, "nproc=", 6) == 0) {
                const char *s     = strchr(val, '=') + 1;
                sb->max_processes = atoi(s);
            } else if (strncmp(val, "timeout=", 8) == 0) {
                const char *s       = strchr(val, '=') + 1;
                sb->timeout_seconds = limit_sandbox_parse_duration(s);
            } else if (strncmp(val, "signal=", 7) == 0) {
                const char *s      = strchr(val, '=') + 1;
                sb->timeout_signal = limit_sandbox_parse_signal(s);
            }
        }
    }

    limit_sandbox_push(session, sb);
    if (sb->timeout_seconds > 0) {
        limit_sandbox_start_timer(session, sb);
    }

    *state = sb;
    return 0;
}

int context_exit_limits(Session *session, int status, void *state) {
    (void)status;
    if (!session || !state)
        return 0;

    LimitSandbox *sb = (LimitSandbox *)state;
    if (sb->timed_out) {
        session->exit_requested = false;
    }
    if (sb->timeout_seconds > 0) {
        limit_sandbox_stop_timer(session, sb);
    }

    LimitSandbox *popped = limit_sandbox_pop(session);
    if (popped) {
        limit_sandbox_free(popped);
    }
    return 0;
}
