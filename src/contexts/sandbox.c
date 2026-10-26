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

typedef struct SandboxCombinedState {
    FSSandbox  *fs_sb;
    NetSandbox *net_sb;
} SandboxCombinedState;

int context_enter_sandbox(Session *session, ContextArg *args, size_t argc,
                          void **state) {
    if (!session)
        return 1;

    FSSandbox  *fs_sb  = fs_sandbox_create();
    NetSandbox *net_sb = NULL;
    if (!fs_sb)
        return 1;

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "readonly") == 0 || strcmp(key, "ro") == 0) {
                if (val[0] == '\0' || strcmp(val, "true") == 0 ||
                    strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) {
                    fs_sb->readonly_all = true;
                } else {
                    fs_sandbox_add_paths(fs_sb, val, 0);
                }
            } else if (strcmp(key, "rw") == 0 ||
                       strcmp(key, "readwrite") == 0 ||
                       strcmp(key, "allow_write") == 0) {
                fs_sandbox_add_paths(fs_sb, val, 1);
            } else if (strcmp(key, "deny") == 0 || strcmp(key, "denied") == 0 ||
                       strcmp(key, "block") == 0) {
                fs_sandbox_add_paths(fs_sb, val, 2);
            } else if (strcmp(key, "chroot") == 0 || strcmp(key, "root") == 0) {
                fs_sandbox_set_chroot(fs_sb, val);
            } else if (strcmp(key, "default") == 0) {
                if (strcmp(val, "deny") == 0 || strcmp(val, "readonly") == 0 ||
                    strcmp(val, "ro") == 0) {
                    fs_sb->readonly_all = true;
                }
            } else if (strcmp(key, "allow") == 0) {
                fs_sandbox_add_paths(fs_sb, val, 1);
            } else if (strcmp(key, "offline") == 0 ||
                       strcmp(key, "airgap") == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb) {
                    net_sb->offline =
                        (val[0] == '\0' || strcmp(val, "true") == 0 ||
                         strcmp(val, "1") == 0 || strcmp(val, "yes") == 0);
                    if (strcmp(key, "airgap") == 0) {
                        net_sb->allow_loopback = false;
                    }
                }
            } else if (strcmp(key, "network") == 0) {
                if (strcmp(val, "none") == 0 || strcmp(val, "offline") == 0 ||
                    strcmp(val, "airgap") == 0) {
                    if (!net_sb)
                        net_sb = net_sandbox_create();
                    if (net_sb)
                        net_sb->offline = true;
                }
            } else if (strcmp(key, "domains") == 0 ||
                       strcmp(key, "allow_domains") == 0 ||
                       strcmp(key, "domain") == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb)
                    net_sandbox_parse_domains(net_sb, val);
            } else if (strcmp(key, "ports") == 0 ||
                       strcmp(key, "allow_ports") == 0 ||
                       strcmp(key, "port") == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb)
                    net_sandbox_parse_ports(net_sb, val, true);
            } else if (strcmp(key, "deny_ports") == 0 ||
                       strcmp(key, "block_ports") == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb)
                    net_sandbox_parse_ports(net_sb, val, false);
            } else if (strcmp(key, "loopback") == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb) {
                    net_sb->allow_loopback =
                        (strcmp(val, "false") != 0 && strcmp(val, "0") != 0);
                }
            }
        } else {
            // Positional argument
            if (strncmp(val, "ro=", 3) == 0) {
                const char *sub = val + 3;
                if (strcmp(sub, "true") == 0 || strcmp(sub, "1") == 0) {
                    fs_sb->readonly_all = true;
                } else {
                    fs_sandbox_add_paths(fs_sb, sub, 0);
                }
            } else if (strncmp(val, "rw=", 3) == 0) {
                fs_sandbox_add_paths(fs_sb, val + 3, 1);
            } else if (strncmp(val, "deny=", 5) == 0) {
                fs_sandbox_add_paths(fs_sb, val + 5, 2);
            } else if (strncmp(val, "chroot=", 7) == 0) {
                fs_sandbox_set_chroot(fs_sb, val + 7);
            } else if (strncmp(val, "root=", 5) == 0) {
                fs_sandbox_set_chroot(fs_sb, val + 5);
            } else if (strcmp(val, "readonly") == 0 ||
                       strcmp(val, "default=deny") == 0) {
                fs_sb->readonly_all = true;
            } else if (strcmp(val, "offline") == 0 ||
                       strcmp(val, "airgap") == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb) {
                    net_sb->offline = true;
                    if (strcmp(val, "airgap") == 0)
                        net_sb->allow_loopback = false;
                }
            } else if (strncmp(val, "domains=", 8) == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb)
                    net_sandbox_parse_domains(net_sb, val + 8);
            } else if (strncmp(val, "ports=", 6) == 0) {
                if (!net_sb)
                    net_sb = net_sandbox_create();
                if (net_sb)
                    net_sandbox_parse_ports(net_sb, val + 6, true);
            } else if (val[0] == '+') {
                fs_sandbox_add_paths(fs_sb, val + 1, 1);
            } else if (val[0] == '-') {
                fs_sandbox_add_paths(fs_sb, val + 1, 2);
            } else {
                fs_sandbox_add_paths(fs_sb, val, 0);
            }
        }
    }

    fs_sandbox_push(session, fs_sb);
    if (net_sb) {
        net_sandbox_push(session, net_sb);
    }

    SandboxCombinedState *combo = calloc(1, sizeof(SandboxCombinedState));
    if (!combo) {
        fs_sandbox_pop(session);
        fs_sandbox_free(fs_sb);
        if (net_sb) {
            net_sandbox_pop(session);
            net_sandbox_free(net_sb);
        }
        return 1;
    }
    combo->fs_sb  = fs_sb;
    combo->net_sb = net_sb;

    *state = combo;
    return 0;
}

int context_exit_sandbox(Session *session, int status, void *state) {
    (void)status;
    if (!session || !state)
        return 0;

    SandboxCombinedState *combo = (SandboxCombinedState *)state;
    if (combo->net_sb) {
        NetSandbox *popped_net = net_sandbox_pop(session);
        if (popped_net) {
            net_sandbox_free(popped_net);
        }
    }
    if (combo->fs_sb) {
        FSSandbox *popped_fs = fs_sandbox_pop(session);
        if (popped_fs) {
            fs_sandbox_free(popped_fs);
        }
    }
    free(combo);
    return 0;
}
