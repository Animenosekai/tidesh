/** command.c
 *
 * Implementation of the command builtin.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtin.h"
#include "builtins/command.h"
#include "builtins/trap.h"
#include "data/array.h"
#include "environ.h"
#include "execute.h"
#include "session.h"

static int execute_external_command(const char *path, int argc, char **argv,
                                    Session *session) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        traps_reset_for_subshell(session);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        Array *env_array = environ_to_array(session->environ);
        char **envp      = NULL;
        if (env_array) {
            envp = malloc((env_array->count + 1) * sizeof(char *));
            if (envp) {
                for (size_t i = 0; i < env_array->count; i++) {
                    envp[i] = env_array->items[i];
                }
                envp[env_array->count] = NULL;
            }
        }

        int    interp_argc = 0;
        char **interp_argv = NULL;
        if (parse_shebang(path, &interp_argc, &interp_argv)) {
            int    new_argc = interp_argc + argc;
            char **new_argv = malloc((new_argc + 1) * sizeof(char *));
            int    idx      = 0;
            for (int i = 0; i < interp_argc; i++) {
                new_argv[idx++] = interp_argv[i];
            }
            new_argv[idx++] = strdup(path);
            for (int i = 1; i < argc; i++) {
                new_argv[idx++] = strdup(argv[i]);
            }
            new_argv[idx] = NULL;

            execve(interp_argv[0], new_argv, envp);
            perror("execve");
            exit(126);
        }

        char **exec_argv = malloc((argc + 1) * sizeof(char *));
        if (exec_argv) {
            for (int i = 0; i < argc; i++) {
                exec_argv[i] = argv[i];
            }
            exec_argv[argc] = NULL;
            execve(path, exec_argv, envp);
        }
        perror("execve");
        exit(126);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            check_pending_traps(session);
            continue;
        }
        break;
    }
    check_pending_traps(session);

    int exit_status = 0;
    if (WIFSIGNALED(status)) {
        exit_status = 128 + WTERMSIG(status);
    } else {
        exit_status = WEXITSTATUS(status);
    }
    environ_set_exit_status(session->environ, exit_status);
    return exit_status;
}

int builtin_command(int argc, char **argv, Session *session) {
    if (!session) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }

    int  opt_idx = 1;
    bool opt_p   = false;
    bool opt_v   = false;
    bool opt_V   = false;

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
                opt_p = true;
            } else if (opt == 'v') {
                opt_v = true;
            } else if (opt == 'V') {
                opt_V = true;
            } else {
                fprintf(stderr, "tidesh: command: -%c: invalid option\n", opt);
                fprintf(stderr,
                        "command: usage: command [-p] command [arg ...] or "
                        "command -v|-V [-p] command ...\n");
                return 2;
            }
        }
        opt_idx++;
    }

    // Handle -v or -V (query mode)
    if (opt_v || opt_V) {
        if (opt_idx >= argc) {
            return 0;
        }
        int exit_status = 0;
        for (int i = opt_idx; i < argc; i++) {
            const char *cmd_name   = argv[i];
            char       *saved_path = NULL;

            if (opt_p) {
                char *curr_path = environ_get(session->environ, "PATH");
                saved_path      = curr_path ? strdup(curr_path) : NULL;
                environ_set(session->environ, "PATH",
                            "/usr/bin:/bin:/usr/sbin:/sbin");
            }

            CommandInfo info = get_command_info(cmd_name, session);

            if (opt_p) {
                if (saved_path) {
                    environ_set(session->environ, "PATH", saved_path);
                    free(saved_path);
                } else {
                    environ_remove(session->environ, "PATH");
                }
            }

            if (opt_v) {
                switch (info.type) {
                    case COMMAND_ALIAS:
                        printf("alias %s='%s'\n", cmd_name, info.path);
                        break;
                    case COMMAND_SPECIAL_BUILTIN:
                    case COMMAND_BUILTIN:
                    case COMMAND_FUNCTION:
                        printf("%s\n", cmd_name);
                        break;
                    case COMMAND_EXTERNAL:
                        printf("%s\n", info.path);
                        break;
                    case COMMAND_NOT_FOUND:
                    default:
                        exit_status = 1;
                        break;
                }
            } else {
                switch (info.type) {
                    case COMMAND_ALIAS:
                        printf("%s is aliased to `%s'\n", cmd_name, info.path);
                        break;
                    case COMMAND_SPECIAL_BUILTIN:
                        printf("%s is a shell special built-in command\n",
                               cmd_name);
                        break;
                    case COMMAND_BUILTIN:
                        printf("%s is a shell built-in command\n", cmd_name);
                        break;
                    case COMMAND_FUNCTION:
                        printf("%s is a shell function\n", cmd_name);
                        break;
                    case COMMAND_EXTERNAL:
                        printf("%s is %s\n", cmd_name, info.path);
                        break;
                    case COMMAND_NOT_FOUND:
                    default:
                        fprintf(stderr, "command: %s: not found\n", cmd_name);
                        exit_status = 1;
                        break;
                }
            }

            if (info.path) {
                free(info.path);
            }
        }
        return exit_status;
    }

    // Execution mode: command [-p] command_name [argument ...]
    if (opt_idx >= argc) {
        return 0;
    }

    const char *target_cmd = argv[opt_idx];
    int         cmd_argc   = argc - opt_idx;
    char      **cmd_argv   = argv + opt_idx;

    // Builtins are checked directly (bypassing functions and aliases)
    int (*builtin_func)(int, char **, Session *) = get_builtin(target_cmd);
    if (builtin_func) {
        int st = builtin_func(cmd_argc, cmd_argv, session);
        environ_set_exit_status(session->environ, st);
        return st;
    }

    // External command
    char *path       = NULL;
    char *saved_path = NULL;
    if (opt_p) {
        char *curr_path = environ_get(session->environ, "PATH");
        saved_path      = curr_path ? strdup(curr_path) : NULL;
        environ_set(session->environ, "PATH", "/usr/bin:/bin:/usr/sbin:/sbin");
    }

    path = find_in_path(target_cmd, session);

    if (opt_p) {
        if (saved_path) {
            environ_set(session->environ, "PATH", saved_path);
            free(saved_path);
        } else {
            environ_remove(session->environ, "PATH");
        }
    }

    if (!path) {
        fprintf(stderr, "tidesh: command: %s: not found\n", target_cmd);
        environ_set_exit_status(session->environ, 127);
        return 127;
    }

    int st = execute_external_command(path, cmd_argc, cmd_argv, session);
    free(path);
    return st;
}
