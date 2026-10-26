/** conditional.h
 *
 * Execution of conditionals (if/elif/else) and case statements.
 */

#ifndef EXECUTE_CONDITIONAL_H
#define EXECUTE_CONDITIONAL_H

#include "ast.h"
#include "session.h"

#ifndef TIDESH_DISABLE_CONDITIONALS
char *expand_case_word(const char *raw_value, Session *session);
int   execute_conditional(ASTNode *node, Session *session);
int   execute_case(ASTNode *node, Session *session);
#endif

#endif /* EXECUTE_CONDITIONAL_H */
