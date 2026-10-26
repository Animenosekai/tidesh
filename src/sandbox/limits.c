/** limits.c
 *
 * Resource limits and execution timeouts sandboxing for tidesh.
 * Manages:
 *  - Execution timeouts (setitimer, SIGALRM, child process termination)
 *  - CPU time and core limits (RLIMIT_CPU, sched_setaffinity)
 *  - Memory limits (RLIMIT_AS, RLIMIT_DATA)
 *  - Process limits & fork-bomb prevention (RLIMIT_NPROC, shell fork tracking)
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__linux__) && defined(_GNU_SOURCE)
#include <sched.h>
#endif

#include "environ.h"
#include "sandbox/limits.h"
#include "sandbox/net.h"
#include "session.h"

/* -------------------------------------------------------------------------
 * Global State for Asynchronous Timeout Signal Delivery
 * ------------------------------------------------------------------------- */

static volatile sig_atomic_t g_timeout_fired    = 0;
static volatile pid_t        g_active_child_pid = 0;
static Session              *g_active_session   = NULL;

static void timeout_alarm_handler(int sig) {
    (void)sig;
    g_timeout_fired = 1;

    pid_t target_pid = g_active_child_pid;
    if (target_pid > 0) {
        // Kill the foreground child and its process group
        kill(target_pid, SIGKILL);
        kill(-target_pid, SIGKILL);
    }

    if (g_active_session && g_active_session->limit_sandbox) {
        g_active_session->limit_sandbox->timed_out = true;
        g_active_session->exit_requested           = true;
    }
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

LimitSandbox *limit_sandbox_create(void) {
    LimitSandbox *sb = calloc(1, sizeof(LimitSandbox));
    if (!sb)
        return NULL;

    sb->is_active      = true;
    sb->timeout_signal = SIGKILL;
    return sb;
}

void limit_sandbox_free(LimitSandbox *sb) {
    if (!sb)
        return;
    if (sb->timer_armed) {
        struct itimerval zero_it = {0};
        setitimer(ITIMER_REAL, &zero_it, NULL);
        sigaction(SIGALRM, &sb->saved_sa, NULL);
        sb->timer_armed = false;
    }
    free(sb);
}

void limit_sandbox_free_stack(LimitSandbox *sb) {
    while (sb) {
        LimitSandbox *prev = sb->prev;
        limit_sandbox_free(sb);
        sb = prev;
    }
}

/* -------------------------------------------------------------------------
 * Session Stack Management
 * ------------------------------------------------------------------------- */

void limit_sandbox_push(Session *session, LimitSandbox *sb) {
    if (!session || !sb)
        return;
    sb->prev               = session->limit_sandbox;
    session->limit_sandbox = sb;
}

LimitSandbox *limit_sandbox_pop(Session *session) {
    if (!session || !session->limit_sandbox)
        return NULL;
    LimitSandbox *top      = session->limit_sandbox;
    session->limit_sandbox = top->prev;
    top->prev              = NULL;
    return top;
}

/* -------------------------------------------------------------------------
 * Parsing Helpers
 * ------------------------------------------------------------------------- */

uint64_t limit_sandbox_parse_bytes(const char *str) {
    if (!str || str[0] == '\0')
        return 0;

    while (*str == ' ' || *str == '\t')
        str++;

    char  *end = NULL;
    double val = strtod(str, &end);
    if (val < 0)
        return 0;
    if (end == str)
        return 0;

    while (*end == ' ' || *end == '\t')
        end++;

    char   unit[16] = {0};
    size_t ulen     = 0;
    while (*end && ulen < sizeof(unit) - 1) {
        unit[ulen++] = (char)tolower((unsigned char)*end++);
    }
    unit[ulen] = '\0';

    uint64_t mult = 1ULL;
    if (strcmp(unit, "t") == 0 || strcmp(unit, "tb") == 0) {
        mult = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    } else if (strcmp(unit, "g") == 0 || strcmp(unit, "gb") == 0) {
        mult = 1024ULL * 1024ULL * 1024ULL;
    } else if (strcmp(unit, "m") == 0 || strcmp(unit, "mb") == 0) {
        mult = 1024ULL * 1024ULL;
    } else if (strcmp(unit, "k") == 0 || strcmp(unit, "kb") == 0) {
        mult = 1024ULL;
    } else if (strcmp(unit, "b") == 0 || unit[0] == '\0') {
        mult = 1ULL;
    }

    return (uint64_t)(val * mult);
}

double limit_sandbox_parse_duration(const char *str) {
    if (!str || str[0] == '\0')
        return 0.0;

    while (*str == ' ' || *str == '\t')
        str++;

    char  *end = NULL;
    double val = strtod(str, &end);
    if (val < 0)
        return 0.0;
    if (end == str)
        return 0.0;

    while (*end == ' ' || *end == '\t')
        end++;

    char   unit[16] = {0};
    size_t ulen     = 0;
    while (*end && ulen < sizeof(unit) - 1) {
        unit[ulen++] = (char)tolower((unsigned char)*end++);
    }
    unit[ulen] = '\0';

    if (strcmp(unit, "h") == 0 || strcmp(unit, "hr") == 0 ||
        strcmp(unit, "hours") == 0) {
        return val * 3600.0;
    } else if (strcmp(unit, "m") == 0 || strcmp(unit, "min") == 0 ||
               strcmp(unit, "mins") == 0) {
        return val * 60.0;
    } else if (strcmp(unit, "s") == 0 || strcmp(unit, "sec") == 0 ||
               strcmp(unit, "secs") == 0 || unit[0] == '\0') {
        return val;
    } else if (strcmp(unit, "ms") == 0) {
        return val * 0.001;
    } else if (strcmp(unit, "us") == 0) {
        return val * 0.000001;
    }

    return val;
}

int limit_sandbox_parse_signal(const char *str) {
    if (!str || str[0] == '\0')
        return SIGKILL;

    while (*str == ' ' || *str == '\t')
        str++;

    if (isdigit((unsigned char)*str)) {
        int sig = atoi(str);
        return sig > 0 ? sig : SIGKILL;
    }

    const char *s = str;
    if (strncasecmp(s, "SIG", 3) == 0)
        s += 3;

    if (strcasecmp(s, "KILL") == 0 || strcasecmp(s, "9") == 0)
        return SIGKILL;
    if (strcasecmp(s, "TERM") == 0 || strcasecmp(s, "15") == 0)
        return SIGTERM;
    if (strcasecmp(s, "INT") == 0 || strcasecmp(s, "2") == 0)
        return SIGINT;
    if (strcasecmp(s, "QUIT") == 0 || strcasecmp(s, "3") == 0)
        return SIGQUIT;
    if (strcasecmp(s, "HUP") == 0 || strcasecmp(s, "1") == 0)
        return SIGHUP;
    if (strcasecmp(s, "USR1") == 0)
        return SIGUSR1;
    if (strcasecmp(s, "USR2") == 0)
        return SIGUSR2;

    return SIGKILL;
}

/* -------------------------------------------------------------------------
 * Process Limit & Fork-Bomb Prevention
 * ------------------------------------------------------------------------- */

bool limit_sandbox_can_fork(Session *session) {
    if (!session)
        return true;

    for (LimitSandbox *sb = session->limit_sandbox; sb; sb = sb->prev) {
        if (sb->is_active && sb->max_processes > 0) {
            if (session->active_child_count >= sb->max_processes) {
                return false;
            }
        }
    }
    return true;
}

void limit_sandbox_child_started(Session *session, pid_t pid) {
    if (session) {
        session->active_child_count++;
    }
    g_active_child_pid = pid;
}

void limit_sandbox_child_ended(Session *session, pid_t pid) {
    if (session && session->active_child_count > 0) {
        session->active_child_count--;
    }
    if (g_active_child_pid == pid) {
        g_active_child_pid = 0;
    }
}

/* -------------------------------------------------------------------------
 * Child Process Enforcement (setrlimit, sched_setaffinity)
 * ------------------------------------------------------------------------- */

void limit_sandbox_apply_child(Session *session) {
    if (!session)
        return;

    for (LimitSandbox *sb = session->limit_sandbox; sb; sb = sb->prev) {
        if (!sb->is_active)
            continue;

        // 1. CPU Time Limit (RLIMIT_CPU)
        if (sb->max_cpu_seconds > 0) {
            struct rlimit rl;
            rl.rlim_cur = (rlim_t)sb->max_cpu_seconds;
            rl.rlim_max = (rlim_t)sb->max_cpu_seconds;
            setrlimit(RLIMIT_CPU, &rl);
        }

        // 2. CPU Cores (Linux CPU Affinity)
#if defined(__linux__) && defined(_GNU_SOURCE)
        if (sb->max_cpu_cores > 0) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            long online = sysconf(_SC_NPROCESSORS_ONLN);
            int  cores  = sb->max_cpu_cores;
            if (online > 0 && cores > (int)online)
                cores = (int)online;
            for (int c = 0; c < cores && c < CPU_SETSIZE; c++) {
                CPU_SET(c, &cpuset);
            }
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
        }
#endif

        // 3. Memory Limit (RLIMIT_AS / RLIMIT_DATA and preload interceptor)
        if (sb->max_memory_bytes > 0) {
            char mem_buf[32];
            snprintf(mem_buf, sizeof(mem_buf), "%llu",
                     (unsigned long long)sb->max_memory_bytes);
            environ_set(session->environ, "TIDESH_LIMIT_MAX_MEMORY", mem_buf);

            const char *preload = net_sandbox_get_preload_path();
            if (preload) {
#if defined(__APPLE__)
                environ_set(session->environ, "DYLD_INSERT_LIBRARIES",
                            (char *)preload);
                environ_set(session->environ, "DYLD_FORCE_FLAT_NAMESPACE", "1");
#else
                environ_set(session->environ, "LD_PRELOAD", (char *)preload);
#endif
            }

            struct rlimit rl;
            rl.rlim_cur = (rlim_t)sb->max_memory_bytes;
            rl.rlim_max = (rlim_t)sb->max_memory_bytes;
#ifdef RLIMIT_AS
            setrlimit(RLIMIT_AS, &rl);
#endif
#ifdef RLIMIT_DATA
            setrlimit(RLIMIT_DATA, &rl);
#endif
        }

        // 4. Process Limit (RLIMIT_NPROC)
        if (sb->max_processes > 0) {
#ifdef RLIMIT_NPROC
            struct rlimit rl;
            rl.rlim_cur = (rlim_t)sb->max_processes;
            rl.rlim_max = (rlim_t)sb->max_processes;
            setrlimit(RLIMIT_NPROC, &rl);
#endif
        }
    }
}

