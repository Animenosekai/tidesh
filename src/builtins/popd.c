#include <stdio.h> /* printf */

#include "builtins/popd.h"
#include "dirstack.h" /* dirstack_popd */
#include "session.h"  /* Session, update_working_dir */

#ifndef TIDESH_DISABLE_DIRSTACK

int builtin_popd(int argc, char **argv, Session *session) {
    if (!session->features.directory_stack) {
        fprintf(stderr, "tidesh: directory stack not enabled\n");
        return 127;
    }
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

#endif
