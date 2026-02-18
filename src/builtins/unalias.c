#include <stdio.h> /* fprintf */

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
        char *alias_value = trie_get(session->aliases, argv[i]);
        if (!trie_delete_key(session->aliases, argv[i])) {
            fprintf(stderr, "tidesh: unalias: %s: not found\n", argv[i]);
            status = 1;
        } else {
            HookEnvVar alias_vars[] = {
                {"TIDE_ALIAS_NAME", argv[i]},
                {"TIDE_ALIAS_VALUE", alias_value ? alias_value : ""}};
            run_cwd_hook_with_vars(session, HOOK_REMOVE_ALIAS, alias_vars, 2);
        }
    }

    return status;
}

#endif
