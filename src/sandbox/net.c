/** net.c
 *
 * Implementation of network isolation and sandboxing in tidesh.
 * Provides fine-grained network access controls including domain allowlists,
 * port allow/deny rules, loopback controls, full air-gapping, and an embedded
 * filtering micro-proxy for system utilities (such as curl).
 */

#if defined(__APPLE__)
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#endif
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__linux__)
#include <sched.h>
#endif

#include "context.h"
#include "sandbox.h"
#include "session.h"

#if defined(__APPLE__)
extern int  sandbox_init(const char *profile, uint64_t flags, char **errorbuf);
extern void sandbox_free_error(char *errorbuf);
#ifndef SANDBOX_NAMED
#define SANDBOX_NAMED 0x0001
#endif

static int safe_sandbox_init(const char *profile) {
    char *err          = NULL;
    int   saved_stderr = dup(STDERR_FILENO);
    int   dev_null     = open("/dev/null", O_WRONLY);
    if (dev_null >= 0) {
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
    }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    int ret = sandbox_init(profile, 0, &err);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    if (err) {
        sandbox_free_error(err);
    }
    return ret;
}
#endif

/* -------------------------------------------------------------------------
 * Embedded Micro-Proxy Server for Domain & Port Filtering
 * ------------------------------------------------------------------------- */

typedef struct {
    int         client_fd;
    NetSandbox *sb;
} NetProxyClientArg;

static bool net_proxy_is_allowed(NetSandbox *sb, const char *host, int port) {
    if (!sb || !host)
        return false;

    bool is_lo = (strcasecmp(host, "localhost") == 0 ||
                  strcmp(host, "127.0.0.1") == 0 || strcmp(host, "::1") == 0);

    // Loopback restriction
    if (is_lo) {
        if (!sb->allow_loopback)
            return false;
        return true;
    }

    // Offline / airgap
    if (sb->offline)
        return false;

    // Denied ports
    if (sb->denied_ports) {
        for (NetSandboxPort *p = sb->denied_ports; p; p = p->next) {
            if (p->port == port)
                return false;
        }
    }

    // Allowed ports
    if (sb->allowed_ports) {
        bool port_matched = false;
        for (NetSandboxPort *p = sb->allowed_ports; p; p = p->next) {
            if (p->port == port) {
                port_matched = true;
                break;
            }
        }
        if (!port_matched)
            return false;
    }

    // Allowed domains
    if (sb->allowed_domains) {
        bool domain_matched = false;
        for (NetSandboxDomain *d = sb->allowed_domains; d; d = d->next) {
            const char *allowed = d->domain;
            if (strcasecmp(host, allowed) == 0) {
                domain_matched = true;
                break;
            }
            const char *pat = allowed;
            if (pat[0] == '*' && pat[1] == '.')
                pat += 2;
            size_t pat_len  = strlen(pat);
            size_t host_len = strlen(host);
            if (host_len > pat_len && host[host_len - pat_len - 1] == '.' &&
                strcasecmp(host + (host_len - pat_len), pat) == 0) {
                domain_matched = true;
                break;
            }
        }
        if (!domain_matched)
            return false;
    }

    return true;
}

static void net_proxy_relay(int fd1, int fd2) {
    char          buf[16384];
    struct pollfd pfds[2];
    pfds[0].fd     = fd1;
    pfds[0].events = POLLIN;
    pfds[1].fd     = fd2;
    pfds[1].events = POLLIN;

    while (1) {
        int ret = poll(pfds, 2, 30000); // 30s timeout
        if (ret <= 0)
            break;

        if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t n = read(fd1, buf, sizeof(buf));
            if (n <= 0)
                break;
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(fd2, buf + written, (size_t)(n - written));
                if (w <= 0)
                    goto done;
                written += w;
            }
        }

        if (pfds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t n = read(fd2, buf, sizeof(buf));
            if (n <= 0)
                break;
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(fd1, buf + written, (size_t)(n - written));
                if (w <= 0)
                    goto done;
                written += w;
            }
        }
    }
done:;
}

