#include <ctype.h>    /* isspace */
#include <errno.h>    /* errno */
#include <fcntl.h>    /* open, O_WRONLY, O_CREAT, O_APPEND, O_TRUNC, O_RDONLY */
#include <limits.h>   /* PATH_MAX */
#include <signal.h>   /* signal, SIGINT, SIGQUIT, SIG_DFL */
#include <stdio.h>    /* fprintf, stderr, printf, perror, fflush, stdout */
#include <stdlib.h>   /* malloc, free, realloc, strdup, calloc, exit */
#include <string.h>   /* strcmp, strchr, strlen, strncpy, strtok, snprintf */
#include <sys/wait.h> /* waitpid, WEXITSTATUS */
#include <unistd.h> /* fork, access, X_OK, dup2, close, write, execve, pipe, STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO, read */

#include "ast.h"        /* ASTNode, NODE_*, parse, free_ast */
#include "builtin.h"    /* is_special_builtin, get_builtin, is_builtin */
#include "data/array.h" /* Array, free_array, init_array, array_add */
#include "data/trie.h"  /* trie_get */
#include "environ.h" /* environ_get, environ_set, environ_set_exit_status, environ_set_last_arg, environ_set_background_pid, environ_to_array */
#include "execute.h" /* execute, execute_string, execute_string_stdout, find_in_path, get_command_info, CommandInfo, COMMAND_* */
#include "expand.h"  /* full_expansion */
#include "jobs.h"    /* jobs_add, jobs_update */
#include "session.h" /* Session */

#define RW_R__R__ 0644

/* Build a command string from argv for display */
static char *build_command_string(char **argv, int argc) {
    if (!argv || argc == 0) {
        return NULL;
    }

    size_t total_len = 0;
    for (int i = 0; i < argc; i++) {
        total_len += strlen(argv[i]) + 1; // +1 for space or null terminator
    }

    char *cmd = malloc(total_len);
    if (!cmd) {
        return NULL;
    }

    cmd[0]         = '\0';
    size_t current = 0;
    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            cmd[current++] = ' ';
        }
        strcpy(cmd + current, argv[i]);
        current += strlen(argv[i]);
    }

    return cmd;
}

/* Trim leading and trailing whitespace from a string */
static char *trim_whitespace(const char *str) {
    if (!str) {
        return NULL;
    }

    // Find the first non-whitespace character
    const char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    // If the entire string is whitespace, return empty string
    if (*start == '\0') {
        return strdup("");
    }

    // Find the last non-whitespace character
    const char *end = start;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }

    // Create a new string with the trimmed content
    size_t len    = end - start;
    char  *result = malloc(len + 1);
    if (result) {
        strncpy(result, start, len);
        result[len] = '\0';
    }
    return result;
}

