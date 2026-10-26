/** net.h
 *
 * Declarations for network isolation and sandboxing in tidesh.
 * Provides data structures and functions for controlling network
 * access, domain allowlists, port restrictions, air-gapping,
 * and proxy enforcement across UNIX-like systems and macOS.
 */

#ifndef SANDBOX_NET_H
#define SANDBOX_NET_H

#include <pthread.h>
#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

typedef struct Session Session;

/* Linked list of allowed domain rules */
typedef struct NetSandboxDomain {
    char                    *domain;
    struct NetSandboxDomain *next;
} NetSandboxDomain;

/* Linked list of allowed or denied ports */
typedef struct NetSandboxPort {
    int                    port;
    struct NetSandboxPort *next;
} NetSandboxPort;

/* Linked list of explicitly allowed IP addresses */
typedef struct NetSandboxIP {
    char                *ip;
    struct NetSandboxIP *next;
} NetSandboxIP;

/* Network sandbox layer */
typedef struct NetSandbox {
    bool               is_active;
    bool               offline; /* Full air-gapping: no external network */
    bool               allow_loopback;  /* Allow 127.0.0.1 / localhost */
    NetSandboxDomain  *allowed_domains; /* Whitelisted domains */
    NetSandboxPort    *allowed_ports;   /* Whitelisted remote ports */
    NetSandboxPort    *denied_ports;    /* Blacklisted remote ports */
    NetSandboxIP      *allowed_ips;     /* Resolved or specified allowed IPs */
    int                proxy_port;      /* Local proxy port if active, or 0 */
    int                proxy_listen_fd; /* Local proxy listen fd */
    pthread_t          proxy_thread;    /* Local proxy thread */
    volatile bool      proxy_running;   /* True if proxy server is running */
    char              *saved_http_proxy;
    char              *saved_https_proxy;
    char              *saved_all_proxy;
    char              *saved_no_proxy;
    struct NetSandbox *prev; /* Stack support */
} NetSandbox;

/**
 * Allocate and initialize a new NetSandbox layer.
 *
 * @return Pointer to allocated NetSandbox, or NULL on allocation failure
 */
NetSandbox *net_sandbox_create(void);

/**
 * Free a single NetSandbox structure and its lists.
 *
 * @param sb Pointer to NetSandbox to free
 */
void net_sandbox_free(NetSandbox *sb);

/**
 * Deep copy a stack of NetSandbox layers.
 *
 * @param stack Pointer to top of stack
 * @return Pointer to head of copied stack
 */
NetSandbox *net_sandbox_copy_stack(const NetSandbox *stack);

/**
 * Free an entire stack of NetSandbox layers.
 *
 * @param stack Pointer to top of stack
 */
void net_sandbox_free_stack(NetSandbox *stack);

/**
 * Push a NetSandbox layer onto the session's network sandbox stack.
 *
 * @param session Pointer to Session
 * @param sb Pointer to NetSandbox layer to push
 */
void net_sandbox_push(Session *session, NetSandbox *sb);

/**
 * Pop the top NetSandbox layer from the session's network sandbox stack.
 *
 * @param session Pointer to Session
 * @return Pointer to popped NetSandbox layer (caller may free)
 */
NetSandbox *net_sandbox_pop(Session *session);

/**
 * Add a domain to the allowed domains list.
 *
 * @param sb Pointer to NetSandbox
 * @param domain Domain name or pattern to allow
 */
void net_sandbox_add_domain(NetSandbox *sb, const char *domain);

/**
 * Add a port to the allowed or denied ports list.
 *
 * @param sb Pointer to NetSandbox
 * @param port Port number (1-65535)
 * @param allow True for allowed ports, false for denied ports
 */
void net_sandbox_add_port(NetSandbox *sb, int port, bool allow);

/**
 * Parse colon/comma-separated ports into NetSandbox.
 *
 * @param sb Pointer to NetSandbox
 * @param port_list Colon or comma-separated ports (e.g. "80,443")
 * @param allow True for allowed ports, false for denied ports
 */
void net_sandbox_parse_ports(NetSandbox *sb, const char *port_list, bool allow);

/**
 * Parse colon/comma-separated domains into NetSandbox.
 *
 * @param sb Pointer to NetSandbox
 * @param domain_list Colon or comma-separated domains
 */
void net_sandbox_parse_domains(NetSandbox *sb, const char *domain_list);

/**
 * Add an IP address to the allowed IPs list.
 *
 * @param sb Pointer to NetSandbox
 * @param ip IP address string
 */
void net_sandbox_add_ip(NetSandbox *sb, const char *ip);

/**
 * Start the local filtering proxy for this NetSandbox layer.
 *
 * @param sb Pointer to NetSandbox
 * @return 0 on success, non-zero on error
 */
int net_sandbox_start_proxy(NetSandbox *sb);

/**
 * Stop the local filtering proxy for this NetSandbox layer.
 *
 * @param sb Pointer to NetSandbox
 */
void net_sandbox_stop_proxy(NetSandbox *sb);

/**
 * Locate the preload interceptor library path on the system.
 *
 * @return Path to libtidesh_net library, or NULL if unavailable
 */
const char *net_sandbox_get_preload_path(void);

/**
 * Apply active network sandbox restrictions to child process before execve.
 *
 * @param session Pointer to Session
 */
void net_sandbox_apply_child(Session *session);

#endif /* SANDBOX_NET_H */