static void *net_proxy_client_worker(void *arg) {
    NetProxyClientArg *pca       = (NetProxyClientArg *)arg;
    int                client_fd = pca->client_fd;
    NetSandbox        *sb        = pca->sb;
    free(pca);

    char   req_buf[8192];
    size_t req_len = 0;

    while (req_len < sizeof(req_buf) - 1) {
        ssize_t n =
            read(client_fd, req_buf + req_len, sizeof(req_buf) - 1 - req_len);
        if (n <= 0)
            break;
        req_len += (size_t)n;
        req_buf[req_len] = '\0';
        if (strstr(req_buf, "\r\n\r\n") || strstr(req_buf, "\n\n"))
            break;
    }

    if (req_len == 0) {
        close(client_fd);
        return NULL;
    }

    // Parse request line: METHOD TARGET PROTOCOL
    char method[16]  = {0};
    char target[512] = {0};
    char proto[16]   = {0};
    if (sscanf(req_buf, "%15s %511s %15s", method, target, proto) < 2) {
        close(client_fd);
        return NULL;
    }

    char host[256] = {0};
    int  port      = 80;

    if (strcasecmp(method, "CONNECT") == 0) {
        port        = 443;
        char *colon = strrchr(target, ':');
        if (colon) {
            size_t hlen = (size_t)(colon - target);
            if (hlen < sizeof(host)) {
                strncpy(host, target, hlen);
                host[hlen] = '\0';
            }
            port = atoi(colon + 1);
        } else {
            strncpy(host, target, sizeof(host) - 1);
        }
    } else {
        // Plain HTTP: target might be "http://host:port/path" or "/path"
        if (strncmp(target, "http://", 7) == 0) {
            const char *p             = target + 7;
            const char *slash         = strchr(p, '/');
            char        hostport[256] = {0};
            if (slash) {
                size_t len = (size_t)(slash - p);
                if (len < sizeof(hostport))
                    strncpy(hostport, p, len);
            } else {
                strncpy(hostport, p, sizeof(hostport) - 1);
            }
            char *colon = strrchr(hostport, ':');
            if (colon) {
                size_t hlen = (size_t)(colon - hostport);
                if (hlen < sizeof(host)) {
                    strncpy(host, hostport, hlen);
                    host[hlen] = '\0';
                }
                port = atoi(colon + 1);
            } else {
                strncpy(host, hostport, sizeof(host) - 1);
                port = 80;
            }
        } else {
            // Check Host: header in req_buf
            char *host_hdr = strcasestr(req_buf, "\nHost:");
            if (!host_hdr)
                host_hdr = strcasestr(req_buf, "\rHost:");
            if (host_hdr) {
                host_hdr += 6;
                while (*host_hdr == ' ')
                    host_hdr++;
                char   hostport[256] = {0};
                size_t i             = 0;
                while (host_hdr[i] && host_hdr[i] != '\r' &&
                       host_hdr[i] != '\n' && i < sizeof(hostport) - 1) {
                    hostport[i] = host_hdr[i];
                    i++;
                }
                hostport[i] = '\0';
                char *colon = strrchr(hostport, ':');
                if (colon) {
                    size_t hlen = (size_t)(colon - hostport);
                    if (hlen < sizeof(host)) {
                        strncpy(host, hostport, hlen);
                        host[hlen] = '\0';
                    }
                    port = atoi(colon + 1);
                } else {
                    strncpy(host, hostport, sizeof(host) - 1);
                    port = 80;
                }
            }
        }
    }

    if (!net_proxy_is_allowed(sb, host, port)) {
        const char *resp =
            "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
        write(client_fd, resp, strlen(resp));
        close(client_fd);
        return NULL;
    }

    // Connect to target
    struct addrinfo  hints = {0};
    struct addrinfo *res   = NULL;
    hints.ai_family        = AF_UNSPEC;
    hints.ai_socktype      = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int target_fd = -1;
    if (getaddrinfo(host, port_str, &hints, &res) == 0 && res) {
        for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
            target_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (target_fd >= 0) {
                if (connect(target_fd, ai->ai_addr, ai->ai_addrlen) == 0) {
                    break;
                }
                close(target_fd);
                target_fd = -1;
            }
        }
        freeaddrinfo(res);
    }

    if (target_fd < 0) {
        const char *resp =
            "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
        write(client_fd, resp, strlen(resp));
        close(client_fd);
        return NULL;
    }

    if (strcasecmp(method, "CONNECT") == 0) {
        const char *resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
        write(client_fd, resp, strlen(resp));
    } else {
        // Forward initial request to target
        write(target_fd, req_buf, req_len);
    }

    net_proxy_relay(client_fd, target_fd);
    close(target_fd);
    close(client_fd);
    return NULL;
}

