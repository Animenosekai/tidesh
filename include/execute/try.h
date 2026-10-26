/** try.h
 *
 * Execution functions for try...catch statements.
 */

#ifndef EXECUTE_TRY_H
#define EXECUTE_TRY_H

#include "ast/node.h"
#include "session.h"

int execute_try(ASTNode *node, Session *session);

#endif /* EXECUTE_TRY_H */
