#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins/pushd.h"
#include "dirstack.h"
#include "session.h"

static bool is_numeric(const char *str) {
    if (!str || *str == '\0')
        return false;
    while (*str) {
        if (!isdigit(*str))
            return false;
        str++;
    }
    return true;
}

int builtin_pushd(int argc, char **argv, Session *session) {
    if (!session || !session->dirstack) {
        return 1;
    }

    bool success = false;
    if (argc < 2) {
        // pushd with no arguments swaps CWD with the top of the stack
        success = dirstack_swap(session->dirstack, 1);
    } else if (argv[1][0] == '+' && is_numeric(argv[1] + 1)) {
        // pushd +N
        int n   = atoi(argv[1] + 1);
        success = dirstack_swap(session->dirstack, n);
    } else {
        // pushd path
        success = dirstack_pushd(session->dirstack, argv[1]);
    }

    if (success) {
        update_working_dir(session);
        // Show the stack (standard pushd behavior)
        printf("%s", session->current_working_dir);
        for (size_t i = 0; i < session->dirstack->stack->count; i++) {
            printf(" %s", session->dirstack->stack->items[i]);
        }
        printf("\n");
        return 0;
    }

    return 1;
}