static void *net_proxy_server_thread(void *arg) {
    NetSandbox *sb        = (NetSandbox *)arg;
    int         listen_fd = sb->proxy_listen_fd;

    while (sb->proxy_running) {
        struct pollfd pfd = {.fd = listen_fd, .events = POLLIN};
        int           ret = poll(&pfd, 1, 200);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ret == 0)
            continue;

        if (pfd.revents & POLLIN) {
            int client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd < 0) {
                if (errno == EINTR)
                    continue;
                break;
            }

            struct timeval tv = {.tv_sec = 30, .tv_usec = 0};
            setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            NetProxyClientArg *pca = malloc(sizeof(NetProxyClientArg));
            if (!pca) {
                close(client_fd);
                continue;
            }
            pca->client_fd = client_fd;
            pca->sb        = sb;

            pthread_t t;
            if (pthread_create(&t, NULL, net_proxy_client_worker, pca) == 0) {
                pthread_detach(t);
            } else {
                free(pca);
                close(client_fd);
            }
        }
    }
    return NULL;
}

int net_sandbox_start_proxy(NetSandbox *sb) {
    if (!sb || sb->proxy_running)
        return 0;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        return -1;

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port        = 0;

    if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 32) < 0) {
        close(listen_fd);
        return -1;
    }

    socklen_t len = sizeof(sin);
    if (getsockname(listen_fd, (struct sockaddr *)&sin, &len) < 0) {
        close(listen_fd);
        return -1;
    }

    sb->proxy_listen_fd = listen_fd;
    sb->proxy_port      = ntohs(sin.sin_port);
    sb->proxy_running   = true;

    if (pthread_create(&sb->proxy_thread, NULL, net_proxy_server_thread, sb) !=
        0) {
        sb->proxy_running = false;
        close(listen_fd);
        sb->proxy_listen_fd = -1;
        sb->proxy_port      = 0;
        return -1;
    }

    return 0;
}

void net_sandbox_stop_proxy(NetSandbox *sb) {
    if (!sb || !sb->proxy_running)
        return;

    sb->proxy_running = false;
    if (sb->proxy_listen_fd >= 0) {
        shutdown(sb->proxy_listen_fd, SHUT_RDWR);
        close(sb->proxy_listen_fd);
        sb->proxy_listen_fd = -1;
    }
    pthread_join(sb->proxy_thread, NULL);
    sb->proxy_port = 0;
}

/* -------------------------------------------------------------------------
 * NetSandbox Lifecycle
 * ------------------------------------------------------------------------- */

NetSandbox *net_sandbox_create(void) {
    NetSandbox *sb = calloc(1, sizeof(NetSandbox));
    if (!sb)
        return NULL;
    sb->is_active       = true;
    sb->offline         = false;
    sb->allow_loopback  = true; // Loopback allowed by default when filtering
    sb->proxy_listen_fd = -1;
    sb->proxy_port      = 0;
    sb->proxy_running   = false;
    return sb;
}

static void free_domains(NetSandboxDomain *head) {
    while (head) {
        NetSandboxDomain *next = head->next;
        free(head->domain);
        free(head);
        head = next;
    }
}

static void free_ports(NetSandboxPort *head) {
    while (head) {
        NetSandboxPort *next = head->next;
        free(head);
        head = next;
    }
}

