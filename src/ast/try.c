/** try.c
 *
 * AST parsing for try...catch statements.
 */

#include <ctype.h>  /* isdigit */
#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, calloc, realloc, free, strtol */
#include <string.h> /* strcmp, strdup */

#include "ast/loop.h"
#include "ast/node.h"
#include "ast/parser.h"
#include "ast/pipeline.h"
#include "ast/try.h"
#include "session.h"

static void parse_codes_from_string(const char *str, int **codes_out,
                                    size_t *count_out, bool *is_catch_all) {
    if (!str || !*str)
        return;

    const char *p = str;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == ',' || *p == ';')) {
            p++;
        }
        if (!*p)
            break;

        if (*p == '*' || strcmp(p, "all") == 0 || strcmp(p, "_") == 0) {
            *is_catch_all = true;
            return;
        }

        if (isdigit((unsigned char)*p) ||
            (*p == '-' && isdigit((unsigned char)*(p + 1)))) {
            char *endptr   = NULL;
            long  val_long = strtol(p, &endptr, 10);
            p              = endptr;

            int  code   = (int)val_long;
            bool exists = false;
            for (size_t j = 0; j < *count_out; j++) {
                if ((*codes_out)[j] == code) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                int *new_codes =
                    realloc(*codes_out, (*count_out + 1) * sizeof(int));
                if (new_codes) {
                    *codes_out               = new_codes;
                    (*codes_out)[*count_out] = code;
                    (*count_out)++;
                }
            }
        } else {
            p++;
        }
    }
}

