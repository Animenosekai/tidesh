/** airgap.c
 *
 * Implementation of the 'airgap' built-in context manager.
 * Completely disables networking (full air-gapping), with optional
 * loopback support for local testing.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/airgap.h"
#include "sandbox.h"
#include "session.h"

int context_enter_airgap(Session *session, ContextArg *args, size_t argc,
                         void **state) {
    if (!session)
        return 1;

    NetSandbox *sb = net_sandbox_create();
    if (!sb)
        return 1;

    sb->offline        = true;
    sb->allow_loopback = false; // Default to zero network including loopback

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "loopback") == 0 || strcmp(key, "lo") == 0 ||
                strcmp(key, "local") == 0 || strcmp(key, "localhost") == 0) {
                if (val[0] == '\0' || strcmp(val, "true") == 0 ||
                    strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) {
                    sb->allow_loopback = true;
                } else if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0 ||
                           strcmp(val, "no") == 0) {
                    sb->allow_loopback = false;
                }
            }
        } else {
            // Positional argument
            if (strcmp(val, "loopback") == 0 || strcmp(val, "+loopback") == 0 ||
                strcmp(val, "local") == 0 || strcmp(val, "localhost") == 0) {
                sb->allow_loopback = true;
            } else if (strcmp(val, "no-loopback") == 0 ||
                       strcmp(val, "-loopback") == 0) {
                sb->allow_loopback = false;
            }
        }
    }

    net_sandbox_push(session, sb);
    *state = sb;
    return 0;
}

int context_exit_airgap(Session *session, int status, void *state) {
    (void)status;
    if (!session || !state)
        return 0;

    NetSandbox *popped = net_sandbox_pop(session);
    if (popped) {
        net_sandbox_free(popped);
    }
    return 0;
}
