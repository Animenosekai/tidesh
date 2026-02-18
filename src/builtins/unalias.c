#include <stdio.h> /* fprintf */

#include "builtins/unalias.h"
#include "data/trie.h" /* trie_delete_key */
#include "hooks.h"     /* HOOK_ALIAS_REMOVE */
#include "session.h"   /* Session */

#ifndef TIDESH_DISABLE_ALIASES

int builtin_unalias(int argc, char **argv, Session *session) {
    if (argc == 1) {
        fprintf(stderr, "unalias: usage: unalias name [name ...]\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (!trie_delete_key(session->aliases, argv[i])) {
            fprintf(stderr, "tidesh: unalias: %s: not found\n", argv[i]);
            status = 1;
        } else {
            run_cwd_hook(session, HOOK_ALIAS_REMOVE);
        }
    }

    return status;
}

#endif
