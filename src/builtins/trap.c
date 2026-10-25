/** trap.c
 *
 * Implementation of the trap builtin command.
 */

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins/trap.h"
#include "environ.h"
#include "execute.h"
#include "session.h"

typedef struct SignalInfo {
    int         signo;
    const char *name;
} SignalInfo;

static const SignalInfo g_signals[] = {
#ifdef SIGHUP
    {SIGHUP, "HUP"},
#endif
#ifdef SIGINT
    {SIGINT, "INT"},
#endif
#ifdef SIGQUIT
    {SIGQUIT, "QUIT"},
#endif
#ifdef SIGILL
    {SIGILL, "ILL"},
#endif
#ifdef SIGTRAP
    {SIGTRAP, "TRAP"},
#endif
#ifdef SIGABRT
    {SIGABRT, "ABRT"},
#endif
#ifdef SIGEMT
    {SIGEMT, "EMT"},
#endif
#ifdef SIGFPE
    {SIGFPE, "FPE"},
#endif
#ifdef SIGKILL
    {SIGKILL, "KILL"},
#endif
#ifdef SIGBUS
    {SIGBUS, "BUS"},
#endif
#ifdef SIGSEGV
    {SIGSEGV, "SEGV"},
#endif
#ifdef SIGSYS
    {SIGSYS, "SYS"},
#endif
#ifdef SIGPIPE
    {SIGPIPE, "PIPE"},
#endif
#ifdef SIGALRM
    {SIGALRM, "ALRM"},
#endif
#ifdef SIGTERM
    {SIGTERM, "TERM"},
#endif
#ifdef SIGURG
    {SIGURG, "URG"},
#endif
#ifdef SIGSTOP
    {SIGSTOP, "STOP"},
#endif
#ifdef SIGTSTP
    {SIGTSTP, "TSTP"},
#endif
#ifdef SIGCONT
    {SIGCONT, "CONT"},
#endif
#ifdef SIGCHLD
    {SIGCHLD, "CHLD"},
#endif
#ifdef SIGTTIN
    {SIGTTIN, "TTIN"},
#endif
#ifdef SIGTTOU
    {SIGTTOU, "TTOU"},
#endif
#ifdef SIGIO
    {SIGIO, "IO"},
#endif
#ifdef SIGXCPU
    {SIGXCPU, "XCPU"},
#endif
#ifdef SIGXFSZ
    {SIGXFSZ, "XFSZ"},
#endif
#ifdef SIGVTALRM
    {SIGVTALRM, "VTALRM"},
#endif
#ifdef SIGPROF
    {SIGPROF, "PROF"},
#endif
#ifdef SIGWINCH
    {SIGWINCH, "WINCH"},
#endif
#ifdef SIGINFO
    {SIGINFO, "INFO"},
#endif
#ifdef SIGUSR1
    {SIGUSR1, "USR1"},
#endif
#ifdef SIGUSR2
    {SIGUSR2, "USR2"},
#endif
    {0, NULL}};

static volatile sig_atomic_t g_pending_traps[TIDESH_MAX_SIGNALS] = {0};

static void tidesh_signal_handler(int signo) {
    if (signo > 0 && signo < TIDESH_MAX_SIGNALS) {
        g_pending_traps[signo] = 1;
    }
}

int trap_condition_to_signo(const char *cond) {
    if (!cond || !*cond) {
        return -1;
    }
    if (strcasecmp(cond, "EXIT") == 0 || strcmp(cond, "0") == 0) {
        return 0;
    }
    bool all_digits = true;
    for (const char *p = cond; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            all_digits = false;
            break;
        }
    }
    if (all_digits) {
        int n = atoi(cond);
        if (n == 0) {
            return 0;
        }
        if (n > 0 && n < TIDESH_MAX_SIGNALS) {
            return n;
        }
        return -1;
    }

    const char *name = cond;
    if (strncasecmp(name, "SIG", 3) == 0) {
        name += 3;
    }
#ifdef SIGABRT
    if (strcasecmp(name, "IOT") == 0) {
        return SIGABRT;
    }
#endif
#ifdef SIGCHLD
    if (strcasecmp(name, "CLD") == 0) {
        return SIGCHLD;
    }
