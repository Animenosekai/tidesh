/** with.h
 *
 * Execution of 'with' context manager blocks.
 */

#ifndef EXECUTE_WITH_H
#define EXECUTE_WITH_H

#include "ast.h"
#include "session.h"

int execute_with(ASTNode *node, Session *session);

#endif /* EXECUTE_WITH_H */
