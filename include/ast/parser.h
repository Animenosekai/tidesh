/** parser.h
 *
 * Parser state and token stream navigation functions.
 */

#ifndef AST_PARSER_H
#define AST_PARSER_H

#include <stdbool.h> /* bool */

#include "ast/node.h"
#include "lexer.h"
#include "session.h"

/* A command parser */
typedef struct Parser {
    /* The lexer input */
    LexerInput *lexer;
    /* The current token */
    LexerToken current_token;
    /* Is there a current token? */
    bool has_token;
    /* Lookahead token */
    LexerToken lookahead_token;
    bool       has_lookahead;
    /* Did an error occur? */
    bool error;
} Parser;

/* Peek at the next token without consuming it */
LexerToken *parser_peek(Parser *parser);

/* Peek at the token after the next token without consuming either */
LexerToken *parser_peek2(Parser *parser);

/* Consume and return the next token */
LexerToken parser_next(Parser *parser);

/* Consume the next token without returning it */
void parser_skip(Parser *parser);

/* Grammar parsing function prototypes */
ASTNode *ast_parse_sequence(Parser *parser, Session *session);
ASTNode *ast_parse_and_or(Parser *parser, Session *session);
ASTNode *ast_parse_pipeline(Parser *parser, Session *session);
ASTNode *ast_parse_command(Parser *parser, Session *session);

#ifndef TIDESH_DISABLE_CONDITIONALS
ASTNode *ast_parse_conditional(Parser *parser, Session *session);
ASTNode *ast_parse_case(Parser *parser, Session *session);
#endif

#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS) || \
    !defined(TIDESH_DISABLE_CONTEXT_MANAGERS)
void ast_parse_loop_redirects(Parser *parser, ASTNode *node, Session *session);
#endif

#ifndef TIDESH_DISABLE_LOOPS
ASTNode *ast_parse_for(Parser *parser, Session *session);
ASTNode *ast_parse_while(Parser *parser, Session *session);
ASTNode *ast_parse_until(Parser *parser, Session *session);
ASTNode *ast_parse_while_or_until(Parser *parser, Session *session,
                                  bool is_until);
#endif

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
ASTNode *ast_parse_with(Parser *parser, Session *session);
#endif

#endif /* AST_PARSER_H */
