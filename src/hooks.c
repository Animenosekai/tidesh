#include <limits.h>   /* PATH_MAX */
#include <stdbool.h>  /* bool */
#include <stdio.h>    /* snprintf, fprintf */
#include <stdlib.h>   /* malloc, free, realloc */
#include <string.h>   /* strdup */
#include <sys/stat.h> /* stat, S_ISREG */
#include <time.h>     /* time */

#include "data/files.h" /* read_all */
#include "environ.h"    /* environ_get, environ_set, environ_remove */
#include "execute.h"    /* execute_string */
#include "hooks.h"      /* HookEnvVar, HOOK_* */
#include "session.h"    /* Session */

#ifndef TIDESH_DISABLE_JOB_CONTROL
#include "jobs.h" /* jobs_set_state_hook */
#endif

static void session_env_change_hook(void *context, const char *key,
                                    EnvironChangeType type);
#ifndef TIDESH_DISABLE_JOB_CONTROL
static void        session_job_state_hook(void *context, const Job *job);
static const char *job_state_name(JobState state);
#endif

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

void run_dir_hook_with_vars(Session *session, const char *dir,
                            const char *hook_name, const HookEnvVar *vars,
                            size_t var_count) {
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
                                        hook_name, session);
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

void hooks_register_session(Session *session) {
    if (!session)
        return;

    environ_set_change_hook(session->environ, session_env_change_hook, session);

#ifndef TIDESH_DISABLE_JOB_CONTROL
    if (session->jobs) {
        jobs_set_state_hook(session->jobs, session_job_state_hook, session);
    }
#endif
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
