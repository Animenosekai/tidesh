#include <dirent.h>  /* DIR, opendir, readdir, closedir */
#include <limits.h>  /* PATH_MAX */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* snprintf */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* strdup, strcmp */
#include <unistd.h>  /* getcwd */

#include "data/array.h"      /* array_add, free_array */
#include "environ.h"         /* environ_get, environ_set, environ_get_default */
#include "feature-flags.h"   /* Features */
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

static void run_parent_exit_hooks(Session *session, const char *path) {
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

    // Run exit hooks in reverse order (from current dir to root)
    for (size_t i = 0; i < parents->count; i++) {
        run_dir_hook_with_vars(session, parents->items[i], HOOK_EXIT, NULL, 0);
    }

    free(cursor);
    free_array(parents);
    free(parents);
}

static void run_descendant_enter_hooks(Session *session, const char *from,
                                       const char *to) {
    if (!session || !from || !to)
        return;

    // Build list of directories from 'to' up to root
    Array *to_parents = init_array(NULL);
    if (!to_parents)
        return;

    char *cursor = strdup(to);
    if (!cursor) {
        free_array(to_parents);
        free(to_parents);
        return;
    }

    size_t len  = trim_trailing_slash_len(cursor, strlen(cursor));
    cursor[len] = '\0';

    while (true) {
        array_add(to_parents, cursor);
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

    free(cursor);

    // Normalize 'from' path for comparison
    size_t from_len        = strlen(from);
    char  *from_normalized = strdup(from);
    if (!from_normalized) {
        free_array(to_parents);
        free(to_parents);
        return;
    }
    from_len = trim_trailing_slash_len(from_normalized, from_len);
    from_normalized[from_len] = '\0';

    // Filter out directories that are ancestors of or equal to 'from'
    // We only want to call enter hooks on new directories
    Array *new_dirs = init_array(NULL);
    if (!new_dirs) {
        free(from_normalized);
        free_array(to_parents);
        free(to_parents);
        return;
    }

    for (size_t i = 0; i < to_parents->count; i++) {
        const char *dir     = to_parents->items[i];
        size_t      dir_len = strlen(dir);

        // Skip if this directory is 'from' or an ancestor of 'from'
        if (dir_len < from_len) {
            // dir is an ancestor of from, skip it
            continue;
        } else if (dir_len == from_len && strcmp(dir, from_normalized) == 0) {
            // dir is exactly 'from', skip it
            continue;
        }

        // This is a new directory we're entering
        array_add(new_dirs, (char *)dir);
    }

    // Run enter hooks in reverse order (root to child)
    for (size_t i = new_dirs->count; i > 0; i--) {
        run_dir_hook_with_vars(session, new_dirs->items[i - 1], HOOK_ENTER,
                               NULL, 0);
    }

    free(from_normalized);
    free(new_dirs); // Don't free the strings, they're owned by to_parents
    free_array(to_parents);
    free(to_parents);
}

static void run_ancestor_exit_hooks(Session *session, const char *from,
                                    const char *to) {
    if (!session || !from || !to)
        return;

    // Build list of directories from 'from' up to root
    Array *from_parents = init_array(NULL);
    if (!from_parents)
        return;

    char *cursor = strdup(from);
    if (!cursor) {
        free_array(from_parents);
        free(from_parents);
        return;
    }

    size_t len  = trim_trailing_slash_len(cursor, strlen(cursor));
    cursor[len] = '\0';

    while (true) {
        array_add(from_parents, cursor);
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

    free(cursor);

    // Normalize 'to' path for comparison
    size_t to_len        = strlen(to);
    char  *to_normalized = strdup(to);
    if (!to_normalized) {
        free_array(from_parents);
        free(from_parents);
        return;
    }
    to_len                = trim_trailing_slash_len(to_normalized, to_len);
    to_normalized[to_len] = '\0';

    // Filter out directories that are ancestors of or equal to 'to'
    // We only want to call exit hooks on directories we're actually leaving
    Array *exiting_dirs = init_array(NULL);
    if (!exiting_dirs) {
        free(to_normalized);
        free_array(from_parents);
        free(from_parents);
        return;
    }

    for (size_t i = 0; i < from_parents->count; i++) {
        const char *dir     = from_parents->items[i];
        size_t      dir_len = strlen(dir);

        // Skip if this directory is 'to' or an ancestor of 'to'
        if (dir_len < to_len) {
            // dir is an ancestor of to, skip it (we're staying in this tree)
            continue;
        } else if (dir_len == to_len && strcmp(dir, to_normalized) == 0) {
            // dir is exactly 'to', skip it (we're moving to it)
            continue;
        }

        // This is a directory we're exiting
        array_add(exiting_dirs, (char *)dir);
    }

    // Run exit hooks in order (child to root)
    for (size_t i = 0; i < exiting_dirs->count; i++) {
        run_dir_hook_with_vars(session, exiting_dirs->items[i], HOOK_EXIT, NULL,
                               0);
    }

    free(to_normalized);
    free(exiting_dirs); // Don't free the strings, they're owned by from_parents
    free_array(from_parents);
    free(from_parents);
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
        array_add(session->dirstack->stack, session->current_working_dir);
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

        // For parent-child relationships, only call hooks on the changed parts
        if (moved_down) {
            // Moving down: enter_child on parent, enter on all new
            // subdirectories No exit hook - we're not leaving, just going
            // deeper
            run_dir_hook_with_vars(session, current_value, HOOK_ENTER_CHILD,
                                   child_vars, 1);
            run_descendant_enter_hooks(session, current_value,
                                       session->current_working_dir);
        } else if (moved_up) {
            // Moving up: exit all intermediate directories, exit_child on
            // parent No enter hook on parent since we're already there
            // conceptually
            run_ancestor_exit_hooks(session, current_value,
                                    session->current_working_dir);
            run_dir_hook_with_vars(session, session->current_working_dir,
                                   HOOK_EXIT_CHILD, exit_child_vars, 1);
        } else {
            // Unrelated paths: exit all from old, enter all for new
            run_parent_exit_hooks(session, current_value);
            run_parent_enter_hooks(session, session->current_working_dir);
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
