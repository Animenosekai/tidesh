/** sandbox.c
 *
 * Implementation of the 'sandbox' built-in context manager.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/sandbox.h"
#include "sandbox.h"
#include "session.h"

int context_enter_sandbox(Session *session, ContextArg *args, size_t argc,
                          void **state) {
    if (!session)
        return 1;

    FSSandbox *sb = fs_sandbox_create();
    if (!sb)
        return 1;

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "readonly") == 0 || strcmp(key, "ro") == 0) {
                if (val[0] == '\0' || strcmp(val, "true") == 0 ||
                    strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) {
                    sb->readonly_all = true;
                } else {
                    fs_sandbox_add_paths(sb, val, 0);
                }
            } else if (strcmp(key, "rw") == 0 ||
                       strcmp(key, "readwrite") == 0 ||
                       strcmp(key, "allow_write") == 0) {
                fs_sandbox_add_paths(sb, val, 1);
            } else if (strcmp(key, "deny") == 0 || strcmp(key, "denied") == 0 ||
                       strcmp(key, "block") == 0) {
                fs_sandbox_add_paths(sb, val, 2);
            } else if (strcmp(key, "chroot") == 0 || strcmp(key, "root") == 0) {
                fs_sandbox_set_chroot(sb, val);
            } else if (strcmp(key, "default") == 0) {
                if (strcmp(val, "deny") == 0 || strcmp(val, "readonly") == 0 ||
                    strcmp(val, "ro") == 0) {
                    sb->readonly_all = true;
                }
            } else if (strcmp(key, "allow") == 0) {
                fs_sandbox_add_paths(sb, val, 1);
            }
        } else {
            // Positional argument
            if (strncmp(val, "ro=", 3) == 0) {
                const char *sub = val + 3;
                if (strcmp(sub, "true") == 0 || strcmp(sub, "1") == 0) {
                    sb->readonly_all = true;
                } else {
                    fs_sandbox_add_paths(sb, sub, 0);
                }
            } else if (strncmp(val, "rw=", 3) == 0) {
                fs_sandbox_add_paths(sb, val + 3, 1);
            } else if (strncmp(val, "deny=", 5) == 0) {
                fs_sandbox_add_paths(sb, val + 5, 2);
            } else if (strncmp(val, "chroot=", 7) == 0) {
                fs_sandbox_set_chroot(sb, val + 7);
            } else if (strncmp(val, "root=", 5) == 0) {
                fs_sandbox_set_chroot(sb, val + 5);
            } else if (strcmp(val, "readonly") == 0 ||
                       strcmp(val, "default=deny") == 0) {
                sb->readonly_all = true;
            } else if (val[0] == '+') {
                fs_sandbox_add_paths(sb, val + 1, 1);
            } else if (val[0] == '-') {
                fs_sandbox_add_paths(sb, val + 1, 2);
            } else {
                fs_sandbox_add_paths(sb, val, 0);
            }
        }
    }

    fs_sandbox_push(session, sb);
    *state = sb;
    return 0;
}

int context_exit_sandbox(Session *session, int status, void *state) {
    (void)status;
    if (!session || !state)
        return 0;

    FSSandbox *popped = fs_sandbox_pop(session);
    if (popped) {
        fs_sandbox_free(popped);
    }
    return 0;
}