static void free_ips(NetSandboxIP *head) {
    while (head) {
        NetSandboxIP *next = head->next;
        free(head->ip);
        free(head);
        head = next;
    }
}

void net_sandbox_free(NetSandbox *sb) {
    if (!sb)
        return;
    net_sandbox_stop_proxy(sb);
    free_domains(sb->allowed_domains);
    free_ports(sb->allowed_ports);
    free_ports(sb->denied_ports);
    free_ips(sb->allowed_ips);
    free(sb->saved_http_proxy);
    free(sb->saved_https_proxy);
    free(sb->saved_all_proxy);
    free(sb->saved_no_proxy);
    free(sb);
}

NetSandbox *net_sandbox_copy_stack(const NetSandbox *stack) {
    if (!stack)
        return NULL;

    NetSandbox  *head = NULL;
    NetSandbox **tail = &head;

    for (const NetSandbox *curr = stack; curr; curr = curr->prev) {
        NetSandbox *copy = net_sandbox_create();
        if (!copy)
            break;

        copy->is_active      = curr->is_active;
        copy->offline        = curr->offline;
        copy->allow_loopback = curr->allow_loopback;

        for (NetSandboxDomain *d = curr->allowed_domains; d; d = d->next) {
            net_sandbox_add_domain(copy, d->domain);
        }
        for (NetSandboxPort *p = curr->allowed_ports; p; p = p->next) {
            net_sandbox_add_port(copy, p->port, true);
        }
        for (NetSandboxPort *p = curr->denied_ports; p; p = p->next) {
            net_sandbox_add_port(copy, p->port, false);
        }
        for (NetSandboxIP *ip = curr->allowed_ips; ip; ip = ip->next) {
            net_sandbox_add_ip(copy, ip->ip);
        }

        if (curr->saved_http_proxy)
            copy->saved_http_proxy = strdup(curr->saved_http_proxy);
        if (curr->saved_https_proxy)
            copy->saved_https_proxy = strdup(curr->saved_https_proxy);
        if (curr->saved_all_proxy)
            copy->saved_all_proxy = strdup(curr->saved_all_proxy);
        if (curr->saved_no_proxy)
            copy->saved_no_proxy = strdup(curr->saved_no_proxy);

        *tail = copy;
        tail  = &copy->prev;
    }

    return head;
}

void net_sandbox_free_stack(NetSandbox *stack) {
    while (stack) {
        NetSandbox *next = stack->prev;
        net_sandbox_free(stack);
        stack = next;
    }
}

void net_sandbox_push(Session *session, NetSandbox *sb) {
    if (!session || !sb)
        return;

    // Start proxy server if domain filtering or non-offline loopback/port rules
    // are present
    if (!sb->offline &&
        (sb->allowed_domains != NULL || !sb->allow_loopback ||
         sb->allowed_ports != NULL || sb->denied_ports != NULL)) {
        net_sandbox_start_proxy(sb);
    }

    sb->prev             = session->net_sandbox;
    session->net_sandbox = sb;
}

NetSandbox *net_sandbox_pop(Session *session) {
    if (!session || !session->net_sandbox)
        return NULL;
    NetSandbox *top      = session->net_sandbox;
    session->net_sandbox = top->prev;
    top->prev            = NULL;
    net_sandbox_stop_proxy(top);
    return top;
}

/* -------------------------------------------------------------------------
 * Rule Configuration
 * ------------------------------------------------------------------------- */

void net_sandbox_add_ip(NetSandbox *sb, const char *ip) {
    if (!sb || !ip || ip[0] == '\0')
        return;
    for (NetSandboxIP *curr = sb->allowed_ips; curr; curr = curr->next) {
        if (strcmp(curr->ip, ip) == 0)
            return;
    }
    NetSandboxIP *node = calloc(1, sizeof(NetSandboxIP));
    if (!node)
        return;
    node->ip        = strdup(ip);
    node->next      = sb->allowed_ips;
    sb->allowed_ips = node;
}

