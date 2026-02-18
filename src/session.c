#include <dirent.h>   /* DIR, opendir, readdir, closedir */
#include <limits.h>   /* PATH_MAX */
#include <stdbool.h>  /* bool */
#include <stdio.h>    /* snprintf */
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* strdup, strcmp */
#include <sys/stat.h> /* stat, S_ISREG */
#include <time.h>     /* time */
#include <unistd.h>   /* getcwd */

#include "data/array.h"      /* array_add, free_array */
#include "data/files.h"      /* read_all */
#include "environ.h"         /* environ_get, environ_set, environ_get_default */
#include "execute.h"         /* execute_string */
#include "features.h"        /* Features */
#include "hooks.h"           /* HOOK_* */
#include "prompt/terminal.h" /* Terminal, terminal functions */
#include "session.h"         /* Session, Environ, History, Trie, DirStack */

static void session_env_change_hook(void *context, const char *key,
                                    EnvironChangeType type);
#ifndef TIDESH_DISABLE_JOB_CONTROL
static void        session_job_state_hook(void *context, const Job *job);
static const char *job_state_name(JobState state);
#endif

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
    jobs_set_state_hook(session->jobs, session_job_state_hook, session);
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
    environ_set_change_hook(session->environ, session_env_change_hook, session);
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

typedef struct HookEnvBackup {
    char *key;
    char *old_value;
    bool  had_value;
} HookEnvBackup;

static void hook_env_backup_add(HookEnvBackup **backups, size_t *count,
                                const char *key, const char *value,
                                Session *session) {
    if (!key || !value || !session)
        return;

    const char   *existing = environ_get(session->environ, (char *)key);
    HookEnvBackup backup   = {0};
    backup.key             = strdup(key);
    backup.had_value       = existing != NULL;
    backup.old_value       = existing ? strdup(existing) : NULL;

    HookEnvBackup *new_backups =
        realloc(*backups, (*count + 1) * sizeof(HookEnvBackup));
    if (!new_backups) {
        free(backup.key);
        free(backup.old_value);
        return;
    }
    *backups           = new_backups;
    (*backups)[*count] = backup;
    (*count)++;

    environ_set(session->environ, (char *)key, (char *)value);
}

static void hook_env_restore(Session *session, HookEnvBackup *backups,
                             size_t count) {
    if (!session || !backups)
        return;

    for (size_t i = count; i > 0; i--) {
        HookEnvBackup *backup = &backups[i - 1];
        if (backup->had_value) {
            environ_set(session->environ, backup->key, backup->old_value);
        } else {
            environ_remove(session->environ, backup->key);
        }
        free(backup->key);
        free(backup->old_value);
    }
    free(backups);
}

static void run_dir_hook_with_vars(Session *session, const char *dir,
                                   const char       *hook_name,
                                   const HookEnvVar *vars, size_t var_count) {
    if (!session || !dir || !hook_name)
        return;
    if (session->hooks_disabled)
        return;

    // Generate TIDE_TIMESTAMP
    char   timestamp_str[32];
    time_t now = time(NULL);
    snprintf(timestamp_str, sizeof(timestamp_str), "%ld", now);

    // Try to run wildcard "*" hook first
    char wildcard_hook_path[PATH_MAX];
    int  written = snprintf(wildcard_hook_path, sizeof(wildcard_hook_path),
                            "%s/.tide/*", dir);
    if (written > 0 && (size_t)written < sizeof(wildcard_hook_path)) {
        struct stat st;
        if (stat(wildcard_hook_path, &st) == 0 && S_ISREG(st.st_mode)) {
            FILE *hook_file = fopen(wildcard_hook_path, "r");
            if (hook_file) {
                char *content = read_all(hook_file);
                fclose(hook_file);
                if (content) {
                    bool hooks_were_disabled = session->hooks_disabled;
                    session->hooks_disabled  = true;

                    HookEnvBackup *backups      = NULL;
                    size_t         backup_count = 0;

                    // Set TIDE_HOOK to the actual hook name (not "*")
                    hook_env_backup_add(&backups, &backup_count, "TIDE_HOOK",
                                        (char *)hook_name, session);
                    hook_env_backup_add(&backups, &backup_count,
                                        "TIDE_TIMESTAMP", timestamp_str,
                                        session);

                    for (size_t i = 0; i < var_count; i++) {
                        if (!vars[i].key || !vars[i].value)
                            continue;
                        hook_env_backup_add(&backups, &backup_count,
                                            vars[i].key, vars[i].value,
                                            session);
                    }

#ifndef TIDESH_DISABLE_HISTORY
                    bool was_disabled          = session->history->disabled;
                    session->history->disabled = true;
#endif

                    execute_string(content, session);

                    hook_env_restore(session, backups, backup_count);
                    session->hooks_disabled = hooks_were_disabled;

#ifndef TIDESH_DISABLE_HISTORY
                    session->history->disabled = was_disabled;
#endif

                    free(content);
                }
            }
        }
    }

    // Now run the specific hook
    char hook_path[PATH_MAX];
    written =
        snprintf(hook_path, sizeof(hook_path), "%s/.tide/%s", dir, hook_name);
    if (written <= 0 || (size_t)written >= sizeof(hook_path))
        return;

    struct stat st;
    if (stat(hook_path, &st) != 0 || !S_ISREG(st.st_mode))
        return;

    FILE *hook_file = fopen(hook_path, "r");
    if (!hook_file) {
        fprintf(stderr, "tidesh: could not open hook: %s\n", hook_path);
        return;
    }

    char *content = read_all(hook_file);
    fclose(hook_file);
    if (!content) {
        fprintf(stderr, "tidesh: could not read hook: %s\n", hook_path);
        return;
    }

    bool hooks_were_disabled = session->hooks_disabled;
    session->hooks_disabled  = true;

    HookEnvBackup *backups      = NULL;
    size_t         backup_count = 0;
    hook_env_backup_add(&backups, &backup_count, "TIDE_HOOK", hook_name,
                        session);
    hook_env_backup_add(&backups, &backup_count, "TIDE_TIMESTAMP",
                        timestamp_str, session);

    for (size_t i = 0; i < var_count; i++) {
        if (!vars[i].key || !vars[i].value)
            continue;
        hook_env_backup_add(&backups, &backup_count, vars[i].key, vars[i].value,
                            session);
    }

#ifndef TIDESH_DISABLE_HISTORY
    bool was_disabled          = session->history->disabled;
    session->history->disabled = true;
#endif

    execute_string(content, session);

    hook_env_restore(session, backups, backup_count);
    session->hooks_disabled = hooks_were_disabled;

#ifndef TIDESH_DISABLE_HISTORY
    session->history->disabled = was_disabled;
#endif

    free(content);
}

