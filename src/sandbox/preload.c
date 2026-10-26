/** sandbox_preload.c
 *
 * Preload library for network sandboxing in tidesh.
 * Intercepts socket, connect, bind, getaddrinfo, and gethostbyname
 * to enforce air-gapping, domain allowlists, and port restrictions.
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__APPLE__)
#define DYLD_INTERPOSE(_replacement, _replacee)                                \
    __attribute__((used)) static struct {                                      \
        const void *replacement;                                               \
        const void *replacee;                                                  \
    } _interpose_##_replacee                                                   \
        __attribute__((section("__DATA,__interpose"))) = {                     \
            (const void *)(unsigned long)&_replacement,                        \
            (const void *)(unsigned long)&_replacee};
#endif

/* -------------------------------------------------------------------------
 * Configuration Helpers
 * ------------------------------------------------------------------------- */

static inline bool is_active(void) {
    const char *act = getenv("TIDESH_NET_ACTIVE");
    return act && (strcmp(act, "1") == 0 || strcmp(act, "true") == 0);
}

static inline bool is_offline(void) {
    const char *off = getenv("TIDESH_NET_OFFLINE");
    return off && (strcmp(off, "1") == 0 || strcmp(off, "true") == 0);
}

static inline bool is_loopback_allowed(void) {
    const char *lo = getenv("TIDESH_NET_ALLOW_LOOPBACK");
    return lo && (strcmp(lo, "1") == 0 || strcmp(lo, "true") == 0);
}

static bool is_addr_loopback(const struct sockaddr *addr) {
    if (!addr)
        return false;
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *in     = (const struct sockaddr_in *)addr;
        uint32_t                  s_addr = ntohl(in->sin_addr.s_addr);
        return ((s_addr >> 24) == 127);
    } else if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)addr;
        return IN6_IS_ADDR_LOOPBACK(&in6->sin6_addr);
    }
    return false;
}

static int get_port(const struct sockaddr *addr) {
    if (!addr)
        return -1;
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
        return ntohs(in->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)addr;
        return ntohs(in6->sin6_port);
    }
    return -1;
}

static bool check_port_in_list(int port, const char *env_var) {
    const char *list = getenv(env_var);
    if (!list || list[0] == '\0')
        return false;

    char buf[1024];
    strncpy(buf, list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ",: ");
    while (token) {
        int p = atoi(token);
        if (p == port)
            return true;
        token = strtok(NULL, ",: ");
    }
    return false;
}

static bool is_port_allowed(int port) {
    // 1. Check denied ports
    if (check_port_in_list(port, "TIDESH_NET_DENIED_PORTS")) {
        return false;
    }

    // 2. Check allowed ports (if specified, only those are allowed)
    const char *allowed_list = getenv("TIDESH_NET_ALLOWED_PORTS");
    if (allowed_list && allowed_list[0] != '\0') {
        return check_port_in_list(port, "TIDESH_NET_ALLOWED_PORTS");
    }

    return true;
}

static bool check_ip_in_list(const char *ip, const char *env_var) {
    const char *list = getenv(env_var);
    if (!list || list[0] == '\0')
        return false;

    char buf[2048];
    strncpy(buf, list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ",: ");
    while (token) {
        if (strcmp(token, ip) == 0)
            return true;
        token = strtok(NULL, ",: ");
    }
    return false;
}

static bool check_domain_in_list(const char *node, const char *env_var) {
    if (!node)
        return false;

    const char *list = getenv(env_var);
    if (!list || list[0] == '\0')
        return false;

    char buf[2048];
    strncpy(buf, list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ",: ");
    while (token) {
        // Exact match
        if (strcasecmp(token, node) == 0)
            return true;

        // Wildcard match (e.g. *.github.com or token is github.com and node
        // ends with .github.com)
        const char *dom = token;
        if (dom[0] == '*' && dom[1] == '.') {
            dom += 2;
        }
        size_t dom_len  = strlen(dom);
        size_t node_len = strlen(node);
        if (node_len > dom_len && node[node_len - dom_len - 1] == '.' &&
            strcasecmp(node + (node_len - dom_len), dom) == 0) {
            return true;
        }

        token = strtok(NULL, ",: ");
    }
    return false;
}

