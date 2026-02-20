#include <stdbool.h> /* bool */
#include <stdint.h>  /* uintptr_t */
#include <stdio.h>   /* fprintf */
#include <stdlib.h>  /* strtoull */
#include <string.h>  /* strcmp */

#include "ast.h" /* ASTNode, free_ast */
#include "builtins/unset.h"
#include "data/trie.h" /* trie_get, trie_delete_key */
#include "environ.h"   /* environ_remove */
#include "session.h"   /* Session */

int builtin_unset(int argc, char **argv, Session *session) {
    if (argc == 1) {
        fprintf(stderr, "unset: usage: unset [-v] [-f] name [name ...]\n");
        return 1;
    }

    bool unset_vars  = false;
    bool unset_funcs = false;
    int  start_idx   = 1;

    // Parse flags
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-v") == 0) {
                unset_vars = true;
                start_idx  = i + 1;
            } else if (strcmp(argv[i], "-f") == 0) {
                unset_funcs = true;
                start_idx   = i + 1;
            } else if (strcmp(argv[i], "--") == 0) {
                start_idx = i + 1;
                break;
            } else {
                fprintf(stderr, "unset: invalid option: %s\n", argv[i]);
                fprintf(stderr, "unset: usage: unset [-v] [-f] name [name "
                                "...]\n");
                return 1;
            }
        } else {
            break;
        }
    }

    // Default: unset variables if no flag specified
    if (!unset_vars && !unset_funcs) {
        unset_vars = true;
    }

    if (start_idx >= argc) {
        fprintf(stderr, "unset: usage: unset [-v] [-f] name [name ...]\n");
        return 1;
    }

    int status = 0;
    for (int i = start_idx; i < argc; i++) {
        bool found = false;

#ifndef TIDESH_DISABLE_FUNCTIONS
        // Try to unset function if -f flag is set
        if (unset_funcs && session->features.functions) {
            char *encoded = trie_get(session->functions, argv[i]);
            if (encoded) {
                // Get the AST node and free it
                uintptr_t ptr  = (uintptr_t)strtoull(encoded, NULL, 16);
                ASTNode  *body = (ASTNode *)ptr;
                if (body) {
                    free_ast(body);
                    free(body);
                }

                // Remove from trie
                if (trie_delete_key(session->functions, argv[i])) {
                    found = true;
                }
            }
        }
#endif

        // Try to unset variable if -v flag is set
        if (unset_vars) {
            if (environ_remove(session->environ, argv[i])) {
                found = true;
            }
        }

        // if (!found) {
        //     fprintf(stderr, "tidesh: unset: %s: not found\n", argv[i]);
        //     status = 1;
        // }
    }

    return status;
}
