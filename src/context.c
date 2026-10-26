/** context.c
 *
 * Implementation of context managers for tidesh.
 */

#include <ctype.h>   /* isdigit */
#include <errno.h>   /* errno */
#include <stdio.h>   /* printf, fprintf, snprintf, perror */
#include <stdlib.h>  /* malloc, calloc, free, atoi */
#include <string.h>  /* strdup, strcmp, strchr, strlen */
#include <sys/stat.h>/* umask, mode_t */
#include <unistd.h>  /* chdir */

#include "ast.h"
#include "context.h"
#include "contexts/allowed_exit_codes.h"
#include "contexts/limits.h"
#include "contexts/pids.h"
#include "contexts/timeout.h"
#include "data/array.h"
#include "data/dynamic.h"
#include "data/trie.h"
#include "environ.h"
#include "execute.h"
#include "expand.h"
#include "session.h"

/* -------------------------------------------------------------------------
 * Memory Management for Context Invocations
 * ------------------------------------------------------------------------- */

ContextInvocation *init_context_invocation(const char *name) {
    ContextInvocation *inv = calloc(1, sizeof(ContextInvocation));
    if (!inv)
        return NULL;
    inv->name      = name ? strdup(name) : NULL;
    inv->args      = NULL;
    inv->arg_count = 0;
    inv->next      = NULL;
    return inv;
}

void context_invocation_add_arg(ContextInvocation *inv, const char *key,
                                const char *value) {
    if (!inv)
        return;
    ContextArg *new_args =
        realloc(inv->args, (inv->arg_count + 1) * sizeof(ContextArg));
    if (!new_args)
        return;
    inv->args = new_args;

    char *k_copy = NULL;
    if (key) {
        while (isspace((unsigned char)*key))
            key++;
        k_copy     = strdup(key);
        size_t len = strlen(k_copy);
        while (len > 0 && (isspace((unsigned char)k_copy[len - 1]) ||
                           k_copy[len - 1] == ',')) {
            k_copy[--len] = '\0';
        }
    }

    const char *v_str = value ? value : "";
    while (isspace((unsigned char)*v_str))
        v_str++;
    char  *v_copy = strdup(v_str);
    size_t len    = strlen(v_copy);
    while (len > 0 && (isspace((unsigned char)v_copy[len - 1]) ||
                       v_copy[len - 1] == ',')) {
        v_copy[--len] = '\0';
    }

    inv->args[inv->arg_count].key   = k_copy;
    inv->args[inv->arg_count].value = v_copy;
    inv->arg_count++;
}

void free_context_invocations(ContextInvocation *inv) {
    while (inv) {
        ContextInvocation *next = inv->next;
        free(inv->name);
        for (size_t i = 0; i < inv->arg_count; i++) {
            free(inv->args[i].key);
            free(inv->args[i].value);
        }
        free(inv->args);
        free(inv);
        inv = next;
    }
}

ContextInvocation *copy_context_invocations(const ContextInvocation *inv) {
    if (!inv)
        return NULL;

    ContextInvocation *head = NULL;
    ContextInvocation *tail = NULL;

    for (const ContextInvocation *curr = inv; curr; curr = curr->next) {
        ContextInvocation *node = init_context_invocation(curr->name);
        if (!node)
            continue;

        for (size_t i = 0; i < curr->arg_count; i++) {
            context_invocation_add_arg(node, curr->args[i].key,
                                       curr->args[i].value);
        }

        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail       = node;
        }
    }

    return head;
}

/* -------------------------------------------------------------------------
 * Custom Context Manager Registry
 * ------------------------------------------------------------------------- */

void init_session_context_managers(Session *session) {
    if (!session)
        return;
    session->context_managers = NULL;
    session->context_stack    = NULL;
}

void free_session_context_managers(Session *session) {
    if (!session)
        return;

    CustomContextManager *m = session->context_managers;
    while (m) {
        CustomContextManager *next = m->next;
        free(m->name);
        free(m->enter_cmd);
        free(m->exit_cmd);
        free(m);
        m = next;
    }
    session->context_managers = NULL;
}

