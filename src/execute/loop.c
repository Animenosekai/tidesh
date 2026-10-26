#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* snprintf */
#include <stdlib.h>  /* atoi, free */

#include "ast.h"
#include "environ.h"
#include "execute.h"
#include "execute/loop.h"
#include "execute/redirection.h"
#include "expand.h"
#include "session.h"

int execute_for(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_LOOPS
    RedirectionState redir_state = {0};
    if (!apply_in_process_redirections(node, session, &redir_state)) {
        restore_in_process_redirections(&redir_state);
        environ_set_exit_status(session->environ, 1);
        return 1;
    }

    const char *var_name =
        (node->argc > 0 && node->argv) ? node->argv[0] : NULL;
    if (!var_name) {
        restore_in_process_redirections(&redir_state);
        environ_set_exit_status(session->environ, 1);
        return 1;
    }

    Array *items = init_array(NULL);
    if (!node->for_has_in) {
        // Use positional parameters $1, $2, ...
        char *num_str    = environ_get(session->environ, "#");
        int   num_params = num_str ? atoi(num_str) : 0;
        for (int i = 1; i <= num_params; i++) {
            char key[16];
            snprintf(key, sizeof(key), "%d", i);
            char *val = environ_get(session->environ, key);
            array_add(items, val ? val : "");
        }
    } else {
        // Expand words in argv[1..argc-1]
        for (int i = 1; i < node->argc; i++) {
            if (node->arg_is_sub && node->arg_is_sub[i] != 0) {
                array_add(items, node->argv[i]);
            } else {
                Array *expansion = full_expansion(node->argv[i], session);
                if (expansion) {
                    for (size_t j = 0; j < expansion->count; j++) {
                        array_add(items, expansion->items[j]);
                    }
                    free_array(expansion);
                    free(expansion);
                }
            }
        }
    }

    session->loop_depth++;
    int last_exit_status = 0;

    for (size_t i = 0; i < items->count; i++) {
        environ_set(session->environ, (char *)var_name, items->items[i]);
        if (node->left) {
            last_exit_status = execute(node->left, session);
        } else {
            last_exit_status = 0;
        }

        if (session->loop_continue > 0) {
            session->loop_continue--;
            if (session->loop_continue > 0) {
                // Continue was for an outer loop
                break;
            }
            // Continue was for this loop
            continue;
        }

        if (session->loop_break > 0) {
            session->loop_break--;
            break;
        }

        if (session->exit_requested || session->return_requested) {
            break;
        }
    }

    session->loop_depth--;
    free_array(items);
    free(items);

    restore_in_process_redirections(&redir_state);
    environ_set_exit_status(session->environ, last_exit_status);
    return last_exit_status;
#else
    (void)node;
    (void)session;
    return 127;
#endif
}

int execute_while_until(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_LOOPS
    RedirectionState redir_state = {0};
    if (!apply_in_process_redirections(node, session, &redir_state)) {
        restore_in_process_redirections(&redir_state);
        environ_set_exit_status(session->environ, 1);
        return 1;
    }

    session->loop_depth++;
    int last_exit_status = 0;

    while (true) {
        if (session->exit_requested || session->return_requested) {
            break;
        }

        int cond_status = 0;
        if (node->left) {
            session->ignore_errexit++;
            cond_status = execute(node->left, session);
            session->ignore_errexit--;
        }

        if (node->type == NODE_WHILE && cond_status != 0) {
            break;
        }
        if (node->type == NODE_UNTIL && cond_status == 0) {
            break;
        }

        if (session->loop_break > 0 || session->loop_continue > 0 ||
            session->exit_requested || session->return_requested) {
            break;
        }

        if (node->right) {
            last_exit_status = execute(node->right, session);
        } else {
            last_exit_status = 0;
        }

        if (session->loop_continue > 0) {
            session->loop_continue--;
            if (session->loop_continue > 0) {
                break;
            }
            continue;
        }

        if (session->loop_break > 0) {
            session->loop_break--;
            break;
        }

        if (session->exit_requested || session->return_requested) {
            break;
        }
    }

    session->loop_depth--;
    restore_in_process_redirections(&redir_state);
    environ_set_exit_status(session->environ, last_exit_status);
    return last_exit_status;
#else
    (void)node;
    (void)session;
    return 127;
#endif
}