#endif
    for (int i = 0; g_signals[i].name != NULL; i++) {
        if (strcasecmp(name, g_signals[i].name) == 0) {
            return g_signals[i].signo;
        }
    }
    return -1;
}

const char *trap_signo_to_name(int signo) {
    if (signo == 0) {
        return "EXIT";
    }
    for (int i = 0; g_signals[i].name != NULL; i++) {
        if (g_signals[i].signo == signo) {
            return g_signals[i].name;
        }
    }
    return NULL;
}

static void print_single_trap(const char *action, const char *cond) {
    printf("trap -- '");
    for (const char *p = action; *p; p++) {
        if (*p == '\'') {
            fputs("'\\''", stdout);
        } else {
            putchar(*p);
        }
    }
    printf("' %s\n", cond);
}

static void print_all_traps(Session *session) {
    if (session->trap_exit) {
        print_single_trap(session->trap_exit, "EXIT");
    }
    for (int signo = 1; signo < TIDESH_MAX_SIGNALS; signo++) {
        if (session->trap_signals[signo] != NULL) {
            const char *name = trap_signo_to_name(signo);
            char        num_buf[16];
            if (!name) {
                snprintf(num_buf, sizeof(num_buf), "%d", signo);
                name = num_buf;
            }
            print_single_trap(session->trap_signals[signo], name);
        }
    }
}

static void print_signal_list(void) {
    int col = 0;
    for (int i = 0; g_signals[i].name != NULL; i++) {
        printf("%2d) SIG%-8s", g_signals[i].signo, g_signals[i].name);
        col++;
        if (col == 4) {
            printf("\n");
            col = 0;
        } else {
            printf(" ");
        }
    }
    if (col != 0) {
        printf("\n");
    }
}

void check_pending_traps(Session *session) {
    if (!session || session->in_trap) {
        return;
    }
    for (int signo = 1; signo < TIDESH_MAX_SIGNALS; signo++) {
        if (g_pending_traps[signo]) {
            g_pending_traps[signo] = 0;
            char *action           = session->trap_signals[signo];
            if (action && *action) {
                session->in_trap = true;
                int saved_status = environ_get_exit_status(session->environ);
                execute_string(action, session);
                if (!session->exit_requested && !session->return_requested) {
                    environ_set_exit_status(session->environ, saved_status);
                }
                session->in_trap = false;
            }
        }
    }
}

void session_run_exit_trap(Session *session) {
    if (!session || !session->trap_exit) {
        return;
    }
    if (session->is_subshell && !session->trap_exit_local) {
        return;
    }
    char *action       = session->trap_exit;
    session->trap_exit = NULL; // Prevent recursion if action calls exit
    if (*action) {
        session->in_trap = true;
        execute_string(action, session);
        session->in_trap = false;
    }
    free(action);
}

void traps_reset_for_subshell(Session *session) {
    if (!session) {
        return;
    }
    session->is_subshell     = true;
    session->trap_exit_local = false;
    for (int i = 1; i < TIDESH_MAX_SIGNALS; i++) {
        if (session->trap_signals[i]) {
            if (session->trap_signals[i][0] != '\0') {
                void (*orig)(int) = session->original_handlers[i];
                signal(i, orig ? orig : SIG_DFL);
            } else {
                signal(i, SIG_IGN);
            }
        }
    }
}

