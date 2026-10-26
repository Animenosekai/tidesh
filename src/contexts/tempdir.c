/** tempdir.c
 *
 * Implementation of the 'tempdir' (and 'tmpdir', 'isolated_dir') built-in
 * context manager.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "context.h"
#include "contexts/tempdir.h"
#include "environ.h"
#include "sandbox.h"
#include "session.h"

typedef struct TempDirEnvBackup {
    char *key;
    char *old_val;
    bool  had_val;
} TempDirEnvBackup;

typedef struct TempDirState {
    char             *dir_path;
    char             *saved_cwd;
    bool              did_cd;
    bool              cleanup;
    TempDirEnvBackup *env_backups;
    size_t            env_backup_count;
    FSSandbox        *sandbox_layer;
} TempDirState;

static void backup_and_set_env(Session *session, TempDirState *state,
                               const char *key, const char *val) {
    if (!session || !state || !key)
        return;

    char *existing = environ_get(session->environ, (char *)key);

    TempDirEnvBackup *new_b =
        realloc(state->env_backups,
                (state->env_backup_count + 1) * sizeof(TempDirEnvBackup));
    if (!new_b)
        return;

    state->env_backups                                  = new_b;
    state->env_backups[state->env_backup_count].key     = strdup(key);
    state->env_backups[state->env_backup_count].had_val = (existing != NULL);
    state->env_backups[state->env_backup_count].old_val =
        existing ? strdup(existing) : NULL;
    state->env_backup_count++;

    environ_set(session->environ, (char *)key, (char *)(val ? val : ""));
}

int context_enter_tempdir(Session *session, ContextArg *args, size_t argc,
                          void **state) {
    if (!session)
        return 1;

    char *custom_var  = NULL;
    bool  do_cd       = true;
    bool  cleanup     = true;
    char  prefix[128] = "tidesh_sandbox_";

    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "var") == 0 || strcmp(key, "variable") == 0 ||
                strcmp(key, "env") == 0) {
                custom_var = strdup(val);
            } else if (strcmp(key, "cd") == 0) {
                do_cd = (strcmp(val, "false") != 0 && strcmp(val, "0") != 0 &&
                         strcmp(val, "no") != 0);
            } else if (strcmp(key, "cleanup") == 0) {
                cleanup = (strcmp(val, "false") != 0 && strcmp(val, "0") != 0 &&
                           strcmp(val, "no") != 0);
            } else if (strcmp(key, "keep") == 0) {
                cleanup = !(strcmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
                            strcmp(val, "yes") == 0);
            } else if (strcmp(key, "prefix") == 0) {
                snprintf(prefix, sizeof(prefix), "%s", val);
            }
        } else {
            if (strncmp(val, "var=", 4) == 0) {
                custom_var = strdup(val + 4);
            } else if (strncmp(val, "cd=", 3) == 0) {
                do_cd =
                    (strcmp(val + 3, "false") != 0 &&
                     strcmp(val + 3, "0") != 0 && strcmp(val + 3, "no") != 0);
            } else if (strncmp(val, "cleanup=", 8) == 0) {
                cleanup =
                    (strcmp(val + 8, "false") != 0 &&
                     strcmp(val + 8, "0") != 0 && strcmp(val + 8, "no") != 0);
            } else if (strncmp(val, "prefix=", 7) == 0) {
                snprintf(prefix, sizeof(prefix), "%s", val + 7);
            } else if (val[0] != '\0') {
                if (!custom_var) {
                    custom_var = strdup(val);
                }
            }
        }
    }

    // Determine base temp directory
    const char *base_tmp = environ_get(session->environ, (char *)"TMPDIR");
    if (!base_tmp || base_tmp[0] == '\0') {
        base_tmp = "/tmp";
    }

    char template_path[PATH_MAX];
    snprintf(template_path, sizeof(template_path), "%s/%sXXXXXX", base_tmp,
             prefix);

    char *created_dir = mkdtemp(template_path);
    if (!created_dir) {
        fprintf(stderr,
                "tidesh: tempdir: failed to create temporary directory: %s\n",
                strerror(errno));
        free(custom_var);
        return 1;
    }

    TempDirState *st = calloc(1, sizeof(TempDirState));
    if (!st) {
        rmdir(created_dir);
        free(custom_var);
        return 1;
    }

    st->dir_path = strdup(created_dir);
    st->cleanup  = cleanup;
    st->did_cd   = do_cd;

    // Environment variables
    backup_and_set_env(session, st, "TMPDIR", created_dir);
    backup_and_set_env(session, st, "TEMP", created_dir);
    backup_and_set_env(session, st, "TMP", created_dir);
    backup_and_set_env(session, st, "SANDBOX_DIR", created_dir);
    if (custom_var && custom_var[0] != '\0') {
        backup_and_set_env(session, st, custom_var, created_dir);
    }
    free(custom_var);

    // Change directory if requested
    if (do_cd) {
        char buf[PATH_MAX];
        if (session->current_working_dir) {
            st->saved_cwd = strdup(session->current_working_dir);
        } else if (getcwd(buf, sizeof(buf))) {
            st->saved_cwd = strdup(buf);
        }

        if (chdir(created_dir) != 0) {
            fprintf(stderr, "tidesh: tempdir: failed to chdir to %s: %s\n",
                    created_dir, strerror(errno));
        } else {
            update_working_dir(session);
        }
    }

    // Push sandbox layer allowing RW on this tempdir
    FSSandbox *sb = fs_sandbox_create();
    if (sb) {
        fs_sandbox_add_rw_path(sb, created_dir);
        sb->temp_dir         = strdup(created_dir);
        sb->temp_dir_cleanup = cleanup;
        fs_sandbox_push(session, sb);
        st->sandbox_layer = sb;
    }

    *state = st;
    return 0;
}

int context_exit_tempdir(Session *session, int status, void *state) {
    (void)status;
    TempDirState *st = (TempDirState *)state;
    if (!session || !st)
        return 0;

    // Pop sandbox layer
    if (st->sandbox_layer) {
        FSSandbox *popped = fs_sandbox_pop(session);
        if (popped) {
            fs_sandbox_free(popped);
        }
    }

    // Restore working directory
    if (st->did_cd && st->saved_cwd) {
        if (chdir(st->saved_cwd) != 0) {
            fprintf(stderr, "tidesh: tempdir: failed to restore cwd %s: %s\n",
                    st->saved_cwd, strerror(errno));
        }
        update_working_dir(session);
    }

    // Restore environment variables
    for (size_t i = 0; i < st->env_backup_count; i++) {
        if (st->env_backups[i].had_val) {
            environ_set(session->environ, st->env_backups[i].key,
                        st->env_backups[i].old_val);
        } else {
            environ_remove(session->environ, st->env_backups[i].key);
        }
        free(st->env_backups[i].key);
        free(st->env_backups[i].old_val);
    }
    free(st->env_backups);

    // Delete temporary directory if cleanup requested
    if (st->cleanup && st->dir_path) {
        fs_sandbox_rmtree(st->dir_path);
    }

    free(st->dir_path);
    free(st->saved_cwd);
    free(st);
    return 0;
}
