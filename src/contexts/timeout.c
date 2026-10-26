/** timeout.c
 *
 * Implementation of the 'timeout' built-in context manager.
 * Limits the total execution time of commands and blocks, delivering a signal
 * (SIGKILL by default) and terminating commands upon expiration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/timeout.h"
#include "sandbox/limits.h"
#include "session.h"

int context_enter_timeout(Session *session, ContextArg *args, size_t argc,
                          void **state) {
    if (!session)
        return 1;

    LimitSandbox *sb = limit_sandbox_create();
    if (!sb)
        return 1;

    double timeout_secs = 0.0;
    int    sig          = SIGKILL;

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "seconds") == 0 || strcmp(key, "sec") == 0 ||
                strcmp(key, "duration") == 0 || strcmp(key, "time") == 0 ||
                strcmp(key, "timeout") == 0) {
                timeout_secs = limit_sandbox_parse_duration(val);
            } else if (strcmp(key, "signal") == 0 || strcmp(key, "sig") == 0) {
                sig = limit_sandbox_parse_signal(val);
            }
        } else {
            // Positional argument
            if (strncmp(val, "signal=", 7) == 0 ||
                strncmp(val, "sig=", 4) == 0) {
                const char *s = strchr(val, '=') + 1;
                sig           = limit_sandbox_parse_signal(s);
            } else if (strncmp(val, "duration=", 9) == 0 ||
                       strncmp(val, "seconds=", 8) == 0 ||
                       strncmp(val, "sec=", 4) == 0) {
                const char *s = strchr(val, '=') + 1;
                timeout_secs  = limit_sandbox_parse_duration(s);
            } else {
                timeout_secs = limit_sandbox_parse_duration(val);
            }
        }
    }

    if (timeout_secs <= 0.0) {
        // Fallback default: 30 seconds if unspecified
        timeout_secs = 30.0;
    }

    sb->timeout_seconds = timeout_secs;
    sb->timeout_signal  = sig;

    limit_sandbox_push(session, sb);
    limit_sandbox_start_timer(session, sb);

    *state = sb;
    return 0;
}

int context_exit_timeout(Session *session, int status, void *state) {
    (void)status;
    if (!session || !state)
        return 0;

    LimitSandbox *sb = (LimitSandbox *)state;
    if (sb->timed_out) {
        session->exit_requested = false;
    }
    limit_sandbox_stop_timer(session, sb);

    LimitSandbox *popped = limit_sandbox_pop(session);
    if (popped) {
        limit_sandbox_free(popped);
    }
    return 0;
}
