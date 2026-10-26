#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* size_t, NULL */
#include <stdlib.h>  /* realloc, free */
#include <string.h>  /* memmove */

#include "ast.h"
#include "lexer.h"
#include "session.h"

/* Peek at the n-th next token (0-indexed) without consuming it */
LexerToken *parser_peek_nth(Parser *parser, size_t n) {
    while (parser->buf_len <= n) {
        if (parser->buf_len >= parser->buf_cap) {
            size_t new_cap = parser->buf_cap == 0 ? 4 : parser->buf_cap * 2;
            parser->buffer =
                realloc(parser->buffer, new_cap * sizeof(LexerToken));
            parser->buf_cap = new_cap;
        }
        parser->buffer[parser->buf_len++] = lexer_next_token(parser->lexer);
    }
    return &parser->buffer[n];
}

/* Peek at the next token without consuming it */
LexerToken *parser_peek(Parser *parser) { return parser_peek_nth(parser, 0); }

/* Peek at the token after the next token without consuming either */
LexerToken *parser_peek2(Parser *parser) { return parser_peek_nth(parser, 1); }

/* Consume and return the next token */
LexerToken parser_next(Parser *parser) {
    parser_peek_nth(parser, 0);
    LexerToken token = parser->buffer[0];
    if (parser->buf_len > 1) {
        memmove(parser->buffer, parser->buffer + 1,
                (parser->buf_len - 1) * sizeof(LexerToken));
    }
    parser->buf_len--;
    return token;
}

/* Consume the next token without returning it */
void parser_skip(Parser *parser) {
    LexerToken token = parser_next(parser);
    free_lexer_token(&token);
}

ASTNode *parse(LexerInput *lexer, Session *session) {
    Parser   parser = {.lexer   = lexer,
                       .buffer  = NULL,
                       .buf_len = 0,
                       .buf_cap = 0,
                       .error   = false};
    ASTNode *tree   = ast_parse_sequence(&parser, session);
    for (size_t i = 0; i < parser.buf_len; i++) {
        free_lexer_token(&parser.buffer[i]);
    }
    free(parser.buffer);
    return tree;
}
