#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins/alias.h"
#include "data/array.h"
#include "data/trie.h"
#include "session.h"

int builtin_alias(int argc, char **argv, Session *session) {
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