/* -------------------------------------------------------------------------
 * Execution Timer Management
 * ------------------------------------------------------------------------- */

void limit_sandbox_start_timer(Session *session, LimitSandbox *sb) {
    if (!session || !sb || sb->timeout_seconds <= 0)
        return;

    g_timeout_fired  = 0;
    sb->timed_out    = false;
    g_active_session = session;

    struct sigaction sa = {0};
    sa.sa_handler       = timeout_alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags =
        0; // Don't use SA_RESTART so waitpid/reads get interrupted with EINTR
    sigaction(SIGALRM, &sa, &sb->saved_sa);

    struct itimerval it = {0};
    it.it_value.tv_sec  = (time_t)sb->timeout_seconds;
    it.it_value.tv_usec =
        (suseconds_t)((sb->timeout_seconds - (time_t)sb->timeout_seconds) *
                      1000000.0);
    if (it.it_value.tv_sec == 0 && it.it_value.tv_usec == 0) {
        it.it_value.tv_usec = 1000; // minimum 1ms
    }

    setitimer(ITIMER_REAL, &it, &sb->saved_it);
    sb->timer_armed = true;
}

void limit_sandbox_stop_timer(Session *session, LimitSandbox *sb) {
    g_timeout_fired = 0;
    if (!sb)
        return;

    if (sb->timer_armed) {
        struct itimerval zero_it = {0};
        setitimer(ITIMER_REAL, &zero_it, NULL);
        sigaction(SIGALRM, &sb->saved_sa, NULL);
        sb->timer_armed = false;

        // Restore previous outer timer if active
        if (sb->prev && sb->prev->timer_armed &&
            sb->prev->timeout_seconds > 0) {
            setitimer(ITIMER_REAL, &sb->prev->saved_it, NULL);
        }
    }

    if (g_active_session == session) {
        g_active_session = NULL;
    }
}

