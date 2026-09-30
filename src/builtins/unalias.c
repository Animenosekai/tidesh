#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* free */
#include <string.h> /* strdup */

#include "builtins/unalias.h"
#include "data/trie.h" /* trie_delete_key */
#include "hooks.h"     /* HOOK_REMOVE_ALIAS */
#include "session.h"   /* Session */

#ifndef TIDESH_DISABLE_ALIASES

int builtin_unalias(int argc, char **argv, Session *session) {
    if (argc == 1) {
        fprintf(stderr, "unalias: usage: unalias name [name ...]\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        char *raw_value   = trie_get(session->aliases, argv[i]);
        char *alias_value = raw_value ? strdup(raw_value) : NULL;
        if (!trie_delete_key(session->aliases, argv[i])) {
            // fprintf(stderr, "tidesh: unalias: %s: not found\n", argv[i]);
            status = 1;
        } else {
            HookEnvVar alias_vars[] = {
                {"TIDE_ALIAS_NAME", argv[i]},
                {"TIDE_ALIAS_VALUE", alias_value ? alias_value : ""}};
            run_cwd_hook_with_vars(session, HOOK_REMOVE_ALIAS, alias_vars, 2);
        }
        free(alias_value);
    }

    return status;
}

#endif
