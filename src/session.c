#include <dirent.h>  /* DIR, opendir, readdir, closedir */
#include <limits.h>  /* PATH_MAX */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* snprintf */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* strdup, strcmp */
#include <unistd.h>  /* getcwd */

#include "data/array.h"      /* array_add, free_array */
#include "environ.h"         /* environ_get, environ_set, environ_get_default */
#include "feature-flags.h"        /* Features */
#include "hooks.h"           /* HOOK_* */
#include "prompt/terminal.h" /* Terminal, terminal functions */
#include "session.h"         /* Session, Environ, History, Trie, DirStack */

Session *init_session(Session *session, char *history_path) {
    if (!session) {
        session = calloc(1, sizeof(Session));
        if (!session) {
            return NULL;
        }
    } else {
        memset(session, 0, sizeof(Session));
    }

    session->environ = init_environ(NULL);
    if (!session->environ) {
        free(session);
        return NULL;
    }

#ifndef TIDESH_DISABLE_HISTORY
    session->history = load_history(NULL, history_path);
    if (!session->history) {
        free_session(session);
        free(session);
        return NULL;
    }
#endif

#ifndef TIDESH_DISABLE_ALIASES
    session->aliases = init_trie(NULL);
    if (!session->aliases) {
        free_session(session);
        free(session);
        return NULL;
    }
#endif

#ifndef TIDESH_DISABLE_DIRSTACK
    session->dirstack = init_dirstack(NULL);
    if (!session->dirstack) {
        free_session(session);
        free(session);
        return NULL;
    }
#endif

    session->path_commands = init_trie(NULL);
    if (!session->path_commands) {
        free_session(session);
        free(session);
        return NULL;
    }

    session->terminal = init_terminal(NULL, session);
    if (!session->terminal) {
        free_session(session);
        free(session);
        return NULL;
    }

#ifndef TIDESH_DISABLE_JOB_CONTROL
    session->jobs = init_jobs();
    if (!session->jobs) {
        free_session(session);
        free(session);
        return NULL;
    }
#endif

    session->current_working_dir  = NULL;
    session->previous_working_dir = NULL;
    session->exit_requested       = false;
    session->hooks_disabled       = false;

    // Initialize feature flags (all enabled by default)
    init_features(&session->features);

    // Initialize working directories
    update_working_dir(session);
    // update_path(session); // Pretty slow
    hooks_register_session(session);
    return session;
}

void free_session(Session *session) {
    if (!session)
        return;

    // Free working directories (session owns these)
    free(session->current_working_dir);
    free(session->previous_working_dir);

#ifndef TIDESH_DISABLE_DIRSTACK
    if (session->dirstack) {
        free_dirstack(session->dirstack);
        free(session->dirstack);
    }
#endif

    if (session->environ) {
        free_environ(session->environ);
        free(session->environ);
    }

#ifndef TIDESH_DISABLE_HISTORY
    if (session->history) {
        free_history(session->history);
        free(session->history);
    }
#endif

#ifndef TIDESH_DISABLE_ALIASES
    if (session->aliases) {
        free_trie(session->aliases);
        free(session->aliases);
    }
#endif

    if (session->path_commands) {
        free_trie(session->path_commands);
        free(session->path_commands);
    }

    if (session->terminal) {
        free_terminal(session->terminal, session);
        free(session->terminal);
    }

#ifndef TIDESH_DISABLE_JOB_CONTROL
    if (session->jobs) {
        free_jobs(session->jobs);
        free(session->jobs);
    }
#endif
}

static void init_previous_working_dir(Session *session) {
    // First time setting CWD, set previous to same value
    char *oldpwd = environ_get(session->environ, "OLDPWD");
    if (!oldpwd) {
        session->previous_working_dir = strdup(session->current_working_dir);
        environ_set(session->environ, "OLDPWD", session->previous_working_dir);
    } else {
        session->previous_working_dir = strdup(oldpwd);
    }
}

static size_t trim_trailing_slash_len(const char *path, size_t len) {
    while (len > 1 && path[len - 1] == '/') {
        len--;
    }
    return len;
}

static bool is_descendant_path(const char *parent, const char *child) {
    if (!parent || !child)
        return false;

    size_t parent_len = trim_trailing_slash_len(parent, strlen(parent));
    size_t child_len  = trim_trailing_slash_len(child, strlen(child));

    if (parent_len >= child_len)
        return false;

    if (strncmp(parent, child, parent_len) != 0)
        return false;

    if (parent_len == 1 && parent[0] == '/') {
        return true;
    }

    return child[parent_len] == '/';
}

static void run_parent_enter_hooks(Session *session, const char *path) {
    if (!session || !path || path[0] == '\0')
        return;

    Array *parents = init_array(NULL);
    if (!parents)
        return;

    char *cursor = strdup(path);
    if (!cursor) {
        free_array(parents);
        free(parents);
        return;
    }

    size_t len  = trim_trailing_slash_len(cursor, strlen(cursor));
    cursor[len] = '\0';

    while (true) {
        array_add(parents, cursor);
        if (strcmp(cursor, "/") == 0)
            break;

        char *slash = strrchr(cursor, '/');
        if (!slash)
            break;
        if (slash == cursor) {
            cursor[1] = '\0';
        } else {
            *slash = '\0';
        }
    }

    for (size_t i = parents->count; i > 0; i--) {
        run_dir_hook_with_vars(session, parents->items[i - 1], HOOK_ENTER, NULL,
                               0);
    }

    free(cursor);
    free_array(parents);
    free(parents);
}

