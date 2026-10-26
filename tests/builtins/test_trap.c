#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtin.h"
#include "builtins/trap.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

describe(trap_builtin) {
    it("should identify trap as builtin and special builtin") {
        assert(is_builtin("trap"));
        assert(is_special_builtin("trap"));
        assertneq(get_builtin("trap"), NULL);
    }

    it("should convert condition strings to signal numbers and vice versa") {
        asserteq(trap_condition_to_signo("EXIT"), 0);
        asserteq(trap_condition_to_signo("exit"), 0);
        asserteq(trap_condition_to_signo("0"), 0);
        asserteq(trap_condition_to_signo("INT"), SIGINT);
        asserteq(trap_condition_to_signo("SIGINT"), SIGINT);
        asserteq(trap_condition_to_signo("sigint"), SIGINT);
        asserteq(trap_condition_to_signo("TERM"), SIGTERM);
        asserteq(trap_condition_to_signo("SIGTERM"), SIGTERM);
        asserteq(trap_condition_to_signo("INVALID_SIGNAL"), -1);
        asserteq(trap_condition_to_signo(""), -1);
        asserteq(trap_condition_to_signo(NULL), -1);

        asserteq_str(trap_signo_to_name(0), "EXIT");
        asserteq_str(trap_signo_to_name(SIGINT), "INT");
        asserteq_str(trap_signo_to_name(SIGTERM), "TERM");
    }

    it("should list signals with -l") {
        Session *session = init_session(NULL, NULL);
        char    *argv[]  = {"trap", "-l", NULL};
        int      res     = builtin_trap(2, argv, session);
        asserteq(res, 0);
        free_session(session);
        free(session);
    }

    it("should reject invalid options with exit status 2") {
        Session *session = init_session(NULL, NULL);
        char    *argv[]  = {"trap", "-z", NULL};
        int      res     = builtin_trap(2, argv, session);
        asserteq(res, 2);
        free_session(session);
        free(session);
    }

    it("should fail when action is given without conditions") {
        Session *session = init_session(NULL, NULL);
        char    *argv[]  = {"trap", "echo hello", NULL};
        int      res     = builtin_trap(2, argv, session);
        asserteq(res, 1);
        free_session(session);
        free(session);
    }

    it("should fail on invalid signal names") {
        Session *session = init_session(NULL, NULL);
        char    *argv[]  = {"trap", "echo hello", "NONEXISTENT_SIG", NULL};
        int      res     = builtin_trap(3, argv, session);
        asserteq(res, 1);
        free_session(session);
        free(session);
    }

    it("should reject trapping or ignoring SIGKILL and SIGSTOP") {
        Session *session = init_session(NULL, NULL);

        char *argv_kill[] = {"trap", "echo hello", "KILL", NULL};
        asserteq(builtin_trap(3, argv_kill, session), 1);

        char *argv_kill_num[] = {"trap", "echo hello", "9", NULL};
        asserteq(builtin_trap(3, argv_kill_num, session), 1);

        char *argv_stop[] = {"trap", "echo hello", "STOP", NULL};
        asserteq(builtin_trap(3, argv_stop, session), 1);

        free_session(session);
        free(session);
    }

    it("should set, query, and clear EXIT trap") {
        Session *session = init_session(NULL, NULL);

        // Set EXIT trap
        char *argv_set[] = {"trap", "echo goodbye", "EXIT", NULL};
        asserteq(builtin_trap(3, argv_set, session), 0);
        assertneq(session->trap_exit, NULL);
        asserteq_str(session->trap_exit, "echo goodbye");

        // Print via -p
        char *argv_p[] = {"trap", "-p", "EXIT", NULL};
        asserteq(builtin_trap(3, argv_p, session), 0);

        // Reset EXIT trap with '-'
        char *argv_reset[] = {"trap", "-", "EXIT", NULL};
        asserteq(builtin_trap(3, argv_reset, session), 0);
        asserteq(session->trap_exit, NULL);

        // Reset via numeric 0
        char *argv_set0[] = {"trap", "echo zero", "0", NULL};
        asserteq(builtin_trap(3, argv_set0, session), 0);
        assertneq(session->trap_exit, NULL);

        char *argv_reset0[] = {"trap", "0", NULL};
        asserteq(builtin_trap(2, argv_reset0, session), 0);
        asserteq(session->trap_exit, NULL);

        free_session(session);
        free(session);
    }

    it("should set, query, ignore, and reset signal traps") {
        Session *session = init_session(NULL, NULL);

        // Set INT trap
        char *argv_set[] = {"trap", "echo caught", "INT", NULL};
        asserteq(builtin_trap(3, argv_set, session), 0);
        assertneq(session->trap_signals[SIGINT], NULL);
        asserteq_str(session->trap_signals[SIGINT], "echo caught");

        // Ignore INT via empty string
        char *argv_ign[] = {"trap", "", "INT", NULL};
        asserteq(builtin_trap(3, argv_ign, session), 0);
        assertneq(session->trap_signals[SIGINT], NULL);
        asserteq_str(session->trap_signals[SIGINT], "");

        // Reset INT via '-'
        char *argv_reset[] = {"trap", "-", "INT", NULL};
        asserteq(builtin_trap(3, argv_reset, session), 0);
        asserteq(session->trap_signals[SIGINT], NULL);

        free_session(session);
        free(session);
    }

    it("should support numeric conditions for default action") {
        Session *session = init_session(NULL, NULL);

        // Set USR1 trap
        char *argv_set[] = {"trap", "echo usr1", "USR1", NULL};
        asserteq(builtin_trap(3, argv_set, session), 0);
        assertneq(session->trap_signals[SIGUSR1], NULL);

        // Reset using numeric argument
        char num_buf[16];
        snprintf(num_buf, sizeof(num_buf), "%d", SIGUSR1);
        char *argv_num[] = {"trap", num_buf, NULL};
        asserteq(builtin_trap(2, argv_num, session), 0);
        asserteq(session->trap_signals[SIGUSR1], NULL);

        free_session(session);
        free(session);
    }

    it("should execute EXIT trap on session termination") {
        Session *session = init_session(NULL, NULL);

        execute_string("trap 'TRAP_EXIT_DONE=yes' EXIT", session);
        assertneq(session->trap_exit, NULL);

        session_run_exit_trap(session);
        asserteq_str(environ_get(session->environ, "TRAP_EXIT_DONE"), "yes");
        asserteq(session->trap_exit, NULL);

        free_session(session);
        free(session);
    }

    it("should execute signal trap upon receiving signal") {
        Session *session = init_session(NULL, NULL);

        execute_string("trap 'SIG_CAUGHT=1' USR1", session);
        asserteq(environ_get(session->environ, "SIG_CAUGHT"), NULL);

        // Send signal to ourselves
        kill(getpid(), SIGUSR1);

        // Executing next command triggers check_pending_traps
        execute_string("true", session);
        asserteq_str(environ_get(session->environ, "SIG_CAUGHT"), "1");

        // Clean up trap
        execute_string("trap - USR1", session);

        free_session(session);
        free(session);
    }

    it("should execute EXIT trap inside subshell independently") {
        const char *out_path = "/tmp/test_trap_subshell.txt";
        unlink(out_path);

        Session *session = init_session(NULL, NULL);
        char     cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "( trap 'echo subshell_exit >> %s' EXIT; echo inside >> %s )",
                 out_path, out_path);
        int st = execute_string(cmd, session);
        asserteq(st, 0);

        FILE *f = fopen(out_path, "r");
        assertneq(f, NULL);
        char buf[256] = {0};
        fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        unlink(out_path);

        asserteq_str(buf, "inside\nsubshell_exit\n");

        free_session(session);
        free(session);
    }

    it("should preserve exit status across signal trap execution") {
        Session *session = init_session(NULL, NULL);

        execute_string("trap 'TRAP_RAN=1' USR2", session);

        // Force exit status to 42
        environ_set_exit_status(session->environ, 42);

        // Send signal
        kill(getpid(), SIGUSR2);

        // Check pending traps
        check_pending_traps(session);

        asserteq_str(environ_get(session->environ, "TRAP_RAN"), "1");
        asserteq(environ_get_exit_status(session->environ), 42);

        execute_string("trap - USR2", session);
        free_session(session);
        free(session);
    }
}
