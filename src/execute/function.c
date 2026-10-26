#include <ctype.h>  /* isdigit */
#include <stdint.h> /* uintptr_t */
#include <stdio.h>  /* fprintf, stderr, snprintf */
#include <stdlib.h> /* malloc, free, realloc, strdup, strtoull */
#include <string.h> /* strcmp, strchr, strndup */

#include "ast.h"
#include "environ.h"
#include "execute/function.h"
#include "session.h"

#ifndef TIDESH_DISABLE_FUNCTIONS
ASTNode *load_function_body(Trie *functions, const char *name) {
    if (!functions || !name) {
        return NULL;
    }

    char *encoded = trie_get(functions, (char *)name);
    if (!encoded) {
        return NULL;
    }

    uintptr_t ptr = (uintptr_t)strtoull(encoded, NULL, 16);
    return (ASTNode *)ptr;
}

bool store_function_body(Trie *functions, char *name, ASTNode *body) {
    if (!functions || !name || !body) {
        return false;
    }

    ASTNode *old_body = load_function_body(functions, name);

    char ptr_buf[32];
    snprintf(ptr_buf, sizeof(ptr_buf), "%p", (void *)body);
    if (!trie_set(functions, name, ptr_buf)) {
        return false;
    }

    if (old_body && old_body != body) {
        free_ast(old_body);
        free(old_body);
    }

    return true;
}

bool is_positional_param(const char *key) {
    if (!key || !*key) {
        return false;
    }
    if (strcmp(key, "#") == 0 || strcmp(key, "@") == 0 ||
        strcmp(key, "*") == 0) {
        return true;
    }
    for (const char *p = key; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }
    return true;
}

void clear_positional_params(Session *session) {
    if (!session || !session->environ) {
        return;
    }

    Array *env_array = environ_to_array(session->environ);
    if (!env_array) {
        return;
    }

    for (size_t i = 0; i < env_array->count; i++) {
        char *entry = env_array->items[i];
        char *eq    = strchr(entry, '=');
        if (!eq) {
            continue;
        }

        size_t key_len = eq - entry;
        char  *key     = strndup(entry, key_len);
        if (key) {
            if (is_positional_param(key)) {
                environ_remove(session->environ, key);
            }
            free(key);
        }
    }

    free_array(env_array);
    free(env_array);
}

PosParamBackup *backup_and_clear_positional_params(Session *session,
                                                   size_t  *count_out) {
    *count_out = 0;
    if (!session || !session->environ) {
        return NULL;
    }

    Array *env_array = environ_to_array(session->environ);
    if (!env_array) {
        return NULL;
    }

    PosParamBackup *backups = NULL;
    size_t          count   = 0;

    for (size_t i = 0; i < env_array->count; i++) {
        char *entry = env_array->items[i];
        char *eq    = strchr(entry, '=');
        if (!eq) {
            continue;
        }

        size_t key_len = eq - entry;
        char  *key     = strndup(entry, key_len);
        if (!key) {
            continue;
        }

        if (is_positional_param(key)) {
            char *val = strdup(eq + 1);
            if (val) {
                PosParamBackup *new_backups =
                    realloc(backups, (count + 1) * sizeof(PosParamBackup));
                if (new_backups) {
                    backups              = new_backups;
                    backups[count].key   = key;
                    backups[count].value = val;
                    count++;
                } else {
                    free(val);
                    free(key);
                }
            } else {
                free(key);
            }
        } else {
            free(key);
        }
    }

    free_array(env_array);
    free(env_array);

    for (size_t i = 0; i < count; i++) {
        environ_remove(session->environ, backups[i].key);
    }

    *count_out = count;
    return backups;
}

void restore_positional_params(Session *session, PosParamBackup *backups,
                               size_t count) {
    if (!session || !session->environ) {
        return;
    }

    clear_positional_params(session);

    if (backups) {
        for (size_t i = 0; i < count; i++) {
            if (backups[i].key && backups[i].value) {
                environ_set(session->environ, backups[i].key, backups[i].value);
            }
            if (backups[i].key) {
                free(backups[i].key);
            }
            if (backups[i].value) {
                free(backups[i].value);
            }
        }
        free(backups);
    }
}
#endif

int execute_function_def(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_FUNCTIONS
    char    *name      = node->argv[0];
    ASTNode *func_copy = copy_ast(node);

    // Store the AST node pointer in the trie
    if (!store_function_body(session->functions, name, func_copy)) {
        fprintf(stderr, "tidesh: failed to store function body\n");
        if (func_copy) {
            free_ast(func_copy);
            free(func_copy);
        }
        return 1;
    }
    return 0;
#else
    (void)node;
    (void)session;
    return 0;
#endif
}