/* -------------------------------------------------------------------------
 * Intercepted Functions
 * ------------------------------------------------------------------------- */

#if !defined(__APPLE__)
static int (*real_connect)(int, const struct sockaddr *, socklen_t) = NULL;
static int (*real_bind)(int, const struct sockaddr *, socklen_t)    = NULL;
static int (*real_socket)(int, int, int)                            = NULL;
static int (*real_getaddrinfo)(const char *, const char *,
                               const struct addrinfo *,
                               struct addrinfo **)                  = NULL;
static struct hostent *(*real_gethostbyname)(const char *)          = NULL;

static void init_real_functions(void) {
    if (!real_connect)
        real_connect = dlsym(RTLD_NEXT, "connect");
    if (!real_bind)
        real_bind = dlsym(RTLD_NEXT, "bind");
    if (!real_socket)
        real_socket = dlsym(RTLD_NEXT, "socket");
    if (!real_getaddrinfo)
        real_getaddrinfo = dlsym(RTLD_NEXT, "getaddrinfo");
    if (!real_gethostbyname)
        real_gethostbyname = dlsym(RTLD_NEXT, "gethostbyname");
}
#endif

/* --- socket() --- */

int tidesh_net_socket(int domain, int type, int protocol) {
    if (is_active()) {
        // Unix domain sockets always permitted (for IPC/pipes)
        if (domain != AF_UNIX) {
            if (is_offline() && !is_loopback_allowed()) {
                errno = EPERM;
                return -1;
            }
        }
    }
#if defined(__APPLE__)
    return socket(domain, type, protocol);
#else
    init_real_functions();
    return real_socket(domain, type, protocol);
#endif
}

/* --- connect() --- */

int tidesh_net_connect(int sockfd, const struct sockaddr *addr,
                       socklen_t addrlen) {
    if (is_active() && addr) {
        if (addr->sa_family == AF_INET || addr->sa_family == AF_INET6) {
            bool is_lo = is_addr_loopback(addr);
            int  port  = get_port(addr);

            // 1. Loopback restriction: if loopback is disallowed, block
            // immediately
            if (is_lo && !is_loopback_allowed()) {
                errno = ECONNREFUSED;
                return -1;
            }

            // 2. Airgap check
            if (is_offline()) {
                if (!is_lo || !is_loopback_allowed()) {
                    errno = ENETUNREACH;
                    return -1;
                }
            }

            // 3. Port restrictions
            if (!is_port_allowed(port)) {
                errno = ECONNREFUSED;
                return -1;
            }

            // 4. IP / Domain allowlist check (if non-loopback and allowed
            // domains or IPs configured)
            if (!is_lo) {
                const char *allowed_ips  = getenv("TIDESH_NET_ALLOWED_IPS");
                const char *allowed_doms = getenv("TIDESH_NET_ALLOWED_DOMAINS");
                if ((allowed_ips && allowed_ips[0] != '\0') ||
                    (allowed_doms && allowed_doms[0] != '\0')) {
                    char ip_buf[INET6_ADDRSTRLEN] = {0};
                    if (addr->sa_family == AF_INET) {
                        const struct sockaddr_in *in =
                            (const struct sockaddr_in *)addr;
                        inet_ntop(AF_INET, &in->sin_addr, ip_buf,
                                  sizeof(ip_buf));
                    } else {
                        const struct sockaddr_in6 *in6 =
                            (const struct sockaddr_in6 *)addr;
                        inet_ntop(AF_INET6, &in6->sin6_addr, ip_buf,
                                  sizeof(ip_buf));
                    }
                    if (!check_ip_in_list(ip_buf, "TIDESH_NET_ALLOWED_IPS")) {
                        errno = ENETUNREACH;
                        return -1;
                    }
                }
            }
        }
    }

#if defined(__APPLE__)
    return connect(sockfd, addr, addrlen);
#else
    init_real_functions();
    return real_connect(sockfd, addr, addrlen);
#endif
}

/* --- bind() --- */

