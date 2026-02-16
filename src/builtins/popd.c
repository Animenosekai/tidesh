#include <stdio.h>
#include <stdlib.h>

#include "builtins/popd.h"
#include "dirstack.h"
#include "session.h"

int builtin_popd(int argc, char **argv, Session *session) {
    (void)argc;
    (void)argv;

    if (!session || !session->dirstack) {
        return 1;
    }

    if (dirstack_popd(session->dirstack)) {
        update_working_dir(session);
        // Show the stack
        printf("%s", session->current_working_dir);
        for (size_t i = 0; i < session->dirstack->stack->count; i++) {
            printf(" %s", session->dirstack->stack->items[i]);
        }
        printf("\n");
        return 0;
    }

    return 1;
}