void net_sandbox_add_domain(NetSandbox *sb, const char *domain) {
    if (!sb || !domain || domain[0] == '\0')
        return;

    for (NetSandboxDomain *curr = sb->allowed_domains; curr;
         curr                   = curr->next) {
        if (strcmp(curr->domain, domain) == 0)
            return;
    }

    NetSandboxDomain *node = calloc(1, sizeof(NetSandboxDomain));
    if (!node)
        return;
    node->domain        = strdup(domain);
    node->next          = sb->allowed_domains;
    sb->allowed_domains = node;

    // Resolve IPs of domain so direct IP connects are also permitted
    struct addrinfo  hints = {0};
    struct addrinfo *res   = NULL;
    hints.ai_family        = AF_UNSPEC;
    hints.ai_socktype      = SOCK_STREAM;

    if (getaddrinfo(domain, NULL, &hints, &res) == 0 && res) {
        for (struct addrinfo *p = res; p; p = p->ai_next) {
            char ip_str[INET6_ADDRSTRLEN] = {0};
            if (p->ai_family == AF_INET) {
                struct sockaddr_in *in = (struct sockaddr_in *)p->ai_addr;
                inet_ntop(AF_INET, &in->sin_addr, ip_str, sizeof(ip_str));
                net_sandbox_add_ip(sb, ip_str);
            } else if (p->ai_family == AF_INET6) {
                struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)p->ai_addr;
                inet_ntop(AF_INET6, &in6->sin6_addr, ip_str, sizeof(ip_str));
                net_sandbox_add_ip(sb, ip_str);
            }
        }
        freeaddrinfo(res);
    }
}

void net_sandbox_add_port(NetSandbox *sb, int port, bool allow) {
    if (!sb || port <= 0 || port > 65535)
        return;

    NetSandboxPort **head = allow ? &sb->allowed_ports : &sb->denied_ports;

    for (NetSandboxPort *curr = *head; curr; curr = curr->next) {
        if (curr->port == port)
            return;
    }

    NetSandboxPort *node = calloc(1, sizeof(NetSandboxPort));
    if (!node)
        return;
    node->port = port;
    node->next = *head;
    *head      = node;
}

void net_sandbox_parse_ports(NetSandbox *sb, const char *port_list,
                             bool allow) {
    if (!sb || !port_list || port_list[0] == '\0')
        return;

    char buf[1024];
    strncpy(buf, port_list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ",: ");
    while (token) {
        int port = atoi(token);
        if (port > 0 && port <= 65535) {
            net_sandbox_add_port(sb, port, allow);
        }
        token = strtok(NULL, ",: ");
    }
}

void net_sandbox_parse_domains(NetSandbox *sb, const char *domain_list) {
    if (!sb || !domain_list || domain_list[0] == '\0')
        return;

    char buf[2048];
    strncpy(buf, domain_list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ",: ");
    while (token) {
        net_sandbox_add_domain(sb, token);
        token = strtok(NULL, ",: ");
    }
}

/* -------------------------------------------------------------------------
 * Preload Library Locator
 * ------------------------------------------------------------------------- */