int tidesh_net_bind(int sockfd, const struct sockaddr *addr,
                    socklen_t addrlen) {
    if (is_active() && addr) {
        if (addr->sa_family == AF_INET || addr->sa_family == AF_INET6) {
            bool is_lo = is_addr_loopback(addr);
            int  port  = get_port(addr);

            if (is_lo && !is_loopback_allowed()) {
                errno = EACCES;
                return -1;
            }

            if (!is_lo && is_offline()) {
                errno = EACCES;
                return -1;
            }

            if (!is_port_allowed(port)) {
                errno = EACCES;
                return -1;
            }
        }
    }

#if defined(__APPLE__)
    return bind(sockfd, addr, addrlen);
#else
    init_real_functions();
    return real_bind(sockfd, addr, addrlen);
#endif
}

/* --- getaddrinfo() --- */

int tidesh_net_getaddrinfo(const char *node, const char *service,
                           const struct addrinfo *hints,
                           struct addrinfo      **res) {
    if (is_active() && node) {
        bool is_lo =
            (strcmp(node, "localhost") == 0 || strcmp(node, "127.0.0.1") == 0 ||
             strcmp(node, "::1") == 0);

        if (is_lo && !is_loopback_allowed()) {
            return EAI_NONAME;
        }

        if (is_offline()) {
            if (!is_lo || !is_loopback_allowed()) {
                return EAI_NONAME;
            }
        }

        // Domain allowlist check
        const char *allowed_doms = getenv("TIDESH_NET_ALLOWED_DOMAINS");
        if (allowed_doms && allowed_doms[0] != '\0') {
            if (is_lo) {
                if (!is_loopback_allowed()) {
                    return EAI_NONAME;
                }
            } else {
                if (!check_domain_in_list(node, "TIDESH_NET_ALLOWED_DOMAINS")) {
                    return EAI_NONAME;
                }
            }
        }
    }

#if defined(__APPLE__)
    return getaddrinfo(node, service, hints, res);
#else
    init_real_functions();
    return real_getaddrinfo(node, service, hints, res);
#endif
}

/* --- gethostbyname() --- */

struct hostent *tidesh_net_gethostbyname(const char *name) {
    if (is_active() && name) {
        bool is_lo =
            (strcmp(name, "localhost") == 0 || strcmp(name, "127.0.0.1") == 0 ||
             strcmp(name, "::1") == 0);

        if (is_lo && !is_loopback_allowed()) {
            h_errno = HOST_NOT_FOUND;
            return NULL;
        }

        if (is_offline()) {
            if (!is_lo || !is_loopback_allowed()) {
                h_errno = HOST_NOT_FOUND;
                return NULL;
            }
        }

        const char *allowed_doms = getenv("TIDESH_NET_ALLOWED_DOMAINS");
        if (allowed_doms && allowed_doms[0] != '\0') {
            if (is_lo) {
                if (!is_loopback_allowed()) {
                    h_errno = HOST_NOT_FOUND;
                    return NULL;
                }
            } else {
                if (!check_domain_in_list(name, "TIDESH_NET_ALLOWED_DOMAINS")) {
                    h_errno = HOST_NOT_FOUND;
                    return NULL;
                }
            }
        }
    }

#if defined(__APPLE__)
    return gethostbyname(name);
#else
    init_real_functions();
    return real_gethostbyname(name);
#endif
}

#if defined(__APPLE__)
DYLD_INTERPOSE(tidesh_net_socket, socket)
DYLD_INTERPOSE(tidesh_net_connect, connect)
DYLD_INTERPOSE(tidesh_net_bind, bind)
DYLD_INTERPOSE(tidesh_net_getaddrinfo, getaddrinfo)
DYLD_INTERPOSE(tidesh_net_gethostbyname, gethostbyname)
#else
int socket(int domain, int type, int protocol) {
    return tidesh_net_socket(domain, type, protocol);
}
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return tidesh_net_connect(sockfd, addr, addrlen);
}
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return tidesh_net_bind(sockfd, addr, addrlen);
}
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res) {
    return tidesh_net_getaddrinfo(node, service, hints, res);
}
struct hostent *gethostbyname(const char *name) {
    return tidesh_net_gethostbyname(name);
}
#endif