char *find_in_path(const char *cmd, Session *session) {
    // If command contains a slash, treat it as a path
    if (strchr(cmd, '/'))
        return strdup(cmd);

    // Search in PATH environment variable
    char       *path_env = environ_get(session->environ, "PATH");
    static char buffer[PATH_MAX];
    if (path_env) {
        char *path_dup = strdup(path_env);
        char *dir      = strtok(path_dup, ":");
        while (dir) {
            snprintf(buffer, sizeof(buffer), "%s/%s", dir, cmd);
            // Check if the file is executable
            if (access(buffer, X_OK) == 0) {
                free(path_dup);
                return strdup(buffer);
            }
            // Next directory
            dir = strtok(NULL, ":");
        }
        free(path_dup);
    }

    // Fallback to common paths
    const char *defaults[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
    for (int i = 0; defaults[i]; i++) {
        snprintf(buffer, sizeof(buffer), "%s/%s", defaults[i], cmd);
        if (access(buffer, X_OK) == 0)
            return strdup(buffer);
    }
    return NULL;
}

CommandInfo get_command_info(const char *cmd, Session *session) {
    CommandInfo info = {COMMAND_NOT_FOUND, NULL};

    // 1. Alias (if enabled)
#ifndef TIDESH_DISABLE_ALIASES
    if (session->features.alias_expansion) {
        char *alias_val = trie_get(session->aliases, (char *)cmd);
        if (alias_val) {
            info.type = COMMAND_ALIAS;
            info.path = strdup(alias_val);
            return info;
        }
    }
#endif

    // 2. Special Builtin
    if (is_special_builtin(cmd)) {
        info.type = COMMAND_SPECIAL_BUILTIN;
        info.path = NULL;
        return info;
    }

    // 3. Regular Builtin
    if (is_builtin(cmd)) {
        info.type = COMMAND_BUILTIN;
        info.path = NULL;
        return info;
    }

    // 4. External Command
    char *path = find_in_path(cmd, session);
    if (path) {
        info.type = COMMAND_EXTERNAL;
        info.path = path;
        return info;
    }

    return info;
}

/* Forward declaration */
int execute(ASTNode *node, Session *session);

/* Handle combined output redirection and process substitution */
static int handle_redirections(ASTNode *node, Session *session) {
    Redirection *redirect = node->redirects;
    while (redirect) {
        int fd_file = -1;
        if (redirect->type == TOKEN_HEREDOC ||
            redirect->type == TOKEN_HERESTRING) {
            int pipe_fds[2];
            if (pipe(pipe_fds) == 0) {
                if (fork() == 0) {
                    signal(SIGINT, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    close(pipe_fds[0]);
                    write(pipe_fds[1], redirect->target,
                          strlen(redirect->target));
                    close(pipe_fds[1]);
                    exit(0);
                }
                close(pipe_fds[1]);
                fd_file = pipe_fds[0];
            }
        } else if (redirect->is_process_substitution) {
            // Process substitution
            bool  is_in = (redirect->type == TOKEN_REDIRECT_IN ||
                          redirect->type == TOKEN_PROCESS_SUBSTITUTION_IN);
            char *cmd   = redirect->target;

            int pipe_fds[2];
            if (pipe(pipe_fds) == 0) {
                if (fork() == 0) {
                    signal(SIGINT, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    if (is_in) {
                        close(pipe_fds[0]);
                        dup2(pipe_fds[1], STDOUT_FILENO);
                        close(pipe_fds[1]);
                    } else {
                        close(pipe_fds[1]);
                        dup2(pipe_fds[0], STDIN_FILENO);
                        close(pipe_fds[0]);
                    }
                    exit(execute_string(cmd, session));
                }
                if (is_in) {
                    close(pipe_fds[1]);
                    fd_file = pipe_fds[0];
                } else {
                    close(pipe_fds[0]);
                    fd_file = pipe_fds[1];
                }
            }
        } else {
            int flags = O_WRONLY | O_CREAT;
            if (redirect->type == TOKEN_REDIRECT_APPEND)
                flags |= O_APPEND;
            else if (redirect->type == TOKEN_REDIRECT_OUT ||
                     redirect->type == TOKEN_REDIRECT_OUT_ERR)
                flags |= O_TRUNC;
            else if (redirect->type == TOKEN_REDIRECT_IN)
                flags = O_RDONLY;

            fd_file = open(redirect->target, flags, RW_R__R__);
        }

        if (fd_file < 0) {
            fprintf(stderr, "Error opening redirection target: %s\n",
                    strerror(errno));
            return -1;
        }

        dup2(fd_file, redirect->fd);
        if (redirect->type == TOKEN_REDIRECT_OUT_ERR) {
            dup2(fd_file, STDERR_FILENO);
        }
        close(fd_file);
        redirect = redirect->next;
    }
    return 0;
}

int execute(ASTNode *node, Session *session) {
    if (!node)
        return 0;

    fflush(stdout);
    fflush(stderr);

    if (node->type == NODE_PIPE) {
        int fds[2];
        if (pipe(fds) < 0)
            return 1;
        pid_t left = fork();
        if (left == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            close(fds[0]);
            dup2(fds[1], STDOUT_FILENO);
            close(fds[1]);
            exit(execute(node->left, session));
        }
        pid_t right = fork();
        if (right == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            close(fds[1]);
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);
            exit(execute(node->right, session));
        }
        close(fds[0]);
        close(fds[1]);
        int st;
        waitpid(left, NULL, 0);
        waitpid(right, &st, 0);
        int exit_status = WEXITSTATUS(st);
        environ_set_exit_status(session->environ, exit_status);
        return exit_status;
    }
    if (node->type == NODE_SEQUENCE) {
        execute(node->left, session);
        return execute(node->right, session);
    }
    if (node->type == NODE_AND) {
        int st = execute(node->left, session);
        if (st == 0)
            return execute(node->right, session);
        environ_set_exit_status(session->environ, st);
        return st;
    }
    if (node->type == NODE_OR) {
        int st = execute(node->left, session);
        if (st != 0)
            return execute(node->right, session);
        environ_set_exit_status(session->environ, st);
        return st;
    }
    if (node->type == NODE_SUBSHELL) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            exit(execute(node->left, session));
        }
        int st;
        waitpid(pid, &st, 0);
        int exit_status = WEXITSTATUS(st);
        environ_set_exit_status(session->environ, exit_status);
        return exit_status;
    }

    if (node->type == NODE_COMMAND) {
        // Expand arguments
        int    argc       = 0;
        char **argv       = NULL;
        int   *arg_is_sub = NULL;

        for (int i = 0; i < node->argc; i++) {
            if (node->arg_is_sub && node->arg_is_sub[i] != 0) {
                argc++;
                argv           = realloc(argv, (argc + 1) * sizeof(char *));
                arg_is_sub     = realloc(arg_is_sub, argc * sizeof(int));
                argv[argc - 1] = strdup(node->argv[i]);
                arg_is_sub[argc - 1] = node->arg_is_sub[i];
                argv[argc]           = NULL;
            } else {
                Array *expansion = full_expansion(node->argv[i], session);
                if (expansion) {
                    for (size_t j = 0; j < expansion->count; j++) {
                        argc++;
                        argv       = realloc(argv, (argc + 1) * sizeof(char *));
                        arg_is_sub = realloc(arg_is_sub, argc * sizeof(int));
                        argv[argc - 1]       = strdup(expansion->items[j]);
                        arg_is_sub[argc - 1] = 0;
                        argv[argc]           = NULL;
                    }
                    free_array(expansion);
                    free(expansion);
                }
            }
        }

        // Handle variable assignments without command
        if (argc == 0 && node->assignments) {
            for (size_t i = 0; i < node->assignments->count; i++) {
                char *copy = strdup(node->assignments->items[i]);
                char *eq   = strchr(copy, '=');
                if (eq) {
                    *eq = 0;
                    environ_set(session->environ, copy, eq + 1);
                }
                free(copy);
            }
            if (argv)
                free(argv);
            if (arg_is_sub)
                free(arg_is_sub);
            environ_set_exit_status(session->environ, 0);
            return 0;
        }

        if (!argv || !argv[0]) {
            if (argv)
                free(argv);
            if (arg_is_sub)
                free(arg_is_sub);
            environ_set_exit_status(session->environ, 0);
            return 0;
        }

        const char *cmd_name_raw     = argv[0];
        char       *cmd_name_trimmed = trim_whitespace(cmd_name_raw);
        const char *cmd_name =
            cmd_name_trimmed ? cmd_name_trimmed : cmd_name_raw;
        environ_set_last_arg(session->environ, argv[argc - 1]);

        // Special builtins should be executed in the main process
        if (is_special_builtin(cmd_name)) {
            int (*builtin_func)(int, char **, Session *) =
                get_builtin(cmd_name);
            if (builtin_func) {
                int st = builtin_func(argc, argv, session);
                for (int i = 0; i < argc; i++)
                    free(argv[i]);
                free(argv);
                free(arg_is_sub);
                if (cmd_name_trimmed)
                    free(cmd_name_trimmed);
                environ_set_exit_status(session->environ, st);
                return st;
            }
        }

        pid_t pid = fork();

        if (pid == 0) {
            /* Child Process */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            // Handle process substitution in argv
            for (int i = 0; i < argc; i++) {
                if (arg_is_sub && arg_is_sub[i] != 0) {
                    bool  is_in = (arg_is_sub[i] == 1);
                    char *cmd   = argv[i];

                    int pipe_fds[2];
                    if (pipe(pipe_fds) == 0) {
                        if (fork() == 0) {
                            signal(SIGINT, SIG_DFL);
                            signal(SIGQUIT, SIG_DFL);
                            if (is_in) {
                                close(pipe_fds[0]);
                                dup2(pipe_fds[1], STDOUT_FILENO);
                                close(pipe_fds[1]);
                            } else {
                                close(pipe_fds[1]);
                                dup2(pipe_fds[0], STDIN_FILENO);
                                close(pipe_fds[0]);
                            }
                            exit(execute_string(cmd, session));
                        }
                        int fd = -1;
                        if (is_in) {
                            close(pipe_fds[1]);
                            fd = pipe_fds[0];
                        } else {
                            close(pipe_fds[0]);
                            fd = pipe_fds[1];
                        }
                        char buf[32];
                        snprintf(buf, sizeof(buf), "/dev/fd/%d", fd);
                        free(argv[i]);
                        argv[i] = strdup(buf);
                    }
                }
            }

            // Manage redirections
            if (handle_redirections(node, session) < 0) {
                exit(1);
            }

            // Manage temporary Assignments (VAR=VAL cmd)
            // Note: We modify the Session only in the child process
            if (node->assignments) {
                for (size_t i = 0; i < node->assignments->count; i++) {
                    char *assign = strdup(node->assignments->items[i]);
                    char *eq     = strchr(assign, '=');
                    if (eq) {
                        *eq = '\0';
                        environ_set(session->environ, assign, eq + 1);
                    }
                    free(assign);
                }
            }

            // Create environment array
            Array *env_array = environ_to_array(session->environ);
            char **envp      = NULL;
            if (env_array) {
                envp = malloc((env_array->count + 1) * sizeof(char *));
                for (size_t i = 0; i < env_array->count; i++) {
                    envp[i] = env_array->items[i];
                }
                envp[env_array->count] = NULL;
            }

            // Execute
            int (*builtin_func)(int, char **, Session *) =
                get_builtin(cmd_name);

            if (builtin_func) {
                int ret = builtin_func(argc, argv, session);
                exit(ret);
            }

            // Find the command path
            char *path = find_in_path(cmd_name, session);
            if (!path) {
#ifdef PROJECT_NAME
                fprintf(stderr, "%s: command not found: %s\n", PROJECT_NAME,
                        cmd_name);
#else
                fprintf(stderr, "tidesh: command not found: %s\n", cmd_name);
#endif
                exit(127);
            }

            execve(path, argv, envp);

            // If we arrived here, execve failed
            perror("execve");
            if (envp)
                free(envp);
            if (env_array) {
                free_array(env_array);
                free(env_array);
            }
            free(path);
            exit(126);
        }

        /* Parent Process */
        for (int i = 0; i < argc; i++)
            free(argv[i]);
        if (argv)
            free(argv);
        if (arg_is_sub)
            free(arg_is_sub);
        if (cmd_name_trimmed)
            free(cmd_name_trimmed);

        if (node->background) {
#ifdef TIDESH_DISABLE_JOB_CONTROL
            fprintf(stderr,
                    "tidesh: background jobs disabled at compile time\n");
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            return 127;
#else
            if (session->features.job_control) {
                char *cmd_str = build_command_string(argv, argc);
                int job_id = jobs_add(session->jobs, pid, cmd_str, JOB_RUNNING);
                if (cmd_str) {
                    free(cmd_str);
                }
                printf("[%d] %d\n", job_id, pid);
                environ_set_background_pid(session->environ, pid);
                environ_set_exit_status(session->environ, 0);
                return 0;
            } else {
                fprintf(stderr, "tidesh: background jobs not enabled\n");
                kill(pid, SIGTERM);
                waitpid(pid, NULL, 0);
                return 127;
            }
#endif
        } else {
            int status;
            waitpid(pid, &status, 0);
            int exit_status = WEXITSTATUS(status);
            environ_set_exit_status(session->environ, exit_status);
            return exit_status;
        }
    }
    environ_set_exit_status(session->environ, 0);
    return 0;
}

