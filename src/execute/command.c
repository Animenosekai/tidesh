#include <errno.h>    /* errno, EINTR */
#include <signal.h>   /* signal, SIGINT, SIGQUIT, SIG_DFL, SIGTERM, SIGKILL */
#include <stdbool.h>  /* bool, true, false */
#include <stdio.h>    /* fprintf, stderr, printf, fputc, snprintf, perror */
#include <stdlib.h>   /* malloc, free, realloc, strdup, calloc, exit */
#include <string.h>   /* strcmp, strchr, strlen, strcpy */
#include <sys/wait.h> /* waitpid, WEXITSTATUS, WIFSTOPPED, WIFSIGNALED, WSTOPSIG, WTERMSIG, WUNTRACED */
#include <termios.h>  /* tcsetattr, TCSADRAIN */
#include <unistd.h> /* fork, pipe, close, dup2, execve, isatty, STDIN_FILENO, STDOUT_FILENO */

#include "ast.h"
#include "builtin.h"
#include "builtins/trap.h"
#include "data/array.h"
#include "data/dynamic.h"
#include "data/trie.h"
#include "environ.h"
#include "execute.h"
#include "execute/command.h"
#include "execute/function.h"
#include "execute/lookup.h"
#include "execute/redirection.h"
#include "expand.h"
#include "expansions/arithmetic.h"
#include "expansions/tildes.h"
#include "expansions/variables.h"
#include "hooks.h"
#include "jobs.h"
#include "sandbox.h"
#include "session.h"

char *expand_word_no_split(const char *raw_value, Session *session) {
    if (!raw_value) {
        return strdup("");
    }
    char *current = strdup(raw_value);

    // 0. Arithmetic expansion $((...))
    if (session && session->features.arithmetic_expansion) {
        char *after_arith = expand_arithmetic(current, session);
        if (after_arith) {
            free(current);
            current = after_arith;
        }
    }

    // 1. Command substitution $(...)
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
    if (session && session->features.command_substitution) {
        char *after_cmd = expand_command_substitution(current, session);
        if (after_cmd) {
            free(current);
            current = after_cmd;
        }
    }
#endif

    // 1. Variable expansion
    if (session && session->features.variable_expansion) {
        Array *var_arr = variable_expansion(current, session);
        if (var_arr) {
            free(current);
            if (var_arr->count == 0) {
                current = strdup("");
            } else if (var_arr->count == 1) {
                current = strdup(var_arr->items[0]);
            } else {
                Dynamic joined = {0};
                init_dynamic(&joined);
                for (size_t i = 0; i < var_arr->count; i++) {
                    if (i > 0)
                        dynamic_append(&joined, ' ');
                    dynamic_extend(&joined, var_arr->items[i]);
                }
                current = dynamic_to_string(&joined);
                free_dynamic(&joined);
            }
            free_array(var_arr);
            free(var_arr);
        }
    }

    // 2. Tilde expansion (for cases like PATH=~/bin:$PATH or HOME=~)
    if (session && session->features.tilde_expansion && current && current[0]) {
        Array *tilde_arr = tilde_expansion(current, session);
        if (tilde_arr) {
            free(current);
            if (tilde_arr->count == 0) {
                current = strdup("");
            } else if (tilde_arr->count == 1) {
                current = strdup(tilde_arr->items[0]);
            } else {
                Dynamic joined = {0};
                init_dynamic(&joined);
                for (size_t i = 0; i < tilde_arr->count; i++) {
                    if (i > 0)
                        dynamic_append(&joined, ' ');
                    dynamic_extend(&joined, tilde_arr->items[i]);
                }
                current = dynamic_to_string(&joined);
                free_dynamic(&joined);
            }
            free_array(tilde_arr);
            free(tilde_arr);
        }
    }

    return current ? current : strdup("");
}

