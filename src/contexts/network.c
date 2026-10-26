/** network.c
 *
 * Implementation of the 'network' built-in context manager.
 * Provides fine-grained network sandboxing including domain allowlists,
 * port allow/deny rules, and airgap modes.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/network.h"
#include "sandbox.h"
#include "session.h"

int context_enter_network(Session *session, ContextArg *args, size_t argc,
                          void **state) {
    if (!session)
        return 1;

    NetSandbox *sb = net_sandbox_create();
    if (!sb)
        return 1;

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "offline") == 0 || strcmp(key, "airgap") == 0 ||
                strcmp(key, "none") == 0) {
                if (val[0] == '\0' || strcmp(val, "true") == 0 ||
                    strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) {
                    sb->offline = true;
                }
            } else if (strcmp(key, "loopback") == 0 || strcmp(key, "lo") == 0 ||
                       strcmp(key, "local") == 0 ||
                       strcmp(key, "localhost") == 0) {
                if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0 ||
                    strcmp(val, "no") == 0) {
                    sb->allow_loopback = false;
                } else {
                    sb->allow_loopback = true;
                }
            } else if (strcmp(key, "domains") == 0 ||
                       strcmp(key, "allow_domains") == 0 ||
                       strcmp(key, "domain") == 0 ||
                       strcmp(key, "allow_domain") == 0) {
                net_sandbox_parse_domains(sb, val);
            } else if (strcmp(key, "ports") == 0 ||
                       strcmp(key, "allow_ports") == 0 ||
                       strcmp(key, "port") == 0 ||
                       strcmp(key, "allow_port") == 0) {
                net_sandbox_parse_ports(sb, val, true);
            } else if (strcmp(key, "deny_ports") == 0 ||
                       strcmp(key, "block_ports") == 0 ||
                       strcmp(key, "deny_port") == 0 ||
                       strcmp(key, "block_port") == 0) {
                net_sandbox_parse_ports(sb, val, false);
            } else if (strcmp(key, "ips") == 0 ||
                       strcmp(key, "allow_ips") == 0 ||
                       strcmp(key, "ip") == 0) {
                char buf[1024];
                strncpy(buf, val, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                char *token          = strtok(buf, ",: ");
                while (token) {
                    net_sandbox_add_ip(sb, token);
                    token = strtok(NULL, ",: ");
                }
            }
        } else {
            // Positional argument
            if (strcmp(val, "offline") == 0 || strcmp(val, "airgap") == 0 ||
                strcmp(val, "none") == 0) {
                sb->offline = true;
            } else if (strncmp(val, "domains=", 8) == 0) {
                net_sandbox_parse_domains(sb, val + 8);
            } else if (strncmp(val, "ports=", 6) == 0) {
                net_sandbox_parse_ports(sb, val + 6, true);
            } else if (strncmp(val, "deny_ports=", 11) == 0) {
                net_sandbox_parse_ports(sb, val + 11, false);
            } else if (strncmp(val, "block_ports=", 12) == 0) {
                net_sandbox_parse_ports(sb, val + 12, false);
            } else if (strcmp(val, "loopback") == 0 ||
                       strcmp(val, "+loopback") == 0) {
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

int context_exit_network(Session *session, int status, void *state) {
    (void)status;
    if (!session || !state)
        return 0;

    NetSandbox *popped = net_sandbox_pop(session);
    if (popped) {
        net_sandbox_free(popped);
    }
    return 0;
}
