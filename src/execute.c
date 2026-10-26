#include <errno.h>    /* errno, EINTR */
#include <fcntl.h>    /* open, O_WRONLY */
#include <signal.h>   /* signal, SIGINT, SIGQUIT, SIG_DFL */
#include <stdbool.h>  /* bool, true, false */
#include <stdio.h>    /* fprintf, stderr, fflush, stdout, perror */
#include <stdlib.h>   /* malloc, free, realloc, strdup, exit */
#include <string.h>   /* strcmp */
#include <sys/wait.h> /* waitpid */
#include <unistd.h>   /* fork, pipe, close, dup2, read, STDOUT_FILENO */

#include "ast.h"
#include "builtins/trap.h"
#include "environ.h"
#include "execute.h"
#include "execute/try.h"
#include "history.h"
#include "hooks.h"
#include "lexer.h"
#include "session.h"

int execute(ASTNode *node, Session *session) {
    if (!node)
        return 0;

    check_pending_traps(session);

    fflush(stdout);
    fflush(stderr);

    if (session->exit_requested)
        return environ_get_exit_status(session->environ);

    if (session->return_requested)
        return session->return_status;

    if (session->try_caught)
        return session->try_exit_code;

#ifndef TIDESH_DISABLE_PIPES
    if (node->type == NODE_PIPE) {
        return execute_pipe(node, session);
    }
#endif

#ifndef TIDESH_DISABLE_SEQUENCES
    if (node->type == NODE_SEQUENCE) {
        return execute_sequence(node, session);
    }
    if (node->type == NODE_AND) {
        return execute_and(node, session);
    }
    if (node->type == NODE_OR) {
        return execute_or(node, session);
    }
#endif

#ifndef TIDESH_DISABLE_SUBSHELLS
    if (node->type == NODE_SUBSHELL) {
        return execute_subshell(node, session);
    }
#endif

#ifndef TIDESH_DISABLE_CONDITIONALS
    if (node->type == NODE_CONDITIONAL) {
        return execute_conditional(node, session);
    }
    if (node->type == NODE_CASE) {
        return execute_case(node, session);
    }
#endif

#ifndef TIDESH_DISABLE_BLOCKS
    if (node->type == NODE_BLOCK) {
        return execute_block(node, session);
    }
#endif

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    if (node->type == NODE_WITH) {
        return execute_with(node, session);
    }
#endif

#ifndef TIDESH_DISABLE_FUNCTIONS
    if (node->type == NODE_FUNCTION_DEF) {
        return execute_function_def(node, session);
    }
#endif

#ifndef TIDESH_DISABLE_LOOPS
    if (node->type == NODE_FOR) {
        return execute_for(node, session);
    }
    if (node->type == NODE_WHILE || node->type == NODE_UNTIL) {
        return execute_while_until(node, session);
    }
#endif

    if (node->type == NODE_TRY) {
        return execute_try(node, session);
    }

    if (node->type == NODE_COMMAND) {
        return execute_command(node, session);
    }

    environ_set_exit_status(session->environ, 0);
    return 0;
}

int execute_string(const char *cmd, Session *session) {
    char      *cmd_word   = extract_first_word(cmd);
    HookEnvVar cmd_vars[] = {{"TIDE_CMDLINE", cmd},
                             {"TIDE_CMD", cmd_word ? cmd_word : ""}};
    run_cwd_hook_with_vars(session, HOOK_BEFORE_CMD, cmd_vars, 2);

    LexerInput lexer_in = {0};
    init_lexer_input(&lexer_in, (char *)cmd, NULL, session);

    ASTNode *tree   = parse(&lexer_in, session);
    int      result = 0;
    if (tree) {
        result = execute(tree, session);
        free_ast(tree);
        free(tree);
        if (session->options.errexit && result != 0 &&
            session->ignore_errexit == 0) {
            session->exit_requested = true;
        }
#ifndef TIDESH_DISABLE_HISTORY
        if (session->features.history) {
            history_append(session->history, cmd);
        }
#endif
    } else {
        HookEnvVar syntax_vars[] = {{"TIDE_CMDLINE", cmd},
                                    {"TIDE_CMD", cmd_word ? cmd_word : ""},
                                    {"TIDE_ERROR", "SYNTAX_ERROR"}};
        run_cwd_hook_with_vars(session, HOOK_SYNTAX_ERROR, syntax_vars, 3);
    }

    run_cwd_hook_with_vars(session, HOOK_AFTER_CMD, cmd_vars, 2);
    if (result != 0) {
        HookEnvVar error_vars[] = {{"TIDE_CMDLINE", cmd},
                                   {"TIDE_CMD", cmd_word ? cmd_word : ""},
                                   {"TIDE_ERROR", "CMD_FAIL"},
                                   {"CMD_FAIL", "1"}};
        run_cwd_hook_with_vars(session, HOOK_ERROR, error_vars, 4);
    }

    free(cmd_word);
    free_lexer_input(&lexer_in);
    return result;
}

/* This function is used to execute commands during command substitution */
char *execute_string_stdout(const char *cmd, Session *session) {
    if (!session->features.command_substitution) {
        fprintf(stderr, "tidesh: command substitution is disabled\n");
        return strdup("");
    }

    // Create a pipe to capture stdout
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return NULL;
    }

    fflush(stdout); // Flush existing buffers before forking

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child Process */
        traps_reset_for_subshell(session);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        close(pipe_fd[0]); // Close read end

        // Redirect STDOUT to the write end of the pipe
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(pipe_fd[1]); // Close original write end

        // Execute the command
        // Note: we are not using execute_string to avoid writing to history
        LexerInput lexer_in = {0};
        init_lexer_input(&lexer_in, (char *)cmd, NULL, session);

        ASTNode *tree   = parse(&lexer_in, session);
        int      status = 1;

        if (tree) {
            status = execute(tree, session);
            free_ast(tree);
            free(tree);
        }

        fflush(stdout);

        // Discard any remaining output
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        } else {
            close(STDOUT_FILENO);
        }

        free_lexer_input(&lexer_in);
        // Note: We don't care if the execution was successful or not here
        exit(status);
    }

    /* Parent Process */
    close(pipe_fd[1]); // Close write end immediately so we detect EOF

    // Read output from the pipe
    size_t buf_size = 128;
    size_t length   = 0;
    char  *buffer   = malloc(buf_size);

    if (!buffer) {
        close(pipe_fd[0]);
        while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
            ;
        return NULL;
    }

    ssize_t bytes_read;
    while ((bytes_read =
                read(pipe_fd[0], buffer + length, buf_size - length - 1)) > 0) {
        length += bytes_read;

        // Resize buffer if necessary
        if (length >= buf_size - 1) {
            buf_size *= 2;
            char *new_buf = realloc(buffer, buf_size);
            if (!new_buf) {
                free(buffer);
                close(pipe_fd[0]);
                while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
                    ;
                return NULL;
            }
            buffer = new_buf;
        }
    }

    buffer[length] = '\0'; // Null-terminate the string
    close(pipe_fd[0]);

    // Wait for the child process to finish
    int status = 0;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
        ;

    // Strip trailing newlines (Standard shell behavior for command
    // substitution)
    while (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
    }

    return buffer;
}
