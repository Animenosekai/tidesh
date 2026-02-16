#include <stdbool.h> /* bool */
#include <stdio.h>   /* printf */
#include <stdlib.h>  /* malloc, free */

#include "builtin.h" /* is_builtin */
#include "builtins/which.h"
#include "data/trie.h" /* trie_get */
#include "execute.h"   /* find_in_path */
#include "session.h"   /* Session */

int builtin_which(int argc, char **argv, Session *session) {
    if (argc < 2) {
        fprintf(stderr, "which: missing operand\n");
        return 1;
    }

    int exit_status = 0;
    for (int i = 1; i < argc; i++) {
        char *cmd_name  = argv[i];
        char *alias_val = trie_get(session->aliases, cmd_name);
        if (alias_val) {
            printf("%s: aliased to %s\n", cmd_name, alias_val);
            continue;
        }
        if (is_special_builtin(cmd_name)) {
            printf("%s: shell special built-in command\n", cmd_name);
            continue;
        }
        if (is_builtin(cmd_name)) {
            printf("%s: shell built-in command\n", cmd_name);
            continue;
        }
        char *cmd_path = find_in_path(cmd_name, session);
        if (cmd_path) {
            printf("%s\n", cmd_path);
            free(cmd_path);
        } else {
            printf("%s: not found\n", cmd_name);
            exit_status = 1;
        }
    }
    return exit_status;
}
