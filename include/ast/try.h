/** try.h
 *
 * AST parsing functions for try...catch statements.
 */

#ifndef AST_TRY_H
#define AST_TRY_H

#include "ast/node.h"
#include "ast/parser.h"
#include "session.h"

ASTNode *ast_parse_try(Parser *parser, Session *session);

#endif /* AST_TRY_H */
