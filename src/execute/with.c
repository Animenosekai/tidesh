#include <stdio.h> /* fprintf, stderr */

#include "ast.h"
#include "context.h"
#include "environ.h"
#include "execute/redirection.h"
#include "execute/with.h"
#include "session.h"

int execute_with(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    if (!session->features.context_managers) {
        fprintf(stderr, "tidesh: context managers are disabled\n");
        return 127;
    }

    RedirectionState redir_state = {0};
    if (!apply_in_process_redirections(node, session, &redir_state)) {
        restore_in_process_redirections(&redir_state);
        environ_set_exit_status(session->environ, 1);
        return 1;
    }

    int exit_status =
        execute_with_block(session, node->with_contexts, node->left);
    restore_in_process_redirections(&redir_state);
    environ_set_exit_status(session->environ, exit_status);
    return exit_status;
#else
    (void)node;
    (void)session;
    return 127;
#endif
}
