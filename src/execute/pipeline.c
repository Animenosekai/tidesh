#include <errno.h>    /* errno, EINTR */
#include <signal.h>   /* signal, SIGINT, SIGQUIT, SIG_DFL */
#include <stdio.h>    /* fprintf, stderr, snprintf */
#include <stdlib.h>   /* exit */
#include <sys/wait.h> /* waitpid, WEXITSTATUS, WIFSIGNALED, WTERMSIG */
#include <unistd.h>   /* fork, pipe, close, dup2, STDOUT_FILENO, STDIN_FILENO */

#include "ast.h"
#include "builtins/trap.h"
#include "environ.h"
#include "execute.h"
#include "execute/pipeline.h"
#include "hooks.h"
#include "session.h"

int execute_pipe(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_PIPES
    if (!session->features.pipes) {
        fprintf(stderr, "tidesh: pipes are disabled\n");
        return 127;
    }
    int fds[2];
    if (pipe(fds) < 0)
        return 1;
    pid_t left = fork();
    if (left == 0) {
        traps_reset_for_subshell(session);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        exit(execute(node->left, session));
    }
    pid_t right = fork();
    if (right == 0) {
        traps_reset_for_subshell(session);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        exit(execute(node->right, session));
    }
    close(fds[0]);
    close(fds[1]);
    int left_st  = 0;
    int right_st = 0;
    while (waitpid(left, &left_st, 0) == -1 && errno == EINTR)
        ;
    while (waitpid(right, &right_st, 0) == -1 && errno == EINTR)
        ;
    int exit_status = WEXITSTATUS(right_st);
    if (session->options.pipefail) {
        int right_code = WEXITSTATUS(right_st);
        int left_code  = WEXITSTATUS(left_st);
        if (right_code != 0) {
            exit_status = right_code;
        } else if (left_code != 0) {
            exit_status = left_code;
        }
    }
    environ_set_exit_status(session->environ, exit_status);
    if (session->options.errexit && exit_status != 0 &&
        session->ignore_errexit == 0) {
        session->exit_requested = true;
    }
    return exit_status;
#else
    (void)node;
    (void)session;
    return 127;
#endif
}

int execute_sequence(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_SEQUENCES
    if (!session->features.sequences) {
        fprintf(stderr, "tidesh: sequences are disabled\n");
        return 127;
    }
    int st = execute(node->left, session);
    check_pending_traps(session);
    if (session->loop_break > 0 || session->loop_continue > 0 ||
        session->exit_requested || session->return_requested) {
        return st;
    }
    if (session->options.errexit && st != 0 && session->ignore_errexit == 0) {
        session->exit_requested = true;
        return st;
    }
    return execute(node->right, session);
#else
    (void)node;
    (void)session;
    return 127;
#endif
}

int execute_and(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_SEQUENCES
    if (!session->features.sequences) {
        fprintf(stderr, "tidesh: sequences are disabled\n");
        return 127;
    }
    session->ignore_errexit++;
    int st = execute(node->left, session);
    session->ignore_errexit--;
    if (st == 0 && !session->loop_break && !session->loop_continue &&
        !session->exit_requested && !session->return_requested)
        return execute(node->right, session);
    environ_set_exit_status(session->environ, st);
    if (session->options.errexit && st != 0 && session->ignore_errexit == 0) {
        session->exit_requested = true;
    }
    return st;
#else
    (void)node;
    (void)session;
    return 127;
#endif
}

int execute_or(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_SEQUENCES
    if (!session->features.sequences) {
        fprintf(stderr, "tidesh: sequences are disabled\n");
        return 127;
    }
    session->ignore_errexit++;
    int st = execute(node->left, session);
    session->ignore_errexit--;
    if (st != 0 && !session->loop_break && !session->loop_continue &&
        !session->exit_requested && !session->return_requested)
        return execute(node->right, session);
    environ_set_exit_status(session->environ, st);
    if (session->options.errexit && st != 0 && session->ignore_errexit == 0) {
        session->exit_requested = true;
    }
    return st;
#else
    (void)node;
    (void)session;
    return 127;
#endif
}

int execute_subshell(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_SUBSHELLS
    if (!session->features.subshells) {
        fprintf(stderr, "tidesh: subshells are disabled\n");
        return 127;
    }
    run_cwd_hook(session, HOOK_ENTER_SUBSHELL);
    pid_t pid = fork();
    if (pid == 0) {
        traps_reset_for_subshell(session);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        int ret = execute(node->left, session);
        session_run_exit_trap(session);
        exit(ret);
    }
    int st = 0;
    while (waitpid(pid, &st, 0) == -1 && errno == EINTR)
        ;
    int exit_status = 0;
    if (WIFSIGNALED(st)) {
        exit_status = 128 + WTERMSIG(st);
        char sig_buf[16];
        snprintf(sig_buf, sizeof(sig_buf), "%d", WTERMSIG(st));
        HookEnvVar sig_vars[] = {{"TIDE_SIGNAL", sig_buf}};
        run_cwd_hook_with_vars(session, HOOK_SIGNAL, sig_vars, 1);
    } else {
        exit_status = WEXITSTATUS(st);
    }
    environ_set_exit_status(session->environ, exit_status);
    run_cwd_hook(session, HOOK_EXIT_SUBSHELL);
    return exit_status;
#else
    (void)node;
    (void)session;
    return 127;
#endif
}

int execute_block(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_BLOCKS
    if (!session->features.blocks) {
        fprintf(stderr, "tidesh: blocks are disabled\n");
        return 127;
    }
    return execute(node->left, session);
#else
    (void)node;
    (void)session;
    return 127;
#endif
}