void update_working_dir(Session *session) {
    const char *current_value = session->current_working_dir;
    char       *cwd           = getcwd(NULL, 0);

    if (cwd) {
        if (current_value && strcmp(current_value, cwd) == 0) {
            free(cwd);
        } else {
            session->current_working_dir = strdup(cwd);
            environ_set(session->environ, "PWD", session->current_working_dir);
            free(cwd);
        }
    } else if (!session->current_working_dir) {
        session->current_working_dir =
            strdup(environ_get_default(session->environ, "PWD", "."));
    } else {
        if (!session->previous_working_dir) {
            init_previous_working_dir(session);
        }
        return;
    }

#ifndef TIDESH_DISABLE_DIRSTACK
    if (!session->dirstack->stack->count) {
        // Initialize dirstack with current dir (make a copy)
        array_add(session->dirstack->stack,
              session->current_working_dir);
    }
#endif

    if (!session->previous_working_dir) {
        init_previous_working_dir(session);
        run_parent_enter_hooks(session, session->current_working_dir);
        return;
    }

    // if `session->current_working_dir` is the same as before, we did not
    // change directories, so do not update the previous working dir
    if (current_value && session->current_working_dir &&
        strcmp(current_value, session->current_working_dir) == 0) {
        return;
    }

    // Else update OLDPWD and the previous working dir
    if (session->previous_working_dir) {
        free(session->previous_working_dir);
    }

    session->previous_working_dir = (char *)current_value;
    if (session->previous_working_dir) {
        environ_set(session->environ, "OLDPWD", session->previous_working_dir);
    }

    if (current_value && session->current_working_dir) {
        HookEnvVar dir_vars[] = {
            {"TIDE_FROM", current_value ? current_value : ""},
            {"TIDE_TO",
             session->current_working_dir ? session->current_working_dir : ""}};
        HookEnvVar child_vars[] = {
            {"TIDE_CHILD",
             session->current_working_dir ? session->current_working_dir : ""}};
        HookEnvVar exit_child_vars[] = {
            {"TIDE_CHILD", current_value ? current_value : ""}};

        bool moved_down =
            is_descendant_path(current_value, session->current_working_dir);
        bool moved_up =
            is_descendant_path(session->current_working_dir, current_value);

        // Get parent directory of target for TIDE_PARENT
        char  *target_dir = session->current_working_dir;
        char  *parent_dir = strdup(target_dir);
        size_t parent_len = strlen(parent_dir);
        if (parent_len > 1 && parent_dir[parent_len - 1] == '/') {
            parent_dir[parent_len - 1] = '\0';
            parent_len--;
        }
        char *last_slash = strrchr(parent_dir, '/');
        if (last_slash == parent_dir) {
            parent_dir[1] = '\0';
        } else if (last_slash) {
            *last_slash = '\0';
        } else {
            strcpy(parent_dir, "/");
        }

        HookEnvVar cd_with_parent[] = {
            {"TIDE_DIR",
             session->current_working_dir ? session->current_working_dir : ""},
            {"TIDE_FROM", current_value ? current_value : ""},
            {"TIDE_TO",
             session->current_working_dir ? session->current_working_dir : ""},
            {"TIDE_PARENT", parent_dir}};

        if (moved_down) {
            run_dir_hook_with_vars(session, current_value, HOOK_ENTER_CHILD,
                                   child_vars, 1);
            run_dir_hook_with_vars(session, session->current_working_dir,
                                   HOOK_ENTER, dir_vars, 2);
        } else if (moved_up) {
            run_dir_hook_with_vars(session, current_value, HOOK_EXIT, dir_vars,
                                   2);
            run_dir_hook_with_vars(session, session->current_working_dir,
                                   HOOK_EXIT_CHILD, exit_child_vars, 1);
        } else {
            run_dir_hook_with_vars(session, current_value, HOOK_EXIT, dir_vars,
                                   2);
            run_dir_hook_with_vars(session, session->current_working_dir,
                                   HOOK_ENTER, dir_vars, 2);
        }

        run_cwd_hook_with_vars(session, HOOK_CD, cd_with_parent, 4);
        free(parent_dir);
    }
}

void update_path(Session *session) {
    char *original_path = environ_get(session->environ, "PATH");
    if (!original_path) {
        return;
    }

    if (session->path_commands) {
        free_trie(session->path_commands);
        free(session->path_commands);
    }

    session->path_commands = init_trie(NULL);
    if (!session->path_commands) {
        return;
    }

    char *path  = strdup(original_path);
    char *token = strtok(path, ":");
    while (token) {
        // Open directory and read entries
        DIR *dir = opendir(token);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                // Skip . and ..
                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                // Build full path
                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", token,
                         entry->d_name);

                // Check if executable
                if (access(full_path, X_OK) == 0) {
                    trie_set(session->path_commands, entry->d_name, full_path);
                }
            }
            closedir(dir);
        }
        token = strtok(NULL, ":");
    }
    free(path);
}