int register_custom_context_manager(Session *session, const char *name,
                                    const char *enter_cmd,
                                    const char *exit_cmd) {
    if (!session || !name || name[0] == '\0')
        return 1;

    // Check if name is a reserved built-in context manager
    if (is_builtin_context_manager(name)) {
        fprintf(stderr, "context-manager: '%s' is a built-in context manager\n",
                name);
        return 1;
    }

    CustomContextManager *existing = get_custom_context_manager(session, name);
    if (existing) {
        free(existing->enter_cmd);
        free(existing->exit_cmd);
        existing->enter_cmd = enter_cmd ? strdup(enter_cmd) : strdup("");
        existing->exit_cmd  = exit_cmd ? strdup(exit_cmd) : strdup("");
        return 0;
    }

    CustomContextManager *mgr = calloc(1, sizeof(CustomContextManager));
    if (!mgr)
        return 1;
    mgr->name                 = strdup(name);
    mgr->enter_cmd            = enter_cmd ? strdup(enter_cmd) : strdup("");
    mgr->exit_cmd             = exit_cmd ? strdup(exit_cmd) : strdup("");
    mgr->next                 = session->context_managers;
    session->context_managers = mgr;

    return 0;
}

int unregister_custom_context_manager(Session *session, const char *name) {
    if (!session || !name)
        return 1;

    CustomContextManager **curr = &session->context_managers;
    while (*curr) {
        if (strcmp((*curr)->name, name) == 0) {
            CustomContextManager *to_free = *curr;
            *curr                         = to_free->next;
            free(to_free->name);
            free(to_free->enter_cmd);
            free(to_free->exit_cmd);
            free(to_free);
            return 0;
        }
        curr = &(*curr)->next;
    }
    return 1;
}

CustomContextManager *get_custom_context_manager(Session    *session,
                                                 const char *name) {
    if (!session || !name)
        return NULL;
    for (CustomContextManager *m = session->context_managers; m; m = m->next) {
        if (strcmp(m->name, name) == 0)
            return m;
    }
    return NULL;
}

bool is_builtin_context_manager(const char *name) {
    if (!name)
        return false;
    return (strcmp(name, "cwd") == 0 || strcmp(name, "cd") == 0 ||
            strcmp(name, "env") == 0 || strcmp(name, "options") == 0 ||
            strcmp(name, "umask") == 0 || strcmp(name, "session") == 0 ||
            strcmp(name, "allowed_exit_codes") == 0 ||
            strcmp(name, "allow_exit_codes") == 0 ||
            strcmp(name, "allowed_exit_code") == 0 ||
            strcmp(name, "allow_exit_code") == 0 ||
            strcmp(name, "allowed_codes") == 0 ||
            strcmp(name, "allow_codes") == 0 ||
            strcmp(name, "allowed_status") == 0 ||
            strcmp(name, "sandbox") == 0 || strcmp(name, "tempdir") == 0 ||
            strcmp(name, "chroot") == 0 || strcmp(name, "airgap") == 0 ||
            strcmp(name, "network") == 0 || strcmp(name, "timeout") == 0 ||
            strcmp(name, "limits") == 0 || strcmp(name, "limit") == 0 ||
            strcmp(name, "rlimit") == 0 || strcmp(name, "pids") == 0 ||
            strcmp(name, "procs") == 0 || strcmp(name, "nproc") == 0);
}

/* -------------------------------------------------------------------------
 * Positional Parameters Helper for Custom Context Managers
 * ------------------------------------------------------------------------- */

typedef struct PositionalBackup {
    char  *count;
    char **params;
    int    param_count;
} PositionalBackup;

static PositionalBackup save_positional_parameters(Session *session) {
    PositionalBackup backup = {0};
    char            *c      = environ_get(session->environ, "#");
    backup.count            = c ? strdup(c) : NULL;
    backup.param_count      = c ? atoi(c) : 0;
    if (backup.param_count > 0) {
        backup.params = calloc(backup.param_count, sizeof(char *));
        for (int i = 1; i <= backup.param_count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "%d", i);
            char *v              = environ_get(session->environ, key);
            backup.params[i - 1] = v ? strdup(v) : NULL;
        }
    }
    return backup;
}

static void restore_positional_parameters(Session          *session,
                                          PositionalBackup *backup) {
    // Remove existing parameters
    char *curr_c     = environ_get(session->environ, "#");
    int   curr_count = curr_c ? atoi(curr_c) : 0;
    for (int i = 1; i <= curr_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%d", i);
        environ_remove(session->environ, key);
    }

    if (backup->count) {
        environ_set(session->environ, "#", backup->count);
        free(backup->count);
        for (int i = 1; i <= backup->param_count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "%d", i);
            if (backup->params[i - 1]) {
                environ_set(session->environ, key, backup->params[i - 1]);
                free(backup->params[i - 1]);
            }
        }
        free(backup->params);
    } else {
        environ_remove(session->environ, "#");
    }
}