const char *net_sandbox_get_preload_path(void) {
    static char cached_path[PATH_MAX] = {0};
    if (cached_path[0] != '\0') {
        if (access(cached_path, R_OK) == 0)
            return cached_path;
    }

    const char *env_p = getenv("TIDESH_PRELOAD_PATH");
    if (env_p && access(env_p, R_OK) == 0) {
        strncpy(cached_path, env_p, sizeof(cached_path) - 1);
        return cached_path;
    }

#if defined(__APPLE__)
    const char *ext = "dylib";
#else
    const char *ext = "so";
#endif

    const char *candidates[] = {
        "bin/libtidesh_net.dylib",
        "bin/libtidesh_net.so",
        "./libtidesh_net.dylib",
        "./libtidesh_net.so",
        "/usr/local/lib/libtidesh_net.dylib",
        "/usr/local/lib/libtidesh_net.so",
        "bindings/python/tidesh/libtidesh_net.dylib",
        "bindings/python/tidesh/libtidesh_net.so",
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (access(candidates[i], R_OK) == 0) {
            char abs_buf[PATH_MAX];
            if (realpath(candidates[i], abs_buf)) {
                strncpy(cached_path, abs_buf, sizeof(cached_path) - 1);
                return cached_path;
            }
        }
    }

    // Attempt compiling from src/sandbox/preload.c if found
    const char *src_candidates[] = {
        "src/sandbox/preload.c",       "../src/sandbox/preload.c",
        "../../src/sandbox/preload.c", "src/sandbox_preload.c",
        "../src/sandbox_preload.c",    "../../src/sandbox_preload.c"};

    for (size_t i = 0; i < sizeof(src_candidates) / sizeof(src_candidates[0]);
         i++) {
        if (access(src_candidates[i], R_OK) == 0) {
            char tmp_target[PATH_MAX];
            snprintf(tmp_target, sizeof(tmp_target), "/tmp/libtidesh_net_%d.%s",
                     (int)getpid(), ext);
            char cmd[PATH_MAX * 2];
#if defined(__APPLE__)
            snprintf(cmd, sizeof(cmd),
                     "cc -dynamiclib -O2 -fPIC -Wall %s -o %s >/dev/null 2>&1",
                     src_candidates[i], tmp_target);
#else
            snprintf(cmd, sizeof(cmd),
                     "cc -shared -O2 -fPIC -Wall %s -o %s -ldl >/dev/null 2>&1",
                     src_candidates[i], tmp_target);
#endif
            int ret = system(cmd);
            if (ret == 0 && access(tmp_target, R_OK) == 0) {
                strncpy(cached_path, tmp_target, sizeof(cached_path) - 1);
                return cached_path;
            }
        }
    }

    return NULL;
}

/* -------------------------------------------------------------------------
 * Child Process Network Sandboxing Application
 * ------------------------------------------------------------------------- */