int execute_string(const char *cmd, Session *session) {
    LexerInput lexer_in = {0};
    init_lexer_input(&lexer_in, (char *)cmd, execute_string_stdout, session);

    ASTNode *tree   = parse(&lexer_in, session);
    int      result = 0;
    if (tree) {
        result = execute(tree, session);
        free_ast(tree);
        free(tree);
#ifndef TIDESH_DISABLE_HISTORY
        if (session->features.history) {
            history_append(session->history, cmd);
        }
#endif
    }

    free_lexer_input(&lexer_in);
    return result;
}

/* This function is used to execute commands during command substitution */
char *execute_string_stdout(const char *cmd, Session *session) {
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
        init_lexer_input(&lexer_in, (char *)cmd, execute_string_stdout,
                         session);

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
        waitpid(pid, NULL, 0);
        return NULL;
    }

    ssize_t bytes_read;
    while ((bytes_read =
                read(pipe_fd[0], buffer + length, buf_size - length - 1)) > 0) {
        length += bytes_read;
        // printf("Read %zd bytes, total %zu: '%s'\n", bytes_read, length,
        // buffer);

        // Resize buffer if necessary
        if (length >= buf_size - 1) {
            buf_size *= 2;
            char *new_buf = realloc(buffer, buf_size);
            if (!new_buf) {
                free(buffer);
                close(pipe_fd[0]);
                waitpid(pid, NULL, 0);
                return NULL;
            }
            buffer = new_buf;
        }
    }

    buffer[length] = '\0'; // Null-terminate the string
    close(pipe_fd[0]);

    // Wait for the child process to finish
    int status;
    waitpid(pid, &status, 0);

    // Strip trailing newlines (Standard shell behavior for command

    // substitution)
    while (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
    }

    return buffer;
}