#ifndef TIDESH_DISABLE_ASSIGNMENTS
char *expand_assignment_value(const char *raw_value, Session *session) {
    return expand_word_no_split(raw_value, session);
}
#endif

/* Build a command string from argv for display */
char *build_command_string(char **argv, int argc) {
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

void free_expanded_args(int argc, char **argv, char **arg_keys,
                        int *arg_is_sub) {
    if (argv) {
        for (int i = 0; i < argc; i++) {
            if (argv[i])
                free(argv[i]);
        }
        free(argv);
    }
    if (arg_keys) {
        for (int i = 0; i < argc; i++) {
            if (arg_keys[i])
                free(arg_keys[i]);
        }
        free(arg_keys);
    }
    if (arg_is_sub) {
        free(arg_is_sub);
    }
}

int execute_command(ASTNode *node, Session *session) {
    // Expand arguments
    int    argc       = 0;
    char **argv       = NULL;
    char **arg_keys   = NULL;
    int   *arg_is_sub = NULL;

    for (int i = 0; i < node->argc; i++) {
        const char *arg_key =
            (node->arg_keys && node->arg_keys[i]) ? node->arg_keys[i] : NULL;
        if (node->arg_is_sub && node->arg_is_sub[i] != 0) {
            argc++;
            argv               = realloc(argv, (argc + 1) * sizeof(char *));
            arg_keys           = realloc(arg_keys, (argc + 1) * sizeof(char *));
            arg_is_sub         = realloc(arg_is_sub, argc * sizeof(int));
            argv[argc - 1]     = strdup(node->argv[i]);
            arg_keys[argc - 1] = arg_key ? strdup(arg_key) : NULL;
            arg_is_sub[argc - 1] = node->arg_is_sub[i];
            argv[argc]           = NULL;
            arg_keys[argc]       = NULL;
        } else {
            Array *expansion = full_expansion(node->argv[i], session);
            if (!expansion) {
                free_expanded_args(argc, argv, arg_keys, arg_is_sub);
                environ_set_exit_status(session->environ, 1);
                if (session->try_depth > 0 && session->ignore_errexit == 0 &&
                    !session_is_exit_code_allowed(session, 1)) {
                    session->try_caught    = true;
                    session->try_exit_code = 1;
                    return 1;
                }
                if (session->options.errexit && session->ignore_errexit == 0) {
                    session->exit_requested = true;
                }
                return 1;
            }
            for (size_t j = 0; j < expansion->count; j++) {
                argc++;
                argv           = realloc(argv, (argc + 1) * sizeof(char *));
                arg_keys       = realloc(arg_keys, (argc + 1) * sizeof(char *));
                arg_is_sub     = realloc(arg_is_sub, argc * sizeof(int));
                argv[argc - 1] = strdup(expansion->items[j]);
                arg_keys[argc - 1]   = arg_key ? strdup(arg_key) : NULL;
                arg_is_sub[argc - 1] = 0;
                argv[argc]           = NULL;
                arg_keys[argc]       = NULL;
            }
            free_array(expansion);
            free(expansion);
        }
    }

    // Handle variable assignments without command
    if (argc == 0 && node->assignments) {
#ifndef TIDESH_DISABLE_ASSIGNMENTS
        if (!session->features.assignments) {
            fprintf(stderr, "tidesh: assignments are disabled\n");
            free_expanded_args(argc, argv, arg_keys, arg_is_sub);
            return 127;
        }
        for (size_t i = 0; i < node->assignments->count; i++) {
            char *copy = strdup(node->assignments->items[i]);
            char *eq   = strchr(copy, '=');
            if (eq) {
                *eq                = 0;
                char *expanded_val = expand_assignment_value(eq + 1, session);
                environ_set(session->environ, copy, expanded_val);
                free(expanded_val);
            }
            free(copy);
        }
#endif
        free_expanded_args(argc, argv, arg_keys, arg_is_sub);
        environ_set_exit_status(session->environ, 0);
        return 0;
    }

    if (!argv || !argv[0]) {
        free_expanded_args(argc, argv, arg_keys, arg_is_sub);
        environ_set_exit_status(session->environ, 0);
        return 0;
    }

    const char *cmd_name_raw     = argv[0];
    char       *cmd_name_trimmed = trim_whitespace(cmd_name_raw);
    const char *cmd_name = cmd_name_trimmed ? cmd_name_trimmed : cmd_name_raw;
    environ_set_last_arg(session->environ, argv[argc - 1]);

    if (session->options.xtrace) {
        char *ps4 = environ_get(session->environ, "PS4");
        fprintf(stderr, "%s", (ps4 && *ps4) ? ps4 : "+ ");
        for (int i = 0; i < argc; i++) {
            if (i > 0)
                fputc(' ', stderr);
            fprintf(stderr, "%s", argv[i]);
        }
        fputc('\n', stderr);
    }

    if (session->options.noexec && strcmp(cmd_name, "set") != 0) {
        free_expanded_args(argc, argv, arg_keys, arg_is_sub);
        if (cmd_name_trimmed)
            free(cmd_name_trimmed);
        environ_set_exit_status(session->environ, 0);
        return 0;
    }

    // Special builtins should be executed in the main process
    if (is_special_builtin(cmd_name)) {
#ifndef TIDESH_DISABLE_ASSIGNMENTS
        typedef struct ReadVarBackup {
            char *key;
            char *val;
            bool  existed;
        } ReadVarBackup;
        ReadVarBackup *read_backups      = NULL;
        size_t         read_backup_count = 0;

        bool is_read_cmd = (strcmp(cmd_name, "read") == 0);
        if (is_read_cmd && node->assignments && node->assignments->count > 0) {
            read_backups =
                calloc(node->assignments->count, sizeof(ReadVarBackup));
            for (size_t i = 0; i < node->assignments->count; i++) {
                char *copy = strdup(node->assignments->items[i]);
                char *eq   = strchr(copy, '=');
                if (eq) {
                    *eq       = '\0';
                    char *cur = environ_get(session->environ, copy);
                    read_backups[read_backup_count].key     = strdup(copy);
                    read_backups[read_backup_count].existed = (cur != NULL);
                    read_backups[read_backup_count].val =
                        cur ? strdup(cur) : NULL;
                    read_backup_count++;
                }
                free(copy);
            }
        }

        if (node->assignments) {
            for (size_t i = 0; i < node->assignments->count; i++) {
                char *copy = strdup(node->assignments->items[i]);
                char *eq   = strchr(copy, '=');
                if (eq) {
                    *eq = 0;
                    char *expanded_val =
                        expand_assignment_value(eq + 1, session);
                    environ_set(session->environ, copy, expanded_val);
                    free(expanded_val);
                }
                free(copy);
            }
        }
#endif
        RedirectionState redir_state = {0};
        if (!apply_in_process_redirections(node, session, &redir_state)) {
            restore_in_process_redirections(&redir_state);
            free_expanded_args(argc, argv, arg_keys, arg_is_sub);
            if (cmd_name_trimmed)
                free(cmd_name_trimmed);
            environ_set_exit_status(session->environ, 1);
            return 1;
        }

        int (*builtin_func)(int, char **, Session *) = get_builtin(cmd_name);
        int st                                       = 1;
        if (builtin_func) {
            st = builtin_func(argc, argv, session);
        }
        if (session->permanent_redirections) {
            session->permanent_redirections = false;
            for (size_t i = 0; i < redir_state.count; i++) {
                if (redir_state.saved[i].dup_fd >= 0) {
                    close(redir_state.saved[i].dup_fd);
                }
            }
            free(redir_state.saved);
            redir_state.saved    = NULL;
            redir_state.count    = 0;
            redir_state.capacity = 0;
        } else {
            restore_in_process_redirections(&redir_state);
        }

#ifndef TIDESH_DISABLE_ASSIGNMENTS
        if (read_backups) {
            for (size_t i = 0; i < read_backup_count; i++) {
                if (read_backups[i].existed) {
                    environ_set(session->environ, read_backups[i].key,
                                read_backups[i].val);
                    free(read_backups[i].val);
                } else {
                    environ_remove(session->environ, read_backups[i].key);
                }
                free(read_backups[i].key);
            }
            free(read_backups);
        }
#endif

        free_expanded_args(argc, argv, arg_keys, arg_is_sub);
        if (cmd_name_trimmed)
            free(cmd_name_trimmed);
        environ_set_exit_status(session->environ, st);
        if (session->try_depth > 0 && session->ignore_errexit == 0 &&
            !session_is_exit_code_allowed(session, st)) {
            session->try_caught    = true;
            session->try_exit_code = st;
            return st;
        }
        if (session->options.errexit && st != 0 &&
            session->ignore_errexit == 0) {
            session->exit_requested = true;
        }
        return st;
    }

#ifndef TIDESH_DISABLE_FUNCTIONS
    // Check for shell functions
    if (session->features.functions) {
        ASTNode *func_node = load_function_body(session->functions, cmd_name);
        if (func_node) {
            ASTNode         *func_body = (func_node->type == NODE_FUNCTION_DEF)
                                             ? func_node->left
                                             : func_node;
            RedirectionState redir_state = {0};
            if (!apply_in_process_redirections(node, session, &redir_state)) {
                restore_in_process_redirections(&redir_state);
                free_expanded_args(argc, argv, arg_keys, arg_is_sub);
                if (cmd_name_trimmed)
                    free(cmd_name_trimmed);
                environ_set_exit_status(session->environ, 1);
                return 1;
            }

            size_t num_declared =
                (func_node->type == NODE_FUNCTION_DEF && func_node->argc > 1)
                    ? (size_t)(func_node->argc - 1)
                    : 0;

            char **param_vals =
                num_declared > 0 ? calloc(num_declared, sizeof(char *)) : NULL;
            bool *param_set =
                num_declared > 0 ? calloc(num_declared, sizeof(bool)) : NULL;

            // 1. Match named arguments to declared parameters
            if (num_declared > 0 && arg_keys) {
                for (int i = 1; i < argc; i++) {
                    if (arg_keys[i]) {
                        for (size_t p = 0; p < num_declared; p++) {
                            if (strcmp(arg_keys[i], func_node->argv[p + 1]) ==
                                0) {
                                param_vals[p] = argv[i];
                                param_set[p]  = true;
                                break;
                            }
                        }
                    }
                }
            }

            // 2. Match positional arguments to unset declared parameters
            size_t next_pos_idx = 0;
            for (int i = 1; i < argc; i++) {
                if (!arg_keys || !arg_keys[i]) {
                    while (next_pos_idx < num_declared &&
                           param_set[next_pos_idx]) {
                        next_pos_idx++;
                    }
                    if (next_pos_idx < num_declared) {
                        param_vals[next_pos_idx] = argv[i];
                        param_set[next_pos_idx]  = true;
                        next_pos_idx++;
                    }
                }
            }

            for (size_t p = 0; p < num_declared; p++) {
                if (!param_set[p]) {
                    param_vals[p] = "";
                }
            }

            size_t pos_call_count = 0;
            for (int i = 1; i < argc; i++) {
                if (!arg_keys || !arg_keys[i]) {
                    pos_call_count++;
                }
            }

            size_t max_pos = 0;
            if (num_declared > 0) {
                for (size_t p = 0; p < num_declared; p++) {
                    if (param_set[p] && (p + 1) > max_pos) {
                        max_pos = p + 1;
                    }
                }
                if (pos_call_count > max_pos) {
                    max_pos = pos_call_count;
                }
            } else {
                max_pos = argc - 1;
            }

            char **pos_values =
                max_pos > 0 ? calloc(max_pos, sizeof(char *)) : NULL;
            for (size_t p = 0; p < max_pos; p++) {
                if (p < num_declared) {
                    pos_values[p] = param_vals[p];
                }
            }
            if (pos_call_count > num_declared) {
                size_t cur_pos = 0;
                for (int i = 1; i < argc; i++) {
                    if (!arg_keys || !arg_keys[i]) {
                        if (cur_pos >= num_declared && cur_pos < max_pos) {
                            pos_values[cur_pos] = argv[i];
                        } else if (num_declared == 0 && cur_pos < max_pos) {
                            pos_values[cur_pos] = argv[i];
                        }
                        cur_pos++;
                    }
                }
            } else if (num_declared == 0) {
                for (int i = 1; i < argc; i++) {
                    pos_values[i - 1] = argv[i];
                }
            }

            // 1. Backup and clear existing positional parameters ($0, $1, ...)
            size_t          old_pos_count = 0;
            PosParamBackup *old_pos_params =
                backup_and_clear_positional_params(session, &old_pos_count);

            // 2. Set new positional parameters for the function ($0, $1, ...)
            environ_set(session->environ, "0", (char *)cmd_name);
            for (size_t i = 0; i < max_pos; i++) {
                char key_str[16];
                snprintf(key_str, sizeof(key_str), "%zu", i + 1);
                environ_set(session->environ, key_str,
                            pos_values[i] ? pos_values[i] : "");
            }
            char argc_str[16];
            snprintf(argc_str, sizeof(argc_str), "%zu", max_pos);
            environ_set(session->environ, "#", argc_str);

            Dynamic all_params = {0};
            init_dynamic(&all_params);
            for (size_t i = 0; i < max_pos; i++) {
                if (i > 0) {
                    dynamic_append(&all_params, ' ');
                }
                dynamic_extend(&all_params, pos_values[i] ? pos_values[i] : "");
            }
            char *all_str = dynamic_to_string(&all_params);
            environ_set(session->environ, "@", all_str ? all_str : "");
            environ_set(session->environ, "*", all_str ? all_str : "");
            free(all_str);
            free_dynamic(&all_params);
            if (pos_values)
                free(pos_values);

            // Backup and set named parameters
            typedef struct NamedParamBackup {
                char *key;
                char *val;
                bool  existed;
            } NamedParamBackup;

            size_t max_named =
                num_declared + (argc > 1 ? (size_t)(argc - 1) : 0);
            NamedParamBackup *named_backups =
                max_named > 0 ? calloc(max_named, sizeof(NamedParamBackup))
                              : NULL;
            size_t named_backup_count = 0;

            for (size_t p = 0; p < num_declared; p++) {
                const char *pname = func_node->argv[p + 1];
                char       *cur = environ_get(session->environ, (char *)pname);
                named_backups[named_backup_count].key     = strdup(pname);
                named_backups[named_backup_count].existed = (cur != NULL);
                named_backups[named_backup_count].val =
                    cur ? strdup(cur) : NULL;
                named_backup_count++;
                environ_set(session->environ, (char *)pname, param_vals[p]);
            }

            if (arg_keys) {
                for (int i = 1; i < argc; i++) {
                    if (arg_keys[i]) {
                        bool already = false;
                        for (size_t b = 0; b < named_backup_count; b++) {
                            if (strcmp(named_backups[b].key, arg_keys[i]) ==
                                0) {
                                already = true;
                                break;
                            }
                        }
                        if (!already) {
                            char *cur =
                                environ_get(session->environ, arg_keys[i]);
                            named_backups[named_backup_count].key =
                                strdup(arg_keys[i]);
                            named_backups[named_backup_count].existed =
                                (cur != NULL);
                            named_backups[named_backup_count].val =
                                cur ? strdup(cur) : NULL;
                            named_backup_count++;
                            environ_set(session->environ, arg_keys[i], argv[i]);
                        }
                    }
                }
            }

            if (param_vals)
                free(param_vals);
            if (param_set)
                free(param_set);

            session->function_depth++;
            // Execute the function body in the current process
            int st = execute(func_body, session);
            session->function_depth--;

            if (session->return_requested) {
                session->return_requested = false;
                st                        = session->return_status;
            }

            // Restore named parameters
            if (named_backups) {
                for (size_t b = 0; b < named_backup_count; b++) {
                    if (named_backups[b].existed) {
                        environ_set(session->environ, named_backups[b].key,
                                    named_backups[b].val);
                        free(named_backups[b].val);
                    } else {
                        environ_remove(session->environ, named_backups[b].key);
                    }
                    free(named_backups[b].key);
                }
                free(named_backups);
            }

            // 3. Clear function positional parameters and restore original ones
            restore_positional_params(session, old_pos_params, old_pos_count);

            restore_in_process_redirections(&redir_state);

            free_expanded_args(argc, argv, arg_keys, arg_is_sub);
            if (cmd_name_trimmed)
                free(cmd_name_trimmed);
            environ_set_exit_status(session->environ, st);
            if (session->try_depth > 0 && session->ignore_errexit == 0 &&
                !session_is_exit_code_allowed(session, st)) {
                session->try_caught    = true;
                session->try_exit_code = st;
                return st;
            }
            if (session->options.errexit && st != 0 &&
                session->ignore_errexit == 0) {
                session->exit_requested = true;
            }
            return st;
        }
    }
#endif

    bool  is_external   = !is_builtin(cmd_name);
    char *resolved_path = NULL;
    if (is_external) {
        resolved_path = find_in_path(cmd_name, session);
        if (!resolved_path) {
            HookEnvVar nf_vars[] = {{"TIDE_CMD", cmd_name}};
            run_cwd_hook_with_vars(session, HOOK_CMD_NOT_FOUND, nf_vars, 1);
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: command not found: %s\n", PROJECT_NAME,
                    cmd_name);
#else
            fprintf(stderr, "tidesh: command not found: %s\n", cmd_name);
#endif
            free_expanded_args(argc, argv, arg_keys, arg_is_sub);
            if (cmd_name_trimmed)
                free(cmd_name_trimmed);
            environ_set_exit_status(session->environ, 127);
            return 127;
        }

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
        if (!fs_sandbox_check_access(session, resolved_path, FS_SANDBOX_EXEC)) {
            fprintf(stderr, "tidesh: %s: Permission denied by sandbox\n",
                    cmd_name);
            free(resolved_path);
            free_expanded_args(argc, argv, arg_keys, arg_is_sub);
            if (cmd_name_trimmed)
                free(cmd_name_trimmed);
            environ_set_exit_status(session->environ, 126);
            return 126;
        }
#endif

        HookEnvVar exec_vars[] = {{"TIDE_EXEC", resolved_path},
                                  {"TIDE_ARGV0", argv[0]}};
        run_cwd_hook_with_vars(session, HOOK_BEFORE_EXEC, exec_vars, 2);
    }

    pid_t pid = fork();

    if (pid == 0) {
        /* Child Process */
        traps_reset_for_subshell(session);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
        fs_sandbox_apply_child(session);
#endif

        // Handle process substitution in argv
        for (int i = 0; i < argc; i++) {
            if (arg_is_sub && arg_is_sub[i] != 0) {
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
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
#endif
            }
        }

        // Manage redirections
        if (handle_redirections(node, session) < 0) {
            exit(1);
        }

        // Manage temporary Assignments (VAR=VAL cmd)
        // Note: We modify the Session only in the child process
        if (node->assignments) {
#ifndef TIDESH_DISABLE_ASSIGNMENTS
            if (!session->features.assignments) {
                fprintf(stderr, "tidesh: assignments are disabled\n");
                exit(127);
            }
            for (size_t i = 0; i < node->assignments->count; i++) {
                char *assign = strdup(node->assignments->items[i]);
                char *eq     = strchr(assign, '=');
                if (eq) {
                    *eq = '\0';
                    char *expanded_val =
                        expand_assignment_value(eq + 1, session);
                    environ_set(session->environ, assign, expanded_val);
                    free(expanded_val);
                }
                free(assign);
            }
#else
            fprintf(stderr, "tidesh: assignments are disabled\n");
            exit(127);
#endif
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
        int (*builtin_func)(int, char **, Session *) = get_builtin(cmd_name);

        if (builtin_func) {
            int ret = builtin_func(argc, argv, session);
            exit(ret);
        }

        // Find the command path if not already resolved
        char *path =
            resolved_path ? resolved_path : find_in_path(cmd_name, session);
        if (!path) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: command not found: %s\n", PROJECT_NAME,
                    cmd_name);
#else
            fprintf(stderr, "tidesh: command not found: %s\n", cmd_name);
#endif
            exit(127);
        }

        // Check for shebang in the script file
        int    interp_argc = 0;
        char **interp_argv = NULL;
        if (parse_shebang(path, &interp_argc, &interp_argv)) {
            // Script has a shebang, execute with the interpreter
            // Build new argv: [interpreter, interp_args..., script_path,
            // original_args...]
            int new_argc =
                interp_argc + argc; // interpreter + args + original args
            char **new_argv = malloc((new_argc + 1) * sizeof(char *));

            int idx = 0;
            // Add interpreter and its arguments
            for (int i = 0; i < interp_argc; i++) {
                new_argv[idx++] = interp_argv[i];
            }
            // Add the script path
            new_argv[idx++] = strdup(path);
            // Add original arguments (skip argv[0] which is the script name)
            for (int i = 1; i < argc; i++) {
                new_argv[idx++] = strdup(argv[i]);
            }
            new_argv[idx] = NULL;

            // Execute with the interpreter
            execve(interp_argv[0], new_argv, envp);

            // If we arrived here, execve failed
            perror("execve");

            // Clean up
            for (int i = 0; i < idx; i++) {
                free(new_argv[i]);
            }
            free(new_argv);
            free(interp_argv);
            if (envp)
                free(envp);
            if (env_array) {
                free_array(env_array);
                free(env_array);
            }
            if (path && path != resolved_path) {
                free(path);
            }
            exit(126);
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
        if (path && path != resolved_path) {
            free(path);
        }
        exit(126);
    }

    /* Parent Process */
    char *cmd_str    = build_command_string(argv, argc);
    char *argv0_copy = argv && argv[0] ? strdup(argv[0]) : NULL;
    free_expanded_args(argc, argv, arg_keys, arg_is_sub);
    if (cmd_name_trimmed)
        free(cmd_name_trimmed);

    if (node->background) {
#ifdef TIDESH_DISABLE_JOB_CONTROL
        fprintf(stderr, "tidesh: background jobs disabled at compile time\n");
        kill(pid, SIGTERM);
        while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
            ;
        if (cmd_str)
            free(cmd_str);
        free(argv0_copy);
        free(resolved_path);
        return 127;
#else
        if (session->features.job_control) {
            int job_id = jobs_add(session->jobs, pid, cmd_str, JOB_RUNNING);
            if (cmd_str) {
                free(cmd_str);
            }
            printf("[%d] %d\n", job_id, pid);
            environ_set_background_pid(session->environ, pid);
            environ_set_exit_status(session->environ, 0);
            HookEnvVar job_vars[] = {{"TIDE_JOB_ID", ""},
                                     {"TIDE_JOB_PID", ""},
                                     {"TIDE_JOB_STATE", "running"}};
            char       job_id_buf[16];
            char       job_pid_buf[20];
            snprintf(job_id_buf, sizeof(job_id_buf), "%d", job_id);
            snprintf(job_pid_buf, sizeof(job_pid_buf), "%d", pid);
            job_vars[0].value = job_id_buf;
            job_vars[1].value = job_pid_buf;
            run_cwd_hook_with_vars(session, HOOK_BEFORE_JOB, job_vars, 3);
            if (is_external) {
                HookEnvVar exec_vars[] = {
                    {"TIDE_EXEC", resolved_path ? resolved_path : ""},
                    {"TIDE_ARGV0", argv0_copy ? argv0_copy : ""}};
                run_cwd_hook_with_vars(session, HOOK_AFTER_EXEC, exec_vars, 2);
            }
            free(argv0_copy);
            free(resolved_path);
            return 0;
        } else {
            fprintf(stderr, "tidesh: background jobs not enabled\n");
            kill(pid, SIGTERM);
            while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
                ;
            if (cmd_str)
                free(cmd_str);
            free(argv0_copy);
            free(resolved_path);
            return 127;
        }
#endif
    } else {
        int status = 0;
        while (waitpid(pid, &status, WUNTRACED) == -1) {
            if (errno == EINTR) {
                check_pending_traps(session);
                continue;
            }
            break;
        }
        check_pending_traps(session);

        if (session->terminal && isatty(STDIN_FILENO)) {
            tcsetattr(STDIN_FILENO, TCSADRAIN,
                      &session->terminal->orig_termios);
        }

        int exit_status = 0;
        if (WIFSTOPPED(status)) {
#ifndef TIDESH_DISABLE_JOB_CONTROL
            if (session->features.job_control) {
                int job_id =
                    jobs_add(session->jobs, pid, cmd_str ? cmd_str : cmd_name,
                             JOB_STOPPED);
                printf("\n[%d]+  Stopped                 %s\n", job_id,
                       cmd_name);
                exit_status = 128 + WSTOPSIG(status);
            } else {
                kill(pid, SIGKILL);
                exit_status = 128 + WSTOPSIG(status);
            }
#else
            kill(pid, SIGKILL);
            exit_status = 128 + WSTOPSIG(status);
#endif
        } else if (WIFSIGNALED(status)) {
            exit_status = 128 + WTERMSIG(status);
            char sig_buf[16];
            snprintf(sig_buf, sizeof(sig_buf), "%d", WTERMSIG(status));
            HookEnvVar sig_vars[] = {{"TIDE_SIGNAL", sig_buf}};
            run_cwd_hook_with_vars(session, HOOK_SIGNAL, sig_vars, 1);
        } else {
            exit_status = WEXITSTATUS(status);
        }
        if (cmd_str) {
            free(cmd_str);
        }
        environ_set_exit_status(session->environ, exit_status);
        if (session->try_depth > 0 && session->ignore_errexit == 0 &&
            !session_is_exit_code_allowed(session, exit_status)) {
            session->try_caught    = true;
            session->try_exit_code = exit_status;
            if (is_external) {
                HookEnvVar exec_vars[] = {
                    {"TIDE_EXEC", resolved_path ? resolved_path : ""},
                    {"TIDE_ARGV0", argv0_copy ? argv0_copy : ""}};
                run_cwd_hook_with_vars(session, HOOK_AFTER_EXEC, exec_vars, 2);
            }
            free(argv0_copy);
            free(resolved_path);
            return exit_status;
        }
        if (session->options.errexit && exit_status != 0 &&
            session->ignore_errexit == 0) {
            session->exit_requested = true;
        }
        if (is_external) {
            HookEnvVar exec_vars[] = {
                {"TIDE_EXEC", resolved_path ? resolved_path : ""},
                {"TIDE_ARGV0", argv0_copy ? argv0_copy : ""}};
            run_cwd_hook_with_vars(session, HOOK_AFTER_EXEC, exec_vars, 2);
        }
        free(argv0_copy);
        free(resolved_path);
        return exit_status;
    }
}