ASTNode *ast_parse_try(Parser *parser, Session *session) {
    LexerToken *token = parser_peek(parser);
    if (token->type != TOKEN_TRY) {
        return NULL;
    }

    // Consume 'try'
    parser_skip(parser);

    // Skip newlines or comments before opening brace / do
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    bool is_block_style = false;
    if (token->type == TOKEN_LBRACE) {
        is_block_style = true;
    } else if (token->type == TOKEN_DO) {
        is_block_style = false;
    } else {
        fprintf(stderr, "Syntax error: expected '{' after 'try'\n");
        parser->error = true;
        return NULL;
    }

    parser_skip(parser); // consume '{' or 'do'

    ASTNode *try_body = ast_parse_sequence(parser, session);

    // Expect '}' or 'done'
    LexerToken close_tok = parser_next(parser);
    if (is_block_style && close_tok.type != TOKEN_RBRACE) {
        fprintf(stderr, "Syntax error: expected '}' to close try block\n");
        parser->error = true;
    } else if (!is_block_style && close_tok.type != TOKEN_DONE) {
        fprintf(stderr, "Syntax error: expected 'done' to close try block\n");
        parser->error = true;
    }
    free_lexer_token(&close_tok);

    if (parser->error) {
        if (try_body) {
            free_ast(try_body);
            free(try_body);
        }
        return NULL;
    }

    ASTNode *try_node       = init_ast(NULL, NODE_TRY);
    try_node->left          = try_body;
    CatchClause *last_catch = NULL;

    // Parse catch blocks using lookahead so we don't consume enclosing newlines
    while (true) {
        size_t      catch_la = 0;
        LexerToken *t        = parser_peek_nth(parser, catch_la);
        while (t && (t->type == TOKEN_EOL || t->type == TOKEN_SEMICOLON ||
                     t->type == TOKEN_COMMENT)) {
            catch_la++;
            t = parser_peek_nth(parser, catch_la);
        }

        if (!t || t->type != TOKEN_CATCH) {
            break;
        }

        // Consume delimiters before 'catch' and 'catch' itself
        for (size_t k = 0; k < catch_la; k++) {
            parser_skip(parser);
        }
        parser_skip(parser); // consume 'catch'

        // Skip whitespace/comments after 'catch'
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        int   *codes        = NULL;
        size_t code_count   = 0;
        bool   is_catch_all = false;

        // Check if exit codes are specified
        if (token->type == TOKEN_LBRACE || token->type == TOKEN_DO) {
            // No exit codes specified -> catch-all block
            is_catch_all = true;
        } else if (token->type == TOKEN_LPAREN) {
            // Parenthesized exit codes: catch (1, 2)
            parser_skip(parser); // consume '('
            while (true) {
                token = parser_peek(parser);
                while (token->type == TOKEN_EOL ||
                       token->type == TOKEN_COMMENT) {
                    parser_skip(parser);
                    token = parser_peek(parser);
                }
                if (token->type == TOKEN_RPAREN || token->type == TOKEN_EOF) {
                    break;
                }
                LexerToken tok = parser_next(parser);
                parse_codes_from_string(tok.value, &codes, &code_count,
                                        &is_catch_all);
                free_lexer_token(&tok);
            }
            token = parser_peek(parser);
            if (token->type == TOKEN_RPAREN) {
                parser_skip(parser); // consume ')'
            }
        } else {
            // Words: catch 1, catch 1 2, catch 1, 2
            while (token->type != TOKEN_LBRACE && token->type != TOKEN_DO &&
                   token->type != TOKEN_EOL && token->type != TOKEN_EOF &&
                   token->type != TOKEN_SEMICOLON) {
                LexerToken tok = parser_next(parser);
                parse_codes_from_string(tok.value, &codes, &code_count,
                                        &is_catch_all);
                free_lexer_token(&tok);
                token = parser_peek(parser);
            }
        }

        if (code_count == 0) {
            is_catch_all = true;
        }

        // Skip newlines, semicolons, comments before catch block body
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
               token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        bool catch_is_block = false;
        if (token->type == TOKEN_LBRACE) {
            catch_is_block = true;
        } else if (token->type == TOKEN_DO) {
            catch_is_block = false;
        } else {
            fprintf(stderr, "Syntax error: expected '{' after 'catch'\n");
            parser->error = true;
            if (codes)
                free(codes);
            break;
        }

        parser_skip(parser); // consume '{' or 'do'

        ASTNode   *catch_body      = ast_parse_sequence(parser, session);
        LexerToken catch_close_tok = parser_next(parser);
        if (catch_is_block && catch_close_tok.type != TOKEN_RBRACE) {
            fprintf(stderr,
                    "Syntax error: expected '}' to close catch block\n");
            parser->error = true;
        } else if (!catch_is_block && catch_close_tok.type != TOKEN_DONE) {
            fprintf(stderr,
                    "Syntax error: expected 'done' to close catch block\n");
            parser->error = true;
        }
        free_lexer_token(&catch_close_tok);

        CatchClause *clause  = calloc(1, sizeof(CatchClause));
        clause->exit_codes   = codes;
        clause->code_count   = code_count;
        clause->is_catch_all = is_catch_all;
        clause->body         = catch_body;
        clause->next         = NULL;

        if (!try_node->catch_clauses) {
            try_node->catch_clauses = clause;
            last_catch              = clause;
        } else {
            last_catch->next = clause;
            last_catch       = clause;
        }

        if (parser->error) {
            break;
        }
    }

    // Optional finally block using lookahead
    size_t      fin_la = 0;
    LexerToken *fin_t  = parser_peek_nth(parser, fin_la);
    while (fin_t &&
           (fin_t->type == TOKEN_EOL || fin_t->type == TOKEN_SEMICOLON ||
            fin_t->type == TOKEN_COMMENT)) {
        fin_la++;
        fin_t = parser_peek_nth(parser, fin_la);
    }

    if (fin_t && fin_t->type == TOKEN_FINALLY) {
        for (size_t k = 0; k < fin_la; k++) {
            parser_skip(parser);
        }
        parser_skip(parser); // consume 'finally'
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        bool fin_is_block = false;
        if (token->type == TOKEN_LBRACE) {
            fin_is_block = true;
        } else if (token->type == TOKEN_DO) {
            fin_is_block = false;
        } else {
            fprintf(stderr, "Syntax error: expected '{' after 'finally'\n");
            parser->error = true;
        }

        if (!parser->error) {
            parser_skip(parser); // consume '{' or 'do'
            ASTNode   *fin_body      = ast_parse_sequence(parser, session);
            LexerToken fin_close_tok = parser_next(parser);
            if (fin_is_block && fin_close_tok.type != TOKEN_RBRACE) {
                fprintf(stderr,
                        "Syntax error: expected '}' to close finally block\n");
                parser->error = true;
            } else if (!fin_is_block && fin_close_tok.type != TOKEN_DONE) {
                fprintf(
                    stderr,
                    "Syntax error: expected 'done' to close finally block\n");
                parser->error = true;
            }
            free_lexer_token(&fin_close_tok);
            try_node->finally_body = fin_body;
        }
    }

    if (!try_node->catch_clauses && !try_node->finally_body) {
        fprintf(
            stderr,
            "Syntax error: expected 'catch' or 'finally' after try block\n");
        parser->error = true;
    }

    if (parser->error) {
        free_ast(try_node);
        free(try_node);
        return NULL;
    }

    // Parse redirections after try statement
    ast_parse_loop_redirects(parser, try_node, session);

    return try_node;
}
