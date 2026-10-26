/** conditional.h
 *
 * Parsing of conditionals (if/elif/else/fi) and pattern matching
 * (case/in/esac).
 */

#ifndef AST_CONDITIONAL_H
#define AST_CONDITIONAL_H

#include "ast/parser.h"

#ifndef TIDESH_DISABLE_CONDITIONALS
ASTNode *ast_parse_conditional(Parser *parser, Session *session);
ASTNode *ast_parse_case(Parser *parser, Session *session);
#endif

#endif /* AST_CONDITIONAL_H */
