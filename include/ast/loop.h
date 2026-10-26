/** loop.h
 *
 * Parsing of loops (for, while, until) and loop redirections.
 */

#ifndef AST_LOOP_H
#define AST_LOOP_H

#include <stdbool.h> /* bool */

#include "ast/parser.h"

#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS) || \
    !defined(TIDESH_DISABLE_CONTEXT_MANAGERS)
void ast_parse_loop_redirects(Parser *parser, ASTNode *node, Session *session);
#endif

#ifndef TIDESH_DISABLE_LOOPS
bool     is_valid_var_name(const char *name);
ASTNode *ast_parse_for(Parser *parser, Session *session);
ASTNode *ast_parse_while(Parser *parser, Session *session);
ASTNode *ast_parse_until(Parser *parser, Session *session);
ASTNode *ast_parse_while_or_until(Parser *parser, Session *session,
                                  bool is_until);
#endif

#endif /* AST_LOOP_H */