void net_sandbox_apply_child(Session *session) {
    if (!session || !session->net_sandbox)
        return;

    NetSandbox *sb = session->net_sandbox;
    if (!sb->is_active)
        return;

    // Close inherited listening proxy socket in child
    if (sb->proxy_listen_fd >= 0) {
        close(sb->proxy_listen_fd);
    }

    // 1. Airgap Linux network namespace if unshare is possible
#if defined(__linux__)
    if (sb->offline && !sb->allow_loopback) {
#if defined(CLONE_NEWNET)
        unshare(CLONE_NEWNET);
#endif
    }
#endif

    // 2. Set proxy environment in child
    if (sb->offline) {
        environ_set(session->environ, "http_proxy", "http://127.0.0.1:0");
        environ_set(session->environ, "https_proxy", "http://127.0.0.1:0");
        environ_set(session->environ, "all_proxy", "http://127.0.0.1:0");
        environ_set(session->environ, "HTTP_PROXY", "http://127.0.0.1:0");
        environ_set(session->environ, "HTTPS_PROXY", "http://127.0.0.1:0");
        environ_set(session->environ, "ALL_PROXY", "http://127.0.0.1:0");
        if (sb->allow_loopback) {
            environ_set(session->environ, "no_proxy",
                        "localhost,127.0.0.1,::1,*.local");
            environ_set(session->environ, "NO_PROXY",
                        "localhost,127.0.0.1,::1,*.local");
        } else {
            environ_set(session->environ, "no_proxy", "");
            environ_set(session->environ, "NO_PROXY", "");
        }
    } else if (sb->proxy_port > 0) {
        char proxy_url[64];
        snprintf(proxy_url, sizeof(proxy_url), "http://127.0.0.1:%d",
                 sb->proxy_port);
        environ_set(session->environ, "http_proxy", proxy_url);
        environ_set(session->environ, "https_proxy", proxy_url);
        environ_set(session->environ, "all_proxy", proxy_url);
        environ_set(session->environ, "HTTP_PROXY", proxy_url);
        environ_set(session->environ, "HTTPS_PROXY", proxy_url);
        environ_set(session->environ, "ALL_PROXY", proxy_url);
        if (sb->allow_loopback) {
            environ_set(session->environ, "no_proxy",
                        "localhost,127.0.0.1,::1,*.local");
            environ_set(session->environ, "NO_PROXY",
                        "localhost,127.0.0.1,::1,*.local");
        } else {
            environ_set(session->environ, "no_proxy", "");
            environ_set(session->environ, "NO_PROXY", "");
        }
    }

    // 3. Configure preload library
    const char *preload = net_sandbox_get_preload_path();
    if (preload) {
#if defined(__APPLE__)
        environ_set(session->environ, "DYLD_INSERT_LIBRARIES", (char *)preload);
        environ_set(session->environ, "DYLD_FORCE_FLAT_NAMESPACE", "1");
#else
        environ_set(session->environ, "LD_PRELOAD", (char *)preload);
#endif
        environ_set(session->environ, "TIDESH_NET_ACTIVE", "1");
        if (sb->offline) {
            environ_set(session->environ, "TIDESH_NET_OFFLINE", "1");
        }
        if (sb->allow_loopback) {
            environ_set(session->environ, "TIDESH_NET_ALLOW_LOOPBACK", "1");
        }

        if (sb->allowed_domains) {
            char   dom_buf[2048] = {0};
            size_t off           = 0;
            for (NetSandboxDomain *d = sb->allowed_domains; d; d = d->next) {
                off += snprintf(dom_buf + off, sizeof(dom_buf) - off, "%s%s",
                                (off == 0 ? "" : ","), d->domain);
            }
            environ_set(session->environ, "TIDESH_NET_ALLOWED_DOMAINS",
                        dom_buf);
        }

        if (sb->allowed_ports) {
            char   port_buf[1024] = {0};
            size_t off            = 0;
            for (NetSandboxPort *p = sb->allowed_ports; p; p = p->next) {
                off += snprintf(port_buf + off, sizeof(port_buf) - off, "%s%d",
                                (off == 0 ? "" : ","), p->port);
            }
            environ_set(session->environ, "TIDESH_NET_ALLOWED_PORTS", port_buf);
        }

        if (sb->denied_ports) {
            char   port_buf[1024] = {0};
            size_t off            = 0;
            for (NetSandboxPort *p = sb->denied_ports; p; p = p->next) {
                off += snprintf(port_buf + off, sizeof(port_buf) - off, "%s%d",
                                (off == 0 ? "" : ","), p->port);
            }
            environ_set(session->environ, "TIDESH_NET_DENIED_PORTS", port_buf);
        }

        if (sb->allowed_ips) {
            char   ip_buf[2048] = {0};
            size_t off          = 0;
            for (NetSandboxIP *ip = sb->allowed_ips; ip; ip = ip->next) {
                off += snprintf(ip_buf + off, sizeof(ip_buf) - off, "%s%s",
                                (off == 0 ? "" : ","), ip->ip);
            }
            environ_set(session->environ, "TIDESH_NET_ALLOWED_IPS", ip_buf);
        }
    }

#if defined(__APPLE__)
    if (sb->offline) {
        char   profile[2048];
        size_t off = 0;
        off += snprintf(profile + off, sizeof(profile) - off,
                        "(version 1)\n(allow default)\n(deny network*)\n");
        if (sb->allow_loopback) {
            off += snprintf(profile + off, sizeof(profile) - off,
                            "(allow network* (local ip \"localhost:*\"))\n"
                            "(allow network* (remote ip \"localhost:*\"))\n");
        }
        safe_sandbox_init(profile);
    } else if (sb->denied_ports) {
        char   profile[2048];
        size_t off = 0;
        off += snprintf(profile + off, sizeof(profile) - off,
                        "(version 1)\n(allow default)\n");
        for (NetSandboxPort *p = sb->denied_ports; p; p = p->next) {
            off += snprintf(profile + off, sizeof(profile) - off,
                            "(deny network-outbound (remote tcp \"*:%d\"))\n"
                            "(deny network-outbound (remote udp \"*:%d\"))\n",
                            p->port, p->port);
        }
        safe_sandbox_init(profile);
    }
#endif
}
