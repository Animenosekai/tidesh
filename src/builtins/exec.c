/** exec.c
 *
 * Implementation of the exec builtin command.
 */

#include <errno.h>    /* errno */
#include <stdbool.h>  /* bool, true, false */
#include <stdio.h>    /* fprintf, stderr, perror */
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* strcmp, strdup, strlen, strchr, strrchr, memcpy */
#include <sys/stat.h> /* stat, S_ISDIR */
#include <unistd.h>   /* access, execve, X_OK, F_OK */

#include "builtins/exec.h"
#include "data/array.h" /* Array, free_array */
#include "environ.h"    /* environ_to_array */
#include "execute.h"    /* find_in_path, parse_shebang */
#include "session.h"    /* Session */

int builtin_exec(int argc, char **argv, Session *session) {
    if (!session || argc < 1) {
        return 1;
    }

    const char *argv0_name  = NULL;
    bool        clear_env   = false;
    bool        login_shell = false;
    int         cmd_idx     = argc;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            cmd_idx = i + 1;
            break;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            bool option_done = false;
            for (size_t j = 1; arg[j]; j++) {
                char opt = arg[j];
                if (opt == 'c') {
                    clear_env = true;
                } else if (opt == 'l') {
                    login_shell = true;
                } else if (opt == 'a') {
                    if (arg[j + 1] != '\0') {
                        argv0_name  = &arg[j + 1];
                        option_done = true;
                        break;
                    } else if (i + 1 < argc) {
                        i++;
                        argv0_name  = argv[i];
                        option_done = true;
                        break;
                    } else {
                        fprintf(stderr, "tidesh: exec: -a: option requires an "
                                        "argument\n");
                        fprintf(stderr,
                                "exec: usage: exec [-cl] [-a name] [command "
                                "[argument ...]]\n");
                        return 2;
                    }
                } else {
                    fprintf(stderr, "tidesh: exec: -%c: invalid option\n", opt);
                    fprintf(stderr,
                            "exec: usage: exec [-cl] [-a name] [command "
                            "[argument ...]]\n");
                    return 2;
                }
            }
            if (option_done) {
                continue;
            }
        } else {
            cmd_idx = i;
            break;
        }
    }

    // No command provided: redirections take effect permanently in current
    // shell
    if (cmd_idx >= argc) {
        session->permanent_redirections = true;
        return 0;
    }

    const char *cmd = argv[cmd_idx];
    if (*cmd == '\0') {
        fprintf(stderr, "tidesh: exec: : not found\n");
        session->exit_requested = true;
        return 127;
    }

    // Resolve path and check permissions
    char *path = NULL;
    if (strchr(cmd, '/')) {
        struct stat st;
        if (stat(cmd, &st) != 0) {
            fprintf(stderr, "tidesh: exec: %s: not found\n", cmd);
            session->exit_requested = true;
            return 127;
        }
        if (S_ISDIR(st.st_mode)) {
            fprintf(stderr, "tidesh: exec: %s: is a directory\n", cmd);
            session->exit_requested = true;
            return 126;
        }
        if (access(cmd, X_OK) != 0) {
            fprintf(stderr, "tidesh: exec: %s: permission denied\n", cmd);
            session->exit_requested = true;
            return 126;
        }
        path = strdup(cmd);
    } else {
        path = find_in_path(cmd, session);
        if (!path) {
            fprintf(stderr, "tidesh: exec: %s: not found\n", cmd);
            session->exit_requested = true;
            return 127;
        }
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            fprintf(stderr, "tidesh: exec: %s: is a directory\n", cmd);
            free(path);
            session->exit_requested = true;
            return 126;
        }
    }

    if (!path) {
        session->exit_requested = true;
        return 127;
    }

    // Determine argv[0] to pass to the command
    const char *base = argv0_name;
    if (!base) {
        if (login_shell) {
            char *slash = strrchr(cmd, '/');
            base        = slash ? slash + 1 : cmd;
        } else {
            base = cmd;
        }
    }

    char *final_argv0 = NULL;
    if (login_shell) {
        size_t len  = strlen(base);
        final_argv0 = malloc(len + 2);
        if (!final_argv0) {
            free(path);
            return 1;
        }
        final_argv0[0] = '-';
        memcpy(final_argv0 + 1, base, len + 1);
    } else {
        final_argv0 = strdup(base);
    }

    // Build command argument list
    int    cmd_argc = argc - cmd_idx;
    char **cmd_argv = malloc((cmd_argc + 1) * sizeof(char *));
    if (!cmd_argv) {
        free(final_argv0);
        free(path);
        return 1;
    }
    cmd_argv[0] = final_argv0;
    for (int i = 1; i < cmd_argc; i++) {
        cmd_argv[i] = strdup(argv[cmd_idx + i]);
    }
    cmd_argv[cmd_argc] = NULL;

    // Prepare environment
    char **envp      = NULL;
    Array *env_array = NULL;
    if (clear_env) {
        envp = malloc(sizeof(char *));
        if (envp) {
            envp[0] = NULL;
        }
    } else {
        env_array = environ_to_array(session->environ);
        if (env_array) {
            envp = malloc((env_array->count + 1) * sizeof(char *));
            if (envp) {
                for (size_t i = 0; i < env_array->count; i++) {
                    envp[i] = env_array->items[i];
                }
                envp[env_array->count] = NULL;
            }
        }
    }

    // Check for shebang
    int    interp_argc = 0;
    char **interp_argv = NULL;
    if (parse_shebang(path, &interp_argc, &interp_argv)) {
        int    total_argc   = interp_argc + cmd_argc;
        char **shebang_argv = malloc((total_argc + 1) * sizeof(char *));
        int    idx          = 0;
        for (int i = 0; i < interp_argc; i++) {
            shebang_argv[idx++] = interp_argv[i];
        }
        shebang_argv[idx++] = strdup(path);
        for (int i = 1; i < cmd_argc; i++) {
            shebang_argv[idx++] = strdup(cmd_argv[i]);
        }
        shebang_argv[idx] = NULL;

        execve(interp_argv[0], shebang_argv, envp);

        perror("execve");
        for (int i = 0; i < idx; i++) {
            free(shebang_argv[i]);
        }
        free(shebang_argv);
        free(interp_argv);
    } else {
        execve(path, cmd_argv, envp);
        perror("execve");
    }

    // If execve returns, it failed
    for (int i = 0; i < cmd_argc; i++) {
        free(cmd_argv[i]);
    }
    free(cmd_argv);
    free(path);
    if (envp) {
        free(envp);
    }
    if (env_array) {
        free_array(env_array);
        free(env_array);
    }

    session->exit_requested = true;
    return 126;
}