bool limit_sandbox_is_timed_out(Session *session) {
    if (!session || !session->limit_sandbox)
        return false;
    for (LimitSandbox *sb = session->limit_sandbox; sb; sb = sb->prev) {
        if (sb->timed_out)
            return true;
    }
    return false;
}

int limit_sandbox_get_timeout_signal(Session *session) {
    if (!session || !session->limit_sandbox)
        return SIGKILL;
    return session->limit_sandbox->timeout_signal;
}

void limit_sandbox_clear_timeout(Session *session) {
    g_timeout_fired = 0;
    if (!session)
        return;
    for (LimitSandbox *sb = session->limit_sandbox; sb; sb = sb->prev) {
        sb->timed_out = false;
    }
}

bool limit_sandbox_is_limit_exceeded(Session *session) {
    if (!session || !session->limit_sandbox)
        return false;
    for (LimitSandbox *sb = session->limit_sandbox; sb; sb = sb->prev) {
        if (sb->limit_exceeded)
            return true;
    }
    return false;
}

LimitResourceType limit_sandbox_get_exceeded_resource(Session *session) {
    if (!session || !session->limit_sandbox)
        return LIMIT_RESOURCE_NONE;
    for (LimitSandbox *sb = session->limit_sandbox; sb; sb = sb->prev) {
        if (sb->limit_exceeded)
            return sb->exceeded_resource;
    }
    return LIMIT_RESOURCE_NONE;
}

void limit_sandbox_set_limit_exceeded(Session          *session,
                                      LimitResourceType resource) {
    if (!session || !session->limit_sandbox)
        return;
    session->limit_sandbox->limit_exceeded    = true;
    session->limit_sandbox->exceeded_resource = resource;
}

void limit_sandbox_clear_limit(Session *session) {
    if (!session)
        return;
    for (LimitSandbox *sb = session->limit_sandbox; sb; sb = sb->prev) {
        sb->limit_exceeded    = false;
        sb->exceeded_resource = LIMIT_RESOURCE_NONE;
    }
}
