/** with.h
 *
 * Parsing of context manager 'with' blocks.
 */

#ifndef AST_WITH_H
#define AST_WITH_H

#include "ast/parser.h"

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
ASTNode *ast_parse_with(Parser *parser, Session *session);
#endif

#endif /* AST_WITH_H */
