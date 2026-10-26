#include <fnmatch.h> /* fnmatch */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* NULL */
#include <stdlib.h>  /* free */

#include "ast.h"
#include "environ.h"
#include "execute.h"
#include "execute/command.h"
#include "execute/conditional.h"
#include "execute/redirection.h"
#include "session.h"

#ifndef TIDESH_DISABLE_CONDITIONALS
char *expand_case_word(const char *raw_value, Session *session) {
    return expand_word_no_split(raw_value, session);
}

int execute_conditional(ASTNode *node, Session *session) {
    RedirectionState redir_state = {0};
    if (!apply_in_process_redirections(node, session, &redir_state)) {
        restore_in_process_redirections(&redir_state);
        environ_set_exit_status(session->environ, 1);
        return 1;
    }

    int exit_status = 0;

    // Iterate through branches
    ConditionalBranch *branch = node->branches;
    while (branch) {
        if (branch->condition == NULL) {
            // This is an 'else' branch
            if (branch->body) {
                exit_status = execute(branch->body, session);
            } else {
                exit_status = 0;
            }
            restore_in_process_redirections(&redir_state);
            environ_set_exit_status(session->environ, exit_status);
            if (session->try_depth > 0 && session->ignore_errexit == 0 &&
                !session_is_exit_code_allowed(session, exit_status)) {
                session->try_caught    = true;
                session->try_exit_code = exit_status;
                return exit_status;
            }
            if (session->options.errexit && exit_status != 0 &&
                session->ignore_errexit == 0) {
                session->exit_requested = true;
            }
            return exit_status;
        } else {
            // This is an 'if' or 'elif' branch
            session->ignore_errexit++;
            int condition_status = execute(branch->condition, session);
            session->ignore_errexit--;
            if (session->return_requested) {
                restore_in_process_redirections(&redir_state);
                return session->return_status;
            }
            if (condition_status == 0) {
                // Condition succeeded, execute body
                if (branch->body) {
                    exit_status = execute(branch->body, session);
                } else {
                    exit_status = 0;
                }
                restore_in_process_redirections(&redir_state);
                environ_set_exit_status(session->environ, exit_status);
                if (session->try_depth > 0 && session->ignore_errexit == 0 &&
                    !session_is_exit_code_allowed(session, exit_status)) {
                    session->try_caught    = true;
                    session->try_exit_code = exit_status;
                    return exit_status;
                }
                if (session->options.errexit && exit_status != 0 &&
                    session->ignore_errexit == 0) {
                    session->exit_requested = true;
                }
                return exit_status;
            }
        }
        branch = branch->next;
    }

    restore_in_process_redirections(&redir_state);
    // No condition matched, return 0 (or the last condition's status)
    environ_set_exit_status(session->environ, 0);
    return 0;
}

int execute_case(ASTNode *node, Session *session) {
    RedirectionState redir_state = {0};
    if (!apply_in_process_redirections(node, session, &redir_state)) {
        restore_in_process_redirections(&redir_state);
        environ_set_exit_status(session->environ, 1);
        return 1;
    }

    const char *raw_word = (node->argc > 0 && node->argv) ? node->argv[0] : "";
    char       *expanded_word = expand_case_word(raw_word, session);

    int  last_exit_status = 0;
    bool fallthrough      = false;

    CaseItem *item = node->case_items;
    while (item) {
        if (session->exit_requested || session->return_requested) {
            break;
        }

        bool matched = false;
        if (fallthrough) {
            matched     = true;
            fallthrough = false;
        } else {
            for (int p = 0; p < item->pattern_count; p++) {
                char *exp_pat = expand_case_word(item->patterns[p], session);
                if (fnmatch(exp_pat, expanded_word, 0) == 0) {
                    matched = true;
                    free(exp_pat);
                    break;
                }
                free(exp_pat);
            }
        }

        if (matched) {
            if (item->body) {
                last_exit_status = execute(item->body, session);
            } else {
                last_exit_status = 0;
            }

            if (session->exit_requested || session->return_requested ||
                session->loop_break > 0 || session->loop_continue > 0) {
                break;
            }

            if (item->terminator == CASE_TERM_BREAK) {
                break;
            } else if (item->terminator == CASE_TERM_FALLTHROUGH) {
                fallthrough = true;
            } else if (item->terminator == CASE_TERM_CONTINUE) {
                // Continue matching subsequent clauses
            }
        }

        item = item->next;
    }

    free(expanded_word);
    restore_in_process_redirections(&redir_state);
    environ_set_exit_status(session->environ, last_exit_status);
    return last_exit_status;
}
#endif
