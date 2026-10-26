/** loop.h
 *
 * Execution of loops (for, while, until).
 */

#ifndef EXECUTE_LOOP_H
#define EXECUTE_LOOP_H

#include "ast.h"
#include "session.h"

int execute_for(ASTNode *node, Session *session);
int execute_while_until(ASTNode *node, Session *session);

#endif /* EXECUTE_LOOP_H */