static char *execute_string_capture_in_process(const char *cmd,
                                               Session    *session) {
    if (!cmd || cmd[0] == '\0') {
        return strdup("");
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        execute_string(cmd, session);
        return strdup("");
    }

    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        execute_string(cmd, session);
        return strdup("");
    }

    dup2(pipe_fd[1], STDOUT_FILENO);
    close(pipe_fd[1]);

    execute_string(cmd, session);

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    // Read captured stdout from pipe_fd[0]
    char    buf[1024];
    Dynamic d = {0};
    init_dynamic(&d);
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_fd[0], buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            dynamic_append(&d, buf[i]);
        }
    }
    close(pipe_fd[0]);

    char *result = dynamic_to_string(&d);
    free_dynamic(&d);
    return result ? result : strdup("");
}

/* -------------------------------------------------------------------------
 * Execution Engine: execute_with_block
 * ------------------------------------------------------------------------- */

static void unwind_active_context(Session *session, ActiveContext *act,
                                  int exit_status) {
    if (!act)
        return;

    if (act->is_builtin) {
        if (strcmp(act->name, "cwd") == 0 || strcmp(act->name, "cd") == 0) {
            context_exit_cwd(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "env") == 0) {
            context_exit_env(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "options") == 0) {
            context_exit_options(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "umask") == 0) {
            context_exit_umask(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "session") == 0) {
            context_exit_session(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "allowed_exit_codes") == 0 ||
                   strcmp(act->name, "allow_exit_codes") == 0 ||
                   strcmp(act->name, "allowed_exit_code") == 0 ||
                   strcmp(act->name, "allow_exit_code") == 0 ||
                   strcmp(act->name, "allowed_codes") == 0 ||
                   strcmp(act->name, "allow_codes") == 0 ||
                   strcmp(act->name, "allowed_status") == 0) {
            context_exit_allowed_exit_codes(session, exit_status,
                                            act->builtin_state);
        } else if (strcmp(act->name, "sandbox") == 0) {
            context_exit_sandbox(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "tempdir") == 0) {
            context_exit_tempdir(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "chroot") == 0) {
            context_exit_chroot(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "airgap") == 0) {
            context_exit_airgap(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "network") == 0) {
            context_exit_network(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "timeout") == 0) {
            context_exit_timeout(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "limits") == 0 ||
                   strcmp(act->name, "limit") == 0 ||
                   strcmp(act->name, "rlimit") == 0) {
            context_exit_limits(session, exit_status, act->builtin_state);
        } else if (strcmp(act->name, "pids") == 0 ||
                   strcmp(act->name, "procs") == 0 ||
                   strcmp(act->name, "nproc") == 0) {
            context_exit_pids(session, exit_status, act->builtin_state);
        }
    } else if (act->custom_mgr && act->custom_mgr->exit_cmd &&
               act->custom_mgr->exit_cmd[0] != '\0') {
        // Set up positional parameters for exit:
        // $1 = saved_state (from enter stdout)
        // $2 = exit status of body
        // $3... = original arguments
        PositionalBackup pos_backup = save_positional_parameters(session);

        char status_str[32];
        snprintf(status_str, sizeof(status_str), "%d", exit_status);

        char total_count_str[32];
        snprintf(total_count_str, sizeof(total_count_str), "%zu",
                 2 + act->saved_argc);
        environ_set(session->environ, "#", total_count_str);

        // Trim trailing newline from saved state
        char *state_clean = act->custom_saved_state
                                ? strdup(act->custom_saved_state)
                                : strdup("");
        if (state_clean) {
            size_t slen = strlen(state_clean);
            while (slen > 0 && (state_clean[slen - 1] == '\n' ||
                                state_clean[slen - 1] == '\r')) {
                state_clean[slen - 1] = '\0';
                slen--;
            }
            environ_set(session->environ, "1", state_clean);
        }

        environ_set(session->environ, "2", status_str);

        for (size_t i = 0; i < act->saved_argc; i++) {
            char key[32];
            snprintf(key, sizeof(key), "%zu", 3 + i);
            environ_set(session->environ, key,
                        act->saved_args[i].value ? act->saved_args[i].value
                                                 : "");
        }

        environ_set(session->environ, "TIDE_CONTEXT_NAME", act->name);
        environ_set(session->environ, "TIDE_CONTEXT_STATUS", status_str);
        environ_set(session->environ, "TIDE_CONTEXT_STATE",
                    state_clean ? state_clean : "");

        execute_string(act->custom_mgr->exit_cmd, session);

        environ_remove(session->environ, "TIDE_CONTEXT_NAME");
        environ_remove(session->environ, "TIDE_CONTEXT_STATUS");
        environ_remove(session->environ, "TIDE_CONTEXT_STATE");
        free(state_clean);

        restore_positional_parameters(session, &pos_backup);
    }

    free(act->name);
    free(act->custom_saved_state);
    if (act->saved_args) {
        for (size_t i = 0; i < act->saved_argc; i++) {
            free(act->saved_args[i].key);
            free(act->saved_args[i].value);
        }
        free(act->saved_args);
    }
    free(act);
}

