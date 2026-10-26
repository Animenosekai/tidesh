/** env.c
 *
 * Implementation of the 'env' built-in context manager.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/env.h"
#include "environ.h"
#include "session.h"

typedef struct EnvBackupItem {
    char *key;
    char *old_value;
    bool  had_value;
} EnvBackupItem;

typedef struct EnvBackupState {
    EnvBackupItem *items;
    size_t         count;
} EnvBackupState;

int context_enter_env(Session *session, ContextArg *args, size_t argc,
                      void **state) {
    EnvBackupState *backup = calloc(1, sizeof(EnvBackupState));
    if (!backup)
        return 1;

    bool path_modified = false;

    for (size_t i = 0; i < argc; i++) {
        char *key      = NULL;
        char *val      = NULL;
        bool  free_key = false;

        if (args[i].key) {
            key = args[i].key;
            val = args[i].value ? args[i].value : "";
        } else if (args[i].value) {
            char *eq = strchr(args[i].value, '=');
            if (eq) {
                size_t klen = (size_t)(eq - args[i].value);
                key         = strndup(args[i].value, klen);
                val         = eq + 1;
                free_key    = true;
            } else {
                key = args[i].value;
                val = "";
            }
        }

        if (!key || key[0] == '\0') {
            if (free_key)
                free(key);
            continue;
        }

        char *existing = environ_get(session->environ, key);

        EnvBackupItem *new_items =
            realloc(backup->items, (backup->count + 1) * sizeof(EnvBackupItem));
        if (new_items) {
            backup->items                          = new_items;
            backup->items[backup->count].key       = strdup(key);
            backup->items[backup->count].had_value = (existing != NULL);
            backup->items[backup->count].old_value =
                existing ? strdup(existing) : NULL;
            backup->count++;
        }

        environ_set(session->environ, key, val);
        if (strcmp(key, "PATH") == 0) {
            path_modified = true;
        }

        if (free_key) {
            free(key);
        }
    }

    if (path_modified) {
        update_path(session);
    }

    *state = backup;
    return 0;
}

int context_exit_env(Session *session, int status, void *state) {
    (void)status;
    EnvBackupState *backup = (EnvBackupState *)state;
    if (!backup)
        return 0;

    bool path_modified = false;

    // Restore in reverse order
    for (size_t i = backup->count; i > 0; i--) {
        EnvBackupItem *item = &backup->items[i - 1];
        if (item->had_value) {
            environ_set(session->environ, item->key,
                        item->old_value ? item->old_value : "");
        } else {
            environ_remove(session->environ, item->key);
        }
        if (strcmp(item->key, "PATH") == 0) {
            path_modified = true;
        }
        free(item->key);
        free(item->old_value);
    }

    if (path_modified) {
        update_path(session);
    }

    free(backup->items);
    free(backup);
    return 0;
}