static void run_dir_hook(Session *session, const char *dir,
                         const char *hook_name) {
    run_dir_hook_with_vars(session, dir, hook_name, NULL, 0);
}

static void session_env_change_hook(void *context, const char *key,
                                    EnvironChangeType type) {
    Session *session = context;
    if (!session || !key)
        return;

    const char *value     = environ_get(session->environ, (char *)key);
    const char *old_value = environ_get_old_value(session->environ);
    HookEnvVar  vars[]    = {{"TIDE_ENV_KEY", key},
                             {"TIDE_ENV_VALUE", value ? value : ""},
                             {"TIDE_ENV_OLD_VALUE", old_value}};

    switch (type) {
        case ENV_CHANGE_ADD:
            run_cwd_hook_with_vars(session, HOOK_ADD_ENVIRON, vars, 3);
            break;
        case ENV_CHANGE_REMOVE:
            run_cwd_hook_with_vars(session, HOOK_REMOVE_ENVIRON, vars, 3);
            break;
        case ENV_CHANGE_UPDATE:
            run_cwd_hook_with_vars(session, HOOK_CHANGE_ENVIRON, vars, 3);
            break;
    }
}

#ifndef TIDESH_DISABLE_JOB_CONTROL
static void session_job_state_hook(void *context, const Job *job) {
    Session *session = context;
    if (!session)
        return;
    const char *state = job ? job_state_name(job->state) : "";
    char        id_buf[16];
    char        pid_buf[20];
    if (job) {
        snprintf(id_buf, sizeof(id_buf), "%d", job->id);
        snprintf(pid_buf, sizeof(pid_buf), "%d", job->pid);
    } else {
        id_buf[0]  = '\0';
        pid_buf[0] = '\0';
    }
    HookEnvVar job_vars[] = {{"TIDE_JOB_ID", id_buf},
                             {"TIDE_JOB_PID", pid_buf},
                             {"TIDE_JOB_STATE", state}};
    run_cwd_hook_with_vars(session, HOOK_AFTER_JOB, job_vars, 3);
}

static const char *job_state_name(JobState state) {
    switch (state) {
        case JOB_RUNNING:
            return "running";
        case JOB_STOPPED:
            return "stopped";
        case JOB_DONE:
            return "done";
        case JOB_KILLED:
            return "killed";
    }
    return "";
}
#endif

void run_cwd_hook(Session *session, const char *hook_name) {
    if (!session || !session->current_working_dir)
        return;

    run_dir_hook_with_vars(session, session->current_working_dir, hook_name,
                           NULL, 0);
}

void run_cwd_hook_with_vars(Session *session, const char *hook_name,
                            const HookEnvVar *vars, size_t var_count) {
    if (!session || !session->current_working_dir)
        return;

    run_dir_hook_with_vars(session, session->current_working_dir, hook_name,
                           vars, var_count);
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
        array_add(parents, strdup(cursor));
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
        run_dir_hook(session, parents->items[i - 1], HOOK_ENTER);
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
                  strdup(session->current_working_dir));
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