int builtin_trap(int argc, char **argv, Session *session) {
    if (!session) {
        return 1;
    }
    if (argc == 1) {
        print_all_traps(session);
        return 0;
    }

    int  opt_idx    = 1;
    bool mode_print = false;
    bool mode_list  = false;

    // Check option flags
    while (opt_idx < argc && argv[opt_idx][0] == '-' &&
           argv[opt_idx][1] != '\0') {
        if (strcmp(argv[opt_idx], "--") == 0) {
            opt_idx++;
            break;
        }
        if (strcmp(argv[opt_idx], "-") == 0) {
            break;
        }
        for (size_t j = 1; argv[opt_idx][j]; j++) {
            char opt = argv[opt_idx][j];
            if (opt == 'p') {
                mode_print = true;
            } else if (opt == 'l') {
                mode_list = true;
            } else {
                fprintf(stderr, "tidesh: trap: -%c: invalid option\n", opt);
                fprintf(stderr,
                        "trap: usage: trap [-lp] [[action] condition ...]\n");
                return 2;
            }
        }
        opt_idx++;
    }

    if (mode_list) {
        print_signal_list();
        return 0;
    }

    if (mode_print) {
        if (opt_idx >= argc) {
            print_all_traps(session);
            return 0;
        }
        int ret = 0;
        for (int i = opt_idx; i < argc; i++) {
            int signo = trap_condition_to_signo(argv[i]);
            if (signo < 0) {
                fprintf(stderr,
                        "tidesh: trap: %s: invalid signal specification\n",
                        argv[i]);
                ret = 1;
                continue;
            }
            if (signo == 0) {
                if (session->trap_exit) {
                    print_single_trap(session->trap_exit, "EXIT");
                }
            } else {
                if (session->trap_signals[signo] != NULL) {
                    const char *name = trap_signo_to_name(signo);
                    char        num_buf[16];
                    if (!name) {
                        snprintf(num_buf, sizeof(num_buf), "%d", signo);
                        name = num_buf;
                    }
                    print_single_trap(session->trap_signals[signo], name);
                }
            }
        }
        return ret;
    }

    if (opt_idx >= argc) {
        print_all_traps(session);
        return 0;
    }

    // Check if the first operand is a decimal integer:
    // "If the first operand is an unsigned decimal integer, the actions for all
    // of the operands shall be set to the default condition."
    bool first_is_digits = (*argv[opt_idx] != '\0');
    for (const char *p = argv[opt_idx]; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            first_is_digits = false;
            break;
        }
    }

    const char *action     = NULL;
    int         cond_start = opt_idx;
    if (first_is_digits) {
        action     = "-";
        cond_start = opt_idx;
    } else {
        action     = argv[opt_idx];
        cond_start = opt_idx + 1;
        if (cond_start >= argc) {
            fprintf(stderr,
                    "trap: usage: trap [-lp] [[action] condition ...]\n");
            return 1;
        }
    }

    int ret = 0;
    for (int i = cond_start; i < argc; i++) {
        const char *cond  = argv[i];
        int         signo = trap_condition_to_signo(cond);
        if (signo < 0) {
            fprintf(stderr, "tidesh: trap: %s: invalid signal specification\n",
                    cond);
            ret = 1;
            continue;
        }
        if (signo == SIGKILL || signo == SIGSTOP) {
            fprintf(stderr, "tidesh: trap: %s: cannot trap or ignore\n", cond);
            ret = 1;
            continue;
        }

        if (signo == 0) {
            if (session->trap_exit) {
                free(session->trap_exit);
                session->trap_exit = NULL;
            }
            if (strcmp(action, "-") != 0) {
                session->trap_exit       = strdup(action);
                session->trap_exit_local = true;
            } else {
                session->trap_exit_local = false;
            }
        } else {
            if (strcmp(action, "-") == 0) {
                if (session->trap_signals[signo]) {
                    free(session->trap_signals[signo]);
                    session->trap_signals[signo] = NULL;
                }
                void (*orig)(int) = session->original_handlers[signo];
                signal(signo, orig ? orig : SIG_DFL);
            } else if (strcmp(action, "") == 0) {
                if (!session->has_orig_handler[signo]) {
                    struct sigaction sa;
                    sigaction(signo, NULL, &sa);
                    session->original_handlers[signo] = sa.sa_handler;
                    session->has_orig_handler[signo]  = true;
                }
                if (session->trap_signals[signo]) {
                    free(session->trap_signals[signo]);
                }
                session->trap_signals[signo] = strdup("");
                signal(signo, SIG_IGN);
            } else {
                if (!session->has_orig_handler[signo]) {
                    struct sigaction sa;
                    sigaction(signo, NULL, &sa);
                    session->original_handlers[signo] = sa.sa_handler;
                    session->has_orig_handler[signo]  = true;
                }
                if (session->trap_signals[signo]) {
                    free(session->trap_signals[signo]);
                }
                session->trap_signals[signo] = strdup(action);
                struct sigaction sa;
                sa.sa_handler = tidesh_signal_handler;
                sigemptyset(&sa.sa_mask);
                sa.sa_flags = SA_RESTART;
                sigaction(signo, &sa, NULL);
            }
        }
    }

    return ret;
}
