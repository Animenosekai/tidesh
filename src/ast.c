#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* NULL */

#include "ast.h"
#include "lexer.h"
#include "session.h"

/* Peek at the next token without consuming it */
LexerToken *parser_peek(Parser *parser) {
    if (!parser->has_token) {
        parser->current_token = lexer_next_token(parser->lexer);
        parser->has_token     = true;
    }
    return &parser->current_token;
}

/* Peek at the token after the next token without consuming either */
LexerToken *parser_peek2(Parser *parser) {
    if (!parser->has_token) {
        parser->current_token = lexer_next_token(parser->lexer);
        parser->has_token     = true;
    }
    if (!parser->has_lookahead) {
        parser->lookahead_token = lexer_next_token(parser->lexer);
        parser->has_lookahead   = true;
    }
    return &parser->lookahead_token;
}

/* Consume and return the next token */
LexerToken parser_next(Parser *parser) {
    if (!parser->has_token) {
        return lexer_next_token(parser->lexer);
    }
    LexerToken token = parser->current_token;
    if (parser->has_lookahead) {
        parser->current_token = parser->lookahead_token;
        parser->has_lookahead = false;
    } else {
        parser->has_token = false;
    }
    return token;
}

/* Consume the next token without returning it */
void parser_skip(Parser *parser) {
    LexerToken token = parser_next(parser);
    free_lexer_token(&token);
}

ASTNode *parse(LexerInput *lexer, Session *session) {
    Parser   parser = {.lexer         = lexer,
                       .has_token     = false,
                       .has_lookahead = false,
                       .error         = false};
    ASTNode *tree   = ast_parse_sequence(&parser, session);
    if (parser.has_token)
        free_lexer_token(&parser.current_token);
    if (parser.has_lookahead)
        free_lexer_token(&parser.lookahead_token);
    return tree;
}
