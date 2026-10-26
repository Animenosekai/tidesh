/** try.c
 *
 * Execution implementation for try...catch statements.
 */

#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* free */

#include "ast/node.h"
#include "environ.h"
#include "execute.h"
#include "execute/redirection.h"
#include "execute/try.h"
#include "session.h"

int execute_try(ASTNode *node, Session *session) {
    if (!node || !session)
        return 0;

    RedirectionState redir_state = {0};
    if (!apply_in_process_redirections(node, session, &redir_state)) {
        restore_in_process_redirections(&redir_state);
        environ_set_exit_status(session->environ, 1);
        return 1;
    }

    int  prev_depth     = session->try_depth;
    bool prev_caught    = session->try_caught;
    int  prev_exit_code = session->try_exit_code;

    session->try_depth++;
    session->try_caught    = false;
    session->try_exit_code = 0;

    int try_st = 0;
    if (node->left) {
        try_st = execute(node->left, session);
    }

    session->try_depth--;

    bool error_occurred = false;
    int  caught_code    = 0;

    if (session->try_caught) {
        error_occurred = true;
        caught_code    = session->try_exit_code;
    } else if (!session_is_exit_code_allowed(session, try_st)) {
        error_occurred = true;
        caught_code    = try_st;
    }

    session->try_caught    = false;
    session->try_exit_code = 0;

    int  final_st = 0;
    bool caught   = false;

    if (error_occurred) {
        // Clear any errexit exit_requested from within the try block
        session->exit_requested = false;

        environ_set_exit_status(session->environ, caught_code);
        char code_str[32];
        snprintf(code_str, sizeof(code_str), "%d", caught_code);
        environ_set(session->environ, "TIDE_TRY_STATUS", code_str);
        environ_set(session->environ, "TIDE_CATCH_STATUS", code_str);

        // Find first matching catch clause
        CatchClause *matched = NULL;
        for (CatchClause *c = node->catch_clauses; c; c = c->next) {
            if (c->is_catch_all) {
                matched = c;
                break;
            }
            for (size_t i = 0; i < c->code_count; i++) {
                if (c->exit_codes[i] == caught_code) {
                    matched = c;
                    break;
                }
            }
            if (matched) {
                break;
            }
        }

        if (matched) {
            caught = true;
            if (matched->body) {
                final_st = execute(matched->body, session);
            } else {
                final_st = 0;
            }
        } else {
            // Unhandled error!
            final_st = caught_code;
            if (session->try_depth > 0) {
                session->try_caught    = true;
                session->try_exit_code = caught_code;
            } else if (session->options.errexit &&
                       session->ignore_errexit == 0) {
                session->exit_requested = true;
            }
        }
    } else {
        final_st = try_st;
    }

    // Always run finally if present
    if (node->finally_body) {
        int saved_st   = final_st;
        int finally_st = execute(node->finally_body, session);
        if (finally_st != 0) {
            final_st = finally_st;
        } else {
            final_st = saved_st;
        }
    }

    restore_in_process_redirections(&redir_state);
    environ_set_exit_status(session->environ, final_st);

    // If an outer try block was already caught before entering here, restore it
    if (prev_depth > 0 && prev_caught && !error_occurred) {
        session->try_caught    = true;
        session->try_exit_code = prev_exit_code;
    }

    return final_st;
}
