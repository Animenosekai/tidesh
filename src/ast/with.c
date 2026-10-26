#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, calloc */
#include <string.h> /* strcmp, strdup, strlen, strchr, strndup */

#include "ast/command.h"
#include "ast/loop.h"
#include "ast/node.h"
#include "ast/parser.h"
#include "ast/pipeline.h"
#include "ast/with.h"
#include "context.h"
#include "session.h"

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
ASTNode *ast_parse_with(Parser *parser, Session *session) {
    LexerToken *token = parser_peek(parser);
    if (token->type != TOKEN_WITH) {
        return NULL;
    }

    // Consume 'with'
    parser_skip(parser);

    ContextInvocation *first_inv = NULL;
    ContextInvocation *last_inv  = NULL;

    // Parse one or more context manager invocations
    while (true) {
        // Skip newlines, comments
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        // Check if we hit the block opener
        if (token->type == TOKEN_LBRACE || token->type == TOKEN_DO ||
            token->type == TOKEN_EOF) {
            break;
        }

        // Optional comma separator between invocations
        if (token->type == TOKEN_WORD && strcmp(token->value, ",") == 0) {
            parser_skip(parser);
            continue;
        }

        // We expect an identifier for the context manager name
        if (token->type != TOKEN_WORD) {
            fprintf(stderr, "Syntax error: expected context manager name in "
                            "'with' statement\n");
            parser->error = true;
            break;
        }

        LexerToken name_tok = parser_next(parser);
        char *mgr_name = name_tok.value ? strdup(name_tok.value) : strdup("");
        free_lexer_token(&name_tok);

        // Strip any trailing comma or '(' from manager name if present
        size_t nlen               = strlen(mgr_name);
        bool   has_attached_paren = false;
        if (nlen > 0 && mgr_name[nlen - 1] == '(') {
            mgr_name[nlen - 1] = '\0';
            has_attached_paren = true;
        } else if (nlen > 0 && mgr_name[nlen - 1] == ',') {
            mgr_name[nlen - 1] = '\0';
        }

        ContextInvocation *inv = init_context_invocation(mgr_name);
        free(mgr_name);

        if (!first_inv) {
            first_inv = inv;
            last_inv  = inv;
        } else {
            last_inv->next = inv;
            last_inv       = inv;
        }

        // Check if arguments follow in parentheses: '('
        token = parser_peek(parser);
        if (has_attached_paren || token->type == TOKEN_LPAREN) {
            if (!has_attached_paren) {
                parser_skip(parser); // consume '('
            }

            // Parse arguments until ')'
            while (true) {
                token = parser_peek(parser);
                while (token->type == TOKEN_EOL ||
                       token->type == TOKEN_COMMENT) {
                    parser_skip(parser);
                    token = parser_peek(parser);
                }

                if (token->type == TOKEN_EOF || parser->error) {
                    fprintf(stderr, "Syntax error: unexpected EOF in context "
                                    "manager arguments\n");
                    parser->error = true;
                    break;
                }

                if (token->type == TOKEN_RPAREN) {
                    parser_skip(parser); // consume ')'
                    break;
                }

                // If comma, skip it
                if (token->type == TOKEN_WORD &&
                    strcmp(token->value, ",") == 0) {
                    parser_skip(parser);
                    continue;
                }

                // Could be TOKEN_ASSIGNMENT: KEY=VAL
                if (token->type == TOKEN_ASSIGNMENT) {
                    LexerToken assign_tok = parser_next(parser);
                    char      *val = assign_tok.extra ? assign_tok.extra : "";
                    char      *val_copy = strdup(val);
                    size_t     vlen     = strlen(val_copy);
                    if (vlen > 0 && val_copy[vlen - 1] == ',') {
                        val_copy[vlen - 1] = '\0';
                    }
                    context_invocation_add_arg(inv, assign_tok.value, val_copy);
                    free(val_copy);
                    free_lexer_token(&assign_tok);
                    continue;
                }

                // Could be TOKEN_WORD
                if (token->type == TOKEN_WORD) {
                    LexerToken w_tok = parser_next(parser);
                    char *w = w_tok.value ? strdup(w_tok.value) : strdup("");
                    free_lexer_token(&w_tok);

                    // Check if followed by '=': KEY = VAL or KEY =VAL
                    LexerToken *next_tok = parser_peek(parser);
                    if (next_tok->type == TOKEN_WORD && next_tok->value &&
                        next_tok->value[0] == '=') {
                        if (strcmp(next_tok->value, "=") == 0) {
                            parser_skip(parser); // consume '='
                            token = parser_peek(parser);
                            while (token->type == TOKEN_EOL ||
                                   token->type == TOKEN_COMMENT) {
                                parser_skip(parser);
                                token = parser_peek(parser);
                            }

                            char *val_str       = "";
                            char *val_allocated = NULL;
                            if (token->type == TOKEN_WORD ||
                                token->type == TOKEN_ASSIGNMENT) {
                                LexerToken  v_tok = parser_next(parser);
                                const char *src_val =
                                    (v_tok.type == TOKEN_ASSIGNMENT &&
                                     v_tok.extra)
                                        ? v_tok.extra
                                        : (v_tok.value ? v_tok.value : "");
                                val_allocated = strdup(src_val);
                                free_lexer_token(&v_tok);
                                size_t vlen = strlen(val_allocated);
                                if (vlen > 0 &&
                                    val_allocated[vlen - 1] == ',') {
                                    val_allocated[vlen - 1] = '\0';
                                }
                                val_str = val_allocated;
                            }

                            context_invocation_add_arg(inv, w, val_str);
                            free(val_allocated);
                            free(w);
                            continue;
                        } else {
                            // next_tok->value starts with '=', e.g. "=VAL"
                            LexerToken eq_tok = parser_next(parser);
                            char      *v_val  = eq_tok.value + 1; // skip '='
                            char      *v_copy = strdup(v_val);
                            size_t     vlen   = strlen(v_copy);
                            if (vlen > 0 && v_copy[vlen - 1] == ',') {
                                v_copy[vlen - 1] = '\0';
                            }
                            context_invocation_add_arg(inv, w, v_copy);
                            free(v_copy);
                            free_lexer_token(&eq_tok);
                            free(w);
                            continue;
                        }
                    }

                    // Check if w contains '=': KEY=VAL or KEY= VAL
                    char *eq = strchr(w, '=');
                    if (eq) {
                        size_t klen   = (size_t)(eq - w);
                        char  *k      = strndup(w, klen);
                        char  *v      = eq + 1;
                        char  *v_copy = NULL;

                        if (*v == '\0') {
                            // Trailing '=', next token is VAL
                            token = parser_peek(parser);
                            while (token->type == TOKEN_EOL ||
                                   token->type == TOKEN_COMMENT) {
                                parser_skip(parser);
                                token = parser_peek(parser);
                            }
                            if (token->type == TOKEN_WORD ||
                                token->type == TOKEN_ASSIGNMENT) {
                                LexerToken  v_tok = parser_next(parser);
                                const char *src_val =
                                    (v_tok.type == TOKEN_ASSIGNMENT &&
                                     v_tok.extra)
                                        ? v_tok.extra
                                        : (v_tok.value ? v_tok.value : "");
                                v_copy = strdup(src_val);
                                free_lexer_token(&v_tok);
                            } else {
                                v_copy = strdup("");
                            }
                        } else {
                            v_copy = strdup(v);
                        }

                        size_t vlen = strlen(v_copy);
                        if (vlen > 0 && v_copy[vlen - 1] == ',') {
                            v_copy[vlen - 1] = '\0';
                        }
                        context_invocation_add_arg(inv, k, v_copy);
                        free(k);
                        free(v_copy);
                        free(w);
                        continue;
                    }

                    // Positional argument
                    size_t wlen = strlen(w);
                    if (wlen > 0 && w[wlen - 1] == ',') {
                        w[wlen - 1] = '\0';
                    }
                    context_invocation_add_arg(inv, NULL, w);
                    free(w);
                    continue;
                }

                // Any other token inside ()
                LexerToken other_tok = parser_next(parser);
                if (other_tok.value) {
                    char  *w    = strdup(other_tok.value);
                    size_t wlen = strlen(w);
                    if (wlen > 0 && w[wlen - 1] == ',') {
                        w[wlen - 1] = '\0';
                    }
                    context_invocation_add_arg(inv, NULL, w);
                    free(w);
                }
                free_lexer_token(&other_tok);
            }
        }

        // Optional trailing comma after manager invocation
        token = parser_peek(parser);
        if (token->type == TOKEN_WORD && strcmp(token->value, ",") == 0) {
            parser_skip(parser);
        }
    }

    if (parser->error || !first_inv) {
        if (!first_inv) {
            fprintf(
                stderr,
                "Syntax error: expected context manager in 'with' statement\n");
            parser->error = true;
        }
        if (first_inv) {
            free_context_invocations(first_inv);
        }
        return NULL;
    }

    // Skip newlines or semicolons before body
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
           token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // Expect '{' or 'do'
    ASTNode *body = NULL;
    if (token->type == TOKEN_LBRACE) {
        parser_skip(parser); // consume '{'
        body                 = ast_parse_sequence(parser, session);
        LexerToken close_tok = parser_next(parser);
        if (close_tok.type != TOKEN_RBRACE) {
            fprintf(stderr,
                    "Syntax error: expected '}' to close 'with' block\n");
            parser->error = true;
        }
        free_lexer_token(&close_tok);
    } else if (token->type == TOKEN_DO) {
        parser_skip(parser); // consume 'do'
        body                 = ast_parse_sequence(parser, session);
        LexerToken close_tok = parser_next(parser);
        if (close_tok.type != TOKEN_DONE) {
            fprintf(stderr,
                    "Syntax error: expected 'done' to close 'with' block\n");
            parser->error = true;
        }
        free_lexer_token(&close_tok);
    } else {
        fprintf(stderr, "Syntax error: expected '{' or 'do' after context "
                        "managers in 'with'\n");
        parser->error = true;
    }

    if (parser->error) {
        free_context_invocations(first_inv);
        if (body) {
            free_ast(body);
            free(body);
        }
        return NULL;
    }

    ASTNode *with_node       = init_ast(NULL, NODE_WITH);
    with_node->with_contexts = first_inv;
    with_node->left          = body;

    // Parse any redirections after the block
    ast_parse_loop_redirects(parser, with_node, session);

    return with_node;
}
#endif