int execute_with_block(Session *session, ContextInvocation *invocations,
                       ASTNode *body) {
    if (!session)
        return 1;

    ContextFrame frame     = {0};
    frame.active_list      = NULL;
    frame.prev             = session->context_stack;
    session->context_stack = &frame;

    ContextInvocation *inv = invocations;
    while (inv) {
        // Expand arguments for this invocation
        ContextArg *expanded_args = NULL;
        if (inv->arg_count > 0) {
            expanded_args = calloc(inv->arg_count, sizeof(ContextArg));
            for (size_t i = 0; i < inv->arg_count; i++) {
                if (inv->args[i].key) {
                    Array *k_exp = full_expansion(inv->args[i].key, session);
                    if (k_exp && k_exp->count > 0) {
                        expanded_args[i].key = strdup(k_exp->items[0]);
                    } else {
                        expanded_args[i].key = strdup(inv->args[i].key);
                    }
                    if (k_exp) {
                        free_array(k_exp);
                        free(k_exp);
                    }
                }

                if (inv->args[i].value) {
                    Array *v_exp = full_expansion(inv->args[i].value, session);
                    if (v_exp && v_exp->count > 0) {
                        expanded_args[i].value = strdup(v_exp->items[0]);
                    } else {
                        expanded_args[i].value = strdup(inv->args[i].value);
                    }
                    if (v_exp) {
                        free_array(v_exp);
                        free(v_exp);
                    }
                } else {
                    expanded_args[i].value = strdup("");
                }
            }
        }

        ActiveContext *act = calloc(1, sizeof(ActiveContext));
        act->name          = strdup(inv->name);

        int enter_status = 0;

        if (is_builtin_context_manager(inv->name)) {
            act->is_builtin = true;
            if (strcmp(inv->name, "cwd") == 0 || strcmp(inv->name, "cd") == 0) {
                enter_status =
                    context_enter_cwd(session, expanded_args, inv->arg_count,
                                      &act->builtin_state);
            } else if (strcmp(inv->name, "env") == 0) {
                enter_status =
                    context_enter_env(session, expanded_args, inv->arg_count,
                                      &act->builtin_state);
            } else if (strcmp(inv->name, "options") == 0) {
                enter_status =
                    context_enter_options(session, expanded_args,
                                          inv->arg_count, &act->builtin_state);
            } else if (strcmp(inv->name, "umask") == 0) {
                enter_status =
                    context_enter_umask(session, expanded_args, inv->arg_count,
                                        &act->builtin_state);
            } else if (strcmp(inv->name, "session") == 0) {
                enter_status =
                    context_enter_session(session, expanded_args,
                                          inv->arg_count, &act->builtin_state);
            } else if (strcmp(inv->name, "allowed_exit_codes") == 0 ||
                       strcmp(inv->name, "allow_exit_codes") == 0 ||
                       strcmp(inv->name, "allowed_exit_code") == 0 ||
                       strcmp(inv->name, "allow_exit_code") == 0 ||
                       strcmp(inv->name, "allowed_codes") == 0 ||
                       strcmp(inv->name, "allow_codes") == 0 ||
                       strcmp(inv->name, "allowed_status") == 0) {
                enter_status = context_enter_allowed_exit_codes(
                    session, expanded_args, inv->arg_count,
                    &act->builtin_state);
            } else if (strcmp(inv->name, "sandbox") == 0) {
                enter_status =
                    context_enter_sandbox(session, expanded_args,
                                          inv->arg_count, &act->builtin_state);
            } else if (strcmp(inv->name, "tempdir") == 0) {
                enter_status =
                    context_enter_tempdir(session, expanded_args,
                                          inv->arg_count, &act->builtin_state);
            } else if (strcmp(inv->name, "chroot") == 0) {
                enter_status =
                    context_enter_chroot(session, expanded_args, inv->arg_count,
                                         &act->builtin_state);
            } else if (strcmp(inv->name, "airgap") == 0) {
                enter_status =
                    context_enter_airgap(session, expanded_args, inv->arg_count,
                                         &act->builtin_state);
            } else if (strcmp(inv->name, "network") == 0) {
                enter_status =
                    context_enter_network(session, expanded_args,
                                          inv->arg_count, &act->builtin_state);
            } else if (strcmp(inv->name, "timeout") == 0) {
                enter_status =
                    context_enter_timeout(session, expanded_args,
                                          inv->arg_count, &act->builtin_state);
            } else if (strcmp(inv->name, "limits") == 0 ||
                       strcmp(inv->name, "limit") == 0 ||
                       strcmp(inv->name, "rlimit") == 0) {
                enter_status =
                    context_enter_limits(session, expanded_args, inv->arg_count,
                                         &act->builtin_state);
            } else if (strcmp(inv->name, "pids") == 0 ||
                       strcmp(inv->name, "procs") == 0 ||
                       strcmp(inv->name, "nproc") == 0) {
                enter_status =
                    context_enter_pids(session, expanded_args, inv->arg_count,
                                       &act->builtin_state);
            }
        } else {
            CustomContextManager *custom =
                get_custom_context_manager(session, inv->name);
            if (!custom) {
                fprintf(stderr, "tidesh: unknown context manager '%s'\n",
                        inv->name);
                enter_status = 127;
            } else {
                act->is_builtin = false;
                act->custom_mgr = custom;
                act->saved_args = expanded_args;
                act->saved_argc = inv->arg_count;
                expanded_args   = NULL; // ownership transferred

                // Set up positional parameters for enter command
                PositionalBackup pos_backup =
                    save_positional_parameters(session);

                char count_str[32];
                snprintf(count_str, sizeof(count_str), "%zu", act->saved_argc);
                environ_set(session->environ, "#", count_str);

                for (size_t i = 0; i < act->saved_argc; i++) {
                    char key[32];
                    snprintf(key, sizeof(key), "%zu", i + 1);
                    environ_set(session->environ, key,
                                act->saved_args[i].value
                                    ? act->saved_args[i].value
                                    : "");
                }

                environ_set(session->environ, "TIDE_CONTEXT_NAME", act->name);
                environ_set(session->environ, "TIDE_CONTEXT_ARGC", count_str);

                if (custom->enter_cmd && custom->enter_cmd[0] != '\0') {
                    char *out = execute_string_capture_in_process(
                        custom->enter_cmd, session);
                    enter_status = environ_get_exit_status(session->environ);
                    act->custom_saved_state = out;
                }

                environ_remove(session->environ, "TIDE_CONTEXT_NAME");
                environ_remove(session->environ, "TIDE_CONTEXT_ARGC");

                restore_positional_parameters(session, &pos_backup);
            }
        }

        if (expanded_args) {
            for (size_t i = 0; i < inv->arg_count; i++) {
                free(expanded_args[i].key);
                free(expanded_args[i].value);
            }
            free(expanded_args);
        }

        if (enter_status != 0) {
            // Enter failed! Free this act and unwind previously entered
            // contexts
            free(act->name);
            free(act->custom_saved_state);
            if (act->saved_args) {
                for (size_t i = 0; i < act->saved_argc; i++) {
                    free(act->saved_args[i].key);
                    free(act->saved_args[i].value);
                }
                free(act->saved_args);
            }
            free(act);

            // Unwind previously entered managers in LIFO order
            ActiveContext *curr = frame.active_list;
            while (curr) {
                ActiveContext *next = curr->next;
                unwind_active_context(session, curr, enter_status);
                curr = next;
            }

            session->context_stack = frame.prev;
            environ_set_exit_status(session->environ, enter_status);
            return enter_status;
        }

        // Prepend to active_list so it is in LIFO order
        act->next         = frame.active_list;
        frame.active_list = act;

        inv = inv->next;
    }

    // Execute body
    int body_status = 0;
    if (body) {
        body_status = execute(body, session);
    }

    if (limit_sandbox_is_timed_out(session)) {
        session->exit_requested = false;
        body_status             = 124;
    }

    // Unwind all active managers in LIFO order
    ActiveContext *curr = frame.active_list;
    while (curr) {
        ActiveContext *next = curr->next;
        unwind_active_context(session, curr, body_status);
        curr = next;
    }

    session->context_stack = frame.prev;
    environ_set_exit_status(session->environ, body_status);
    return body_status;
}
