#ifndef TIDESH_SANDBOX_LIMITS_H
#define TIDESH_SANDBOX_LIMITS_H

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

struct Session;

typedef enum LimitResourceType {
    LIMIT_RESOURCE_NONE   = 0,
    LIMIT_RESOURCE_CPU    = 1,
    LIMIT_RESOURCE_MEMORY = 2,
    LIMIT_RESOURCE_PROCS  = 3
} LimitResourceType;

/**
 * LimitSandbox structure
 * Encapsulates execution timeouts, CPU caps, memory limits, and process limits.
 */
typedef struct LimitSandbox {
    bool is_active;

    // Execution Timeout
    double timeout_seconds;    // Timeout in seconds (0 = disabled)
    int    timeout_signal;     // Signal to deliver on timeout (default SIGKILL)
    bool   timed_out;          // Flag indicating timeout occurred
    struct itimerval saved_it; // Saved timer value for restoration
    struct sigaction saved_sa; // Saved signal action for restoration
    bool             timer_armed; // Whether timer is currently active

    // Resource Exceeded State
    bool              limit_exceeded;
    LimitResourceType exceeded_resource;

    // CPU Limits
    uint64_t max_cpu_seconds; // Max CPU time in seconds (0 = disabled)
    int      max_cpu_cores;   // Max CPU cores allowed (0 = disabled)

    // Memory Limits
    uint64_t max_memory_bytes; // Max virtual memory in bytes (0 = disabled)

    // Process Limits (Fork Bomb Prevention)
    int max_processes; // Max active child processes (0 = disabled)

    struct LimitSandbox *prev; // Stack chaining for nesting
} LimitSandbox;

/* Lifecycle */
LimitSandbox *limit_sandbox_create(void);
void          limit_sandbox_free(LimitSandbox *sb);
void          limit_sandbox_free_stack(LimitSandbox *sb);

/* Stack Management on Session */
void          limit_sandbox_push(struct Session *session, LimitSandbox *sb);
LimitSandbox *limit_sandbox_pop(struct Session *session);

/* Parsing Helpers */
uint64_t limit_sandbox_parse_bytes(const char *str);
double   limit_sandbox_parse_duration(const char *str);
int      limit_sandbox_parse_signal(const char *str);

/* Process Limit Enforcement */
bool limit_sandbox_can_fork(struct Session *session);
void limit_sandbox_child_started(struct Session *session, pid_t pid);
void limit_sandbox_child_ended(struct Session *session, pid_t pid);

/* Child Process Application (setrlimit, sched_setaffinity) */
void limit_sandbox_apply_child(struct Session *session);

/* Timer Management */
void limit_sandbox_start_timer(struct Session *session, LimitSandbox *sb);
void limit_sandbox_stop_timer(struct Session *session, LimitSandbox *sb);
bool limit_sandbox_is_timed_out(struct Session *session);
int  limit_sandbox_get_timeout_signal(struct Session *session);
void limit_sandbox_clear_timeout(struct Session *session);

/* Resource Limit State */
bool              limit_sandbox_is_limit_exceeded(struct Session *session);
LimitResourceType limit_sandbox_get_exceeded_resource(struct Session *session);
void              limit_sandbox_set_limit_exceeded(struct Session   *session,
                                                   LimitResourceType resource);
void              limit_sandbox_clear_limit(struct Session *session);

#endif /* TIDESH_SANDBOX_LIMITS_H */
