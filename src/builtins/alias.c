#include <stdio.h>  /* printf, fprintf, perror */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* strchr, strncpy */

#include "builtins/alias.h"
#include "data/array.h" /* Array, free_array */
#include "data/trie.h"  /* trie_starting_with, trie_get, trie_set */
#include "hooks.h"      /* HOOK_ALIAS_ADD */
#include "session.h"    /* Session */

#ifndef TIDESH_DISABLE_ALIASES

int builtin_alias(int argc, char **argv, Session *session) {
    if (!session->features.alias_expansion) {
        fprintf(stderr, "tidesh: aliases not enabled\n");
        return 127;
    }

    if (argc == 1) {
        Array *keys = trie_starting_with(session->aliases, "");
        if (keys) {
            for (size_t i = 0; i < keys->count; i++) {
                char *name  = keys->items[i];
                char *value = trie_get(session->aliases, name);
                if (value) {
                    printf("alias %s='%s'\n", name, value);
                }
            }
            free_array(keys);
            free(keys);
        }
        return 0;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *eq  = strchr(arg, '=');

        if (eq) {
            // alias name=value
            size_t name_len = eq - arg;
            char  *name     = malloc(name_len + 1);
            if (!name) {
                perror("malloc");
                return 1;
            }
            strncpy(name, arg, name_len);
            name[name_len] = '\0';
            char *value    = eq + 1;

            if (!trie_set(session->aliases, name, value)) {
                fprintf(stderr, "alias: failed to set alias %s\n", name);
                status = 1;
            } else {
                run_cwd_hook(session, HOOK_ALIAS_ADD);
            }
            free(name);
        } else {
            // alias name
            char *value = trie_get(session->aliases, arg);
            if (value) {
                printf("alias %s='%s'\n", arg, value);
            } else {
                fprintf(stderr, "alias: %s: not found\n", arg);
                status = 1;
            }
        }
    }

    return status;
}

#endif
