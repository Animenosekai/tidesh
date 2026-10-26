#include <ctype.h>  /* isalpha, isalnum */
#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, calloc, atoi */
#include <string.h> /* strcmp, strdup */

#include "ast/command.h"
#include "ast/conditional.h"
#include "ast/loop.h"
#include "ast/node.h"
#include "ast/parser.h"
#include "data/array.h"
#include "expand.h"
#include "session.h"

#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS) || \
    !defined(TIDESH_DISABLE_CONTEXT_MANAGERS)
void ast_parse_loop_redirects(Parser *parser, ASTNode *node, Session *session) {
    while (true) {
        LexerToken *token = parser_peek(parser);
        int         fd    = -1;
#ifndef TIDESH_DISABLE_REDIRECTIONS
        if (token->type == TOKEN_IO_NUMBER) {
            LexerToken io_token = parser_next(parser);
            fd                  = atoi(io_token.value);
            free_lexer_token(&io_token);
            token = parser_peek(parser);
        }

        if ((token->type >= TOKEN_REDIRECT_IN &&
             token->type <= TOKEN_HERESTRING &&
             token->type != TOKEN_PROCESS_SUBSTITUTION_IN) ||
            (token->type >= TOKEN_REDIRECT_OUT &&
             token->type <= TOKEN_REDIRECT_APPEND_ERR)) {
            Redirection *redirect = calloc(1, sizeof(Redirection));
            redirect->type        = token->type;

            if (fd != -1) {
                redirect->fd = fd;
            } else if (token->type == TOKEN_REDIRECT_IN ||
                       token->type == TOKEN_FD_DUPLICATION ||
                       token->type == TOKEN_PROCESS_SUBSTITUTION_IN ||
                       token->type == TOKEN_HEREDOC ||
                       token->type == TOKEN_HERESTRING) {
                redirect->fd = 0;
            } else {
                redirect->fd = 1;
            }

            if (token->type == TOKEN_HEREDOC ||
                token->type == TOKEN_HERESTRING ||
                token->type == TOKEN_PROCESS_SUBSTITUTION_IN ||
                token->type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
                redirect->target = strdup(token->value);
                if (token->type == TOKEN_PROCESS_SUBSTITUTION_IN ||
                    token->type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
                    redirect->is_process_substitution = true;
                }
                parser_skip(parser);
            } else {
                parser_skip(parser);
                LexerToken target = parser_next(parser);
                if (target.type == TOKEN_WORD) {
                    Array *expansion = full_expansion(target.value, session);
                    if (expansion && expansion->count > 0) {
                        redirect->target = strdup(expansion->items[0]);
                        free_array(expansion);
                        free(expansion);
                    } else {
                        redirect->target = strdup(target.value);
                    }
                } else if (target.type == TOKEN_PROCESS_SUBSTITUTION_IN ||
                           target.type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
                    redirect->target                  = strdup(target.value);
                    redirect->is_process_substitution = true;
                } else {
                    fprintf(stderr, "Syntax error: expected filename\n");
                    parser->error = true;
                }
                free_lexer_token(&target);

                if (redirect->type == TOKEN_REDIRECT_OUT_ERR) {
                    if (redirect->target &&
                        (is_all_digits(redirect->target) ||
                         strcmp(redirect->target, "-") == 0)) {
                        redirect->type = TOKEN_FD_DUPLICATION;
                    }
                } else if (redirect->type == TOKEN_REDIRECT_ALL) {
                    redirect->type = TOKEN_REDIRECT_OUT_ERR;
                }
            }

            if (!node->redirects) {
                node->redirects = redirect;
            } else {
                Redirection *curr = node->redirects;
                while (curr->next) {
                    curr = curr->next;
                }
                curr->next = redirect;
            }
            continue;
        }
#endif
        break;
    }
}
#endif

#ifndef TIDESH_DISABLE_LOOPS
bool is_valid_var_name(const char *name) {
    if (!name || !*name)
        return false;
    if (!isalpha((unsigned char)*name) && *name != '_')
        return false;
    for (const char *p = name + 1; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_')
            return false;
    }
    return true;
}

/* Parse a for loop */
ASTNode *ast_parse_for(Parser *parser, Session *session) {
    LexerToken *token = parser_peek(parser);
    if (token->type != TOKEN_FOR) {
        return NULL;
    }

    // Consume 'for'
    parser_skip(parser);

    // Skip newlines or comments before variable name
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // Optional leading '(' for C-style for (x in ...)
    bool has_paren = false;
    if (token->type == TOKEN_LPAREN) {
        has_paren = true;
        parser_skip(parser); // consume '('
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }
    }

    // Variable name
    LexerToken var_tok = parser_next(parser);
    if ((var_tok.type != TOKEN_WORD && var_tok.type != TOKEN_IN) ||
        !is_valid_var_name(var_tok.value)) {
        fprintf(stderr,
                "Syntax error: '%s' is not a valid identifier in for loop\n",
                var_tok.value ? var_tok.value : "");
        parser->error = true;
        free_lexer_token(&var_tok);
        return NULL;
    }

    ASTNode *for_node = init_ast(NULL, NODE_FOR);
    add_argument(for_node, var_tok.value, 0);
    free_lexer_token(&var_tok);

    // Skip newlines or comments before 'in' or ';' or 'do' or '{'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // Check if 'in' is present
    if (token->type == TOKEN_IN ||
        (token->type == TOKEN_WORD && strcmp(token->value, "in") == 0)) {
        for_node->for_has_in = true;
        parser_skip(parser); // consume 'in'

        // Parse wordlist until ';', newline, 'do', '{', ')', or EOF
        while (true) {
            token = parser_peek(parser);
            if (token->type == TOKEN_SEMICOLON || token->type == TOKEN_EOL ||
                token->type == TOKEN_DO || token->type == TOKEN_LBRACE ||
                token->type == TOKEN_EOF || token->type == TOKEN_COMMENT ||
                (has_paren && token->type == TOKEN_RPAREN)) {
                break;
            }

            LexerToken word     = parser_next(parser);
            int        sub_type = 0;
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
            if (word.type == TOKEN_PROCESS_SUBSTITUTION_IN) {
                sub_type = 1;
            } else if (word.type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
                sub_type = 2;
            }
#endif
            add_argument(for_node, word.value, sub_type);
            free_lexer_token(&word);
        }

        // If wordlist was terminated by ';', consume it
        token = parser_peek(parser);
        if (token->type == TOKEN_SEMICOLON) {
            parser_skip(parser);
        }
    } else if (token->type == TOKEN_SEMICOLON) {
        for_node->for_has_in = false;
        parser_skip(parser); // consume ';'
    } else {
        for_node->for_has_in = false;
    }

    if (has_paren) {
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }
        if (token->type == TOKEN_RPAREN) {
            parser_skip(parser); // consume ')'
        }
    }

    // Skip newlines, semicolons, comments before 'do' or '{'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
           token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    bool is_block_style = false;
    if (token->type == TOKEN_DO) {
        is_block_style = false;
    } else if (token->type == TOKEN_LBRACE) {
        is_block_style = true;
    } else {
        fprintf(stderr, "Syntax error: expected 'do' or '{' in for loop\n");
        parser->error = true;
        free_ast(for_node);
        free(for_node);
        return NULL;
    }

    ASTNode *body = NULL;
    if (is_block_style) {
        parser_skip(parser); // consume '{'
        body                 = ast_parse_sequence(parser, session);
        LexerToken close_tok = parser_next(parser);
        if (close_tok.type != TOKEN_RBRACE) {
            fprintf(stderr, "Syntax error: expected '}' to close for loop\n");
            parser->error = true;
        }
        free_lexer_token(&close_tok);
    } else {
        parser_skip(parser); // consume 'do'

        // Skip newlines, semicolons, comments after 'do'
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
               token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        // Parse the body commands until 'done'
        while (true) {
            token = parser_peek(parser);
            if (token->type == TOKEN_DONE) {
                break;
            }
            if (token->type == TOKEN_EOF) {
                fprintf(stderr, "Syntax error: unexpected EOF in for loop\n");
                parser->error = true;
                break;
            }

            ASTNode *cmd = ast_parse_conditional(parser, session);
            if (!cmd) {
                token = parser_peek(parser);
                if (token->type == TOKEN_EOL ||
                    token->type == TOKEN_SEMICOLON ||
                    token->type == TOKEN_COMMENT) {
                    parser_skip(parser);
                } else {
                    break;
                }
                continue;
            }

            if (!body) {
                body = cmd;
            } else {
                ASTNode *seq = init_ast(NULL, NODE_SEQUENCE);
                seq->left    = body;
                seq->right   = cmd;
                body         = seq;
            }

            token = parser_peek(parser);
            if (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
                token->type == TOKEN_COMMENT) {
                parser_skip(parser);
            }
        }

        // Expect and consume 'done'
        token = parser_peek(parser);
        if (token->type == TOKEN_DONE) {
            parser_skip(parser);
        } else {
            fprintf(stderr,
                    "Syntax error: expected 'done' to close for loop\n");
            parser->error = true;
        }
    }

    if (parser->error) {
        if (body) {
            free_ast(body);
            free(body);
        }
        free_ast(for_node);
        free(for_node);
        return NULL;
    }

    for_node->left = body;

    // Parse any redirections after 'done' or '}' (e.g. for ... done > out.txt
    // or for ... { ... } > out.txt)
    ast_parse_loop_redirects(parser, for_node, session);

    return for_node;
}

/* Parse a while or until loop */
ASTNode *ast_parse_while_or_until(Parser *parser, Session *session,
                                  bool is_until) {
    TokenType   expected_tok = is_until ? TOKEN_UNTIL : TOKEN_WHILE;
    NodeType    node_type    = is_until ? NODE_UNTIL : NODE_WHILE;
    const char *name         = is_until ? "until" : "while";

    LexerToken *token = parser_peek(parser);
    if (token->type != expected_tok) {
        return NULL;
    }

    // Consume 'while' or 'until'
    parser_skip(parser);

    // Skip newlines or comments after keyword
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    ASTNode *loop_node = init_ast(NULL, node_type);

    // Parse the condition commands until 'do' or '{' (when condition already
    // started)
    ASTNode *condition = NULL;
    while (true) {
        token = parser_peek(parser);
        if (token->type == TOKEN_DO) {
            break;
        }
        if (condition != NULL && token->type == TOKEN_LBRACE) {
            break;
        }
        if (token->type == TOKEN_EOF) {
            fprintf(stderr,
                    "Syntax error: unexpected EOF in %s loop condition\n",
                    name);
            parser->error = true;
            break;
        }

        ASTNode *cmd = ast_parse_conditional(parser, session);
        if (!cmd) {
            token = parser_peek(parser);
            if (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
                token->type == TOKEN_COMMENT) {
                parser_skip(parser);
            } else {
                break;
            }
            continue;
        }

        if (!condition) {
            condition = cmd;
        } else {
            ASTNode *seq = init_ast(NULL, NODE_SEQUENCE);
            seq->left    = condition;
            seq->right   = cmd;
            condition    = seq;
        }

        token = parser_peek(parser);
        if (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
            token->type == TOKEN_COMMENT) {
            parser_skip(parser);
        }
    }

    if (!condition) {
        fprintf(stderr, "Syntax error: expected condition in %s loop\n", name);
        parser->error = true;
        free_ast(loop_node);
        free(loop_node);
        return NULL;
    }

    // Skip newlines, semicolons, comments before 'do' or '{'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
           token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    bool is_block_style = false;
    if (token->type == TOKEN_DO) {
        is_block_style = false;
    } else if (token->type == TOKEN_LBRACE) {
        is_block_style = true;
    } else {
        fprintf(stderr, "Syntax error: expected 'do' or '{' in %s loop\n",
                name);
        parser->error = true;
        free_ast(condition);
        free(condition);
        free_ast(loop_node);
        free(loop_node);
        return NULL;
    }

    ASTNode *body = NULL;
    if (is_block_style) {
        parser_skip(parser); // consume '{'
        body                 = ast_parse_sequence(parser, session);
        LexerToken close_tok = parser_next(parser);
        if (close_tok.type != TOKEN_RBRACE) {
            fprintf(stderr, "Syntax error: expected '}' to close %s loop\n",
                    name);
            parser->error = true;
        }
        free_lexer_token(&close_tok);
    } else {
        parser_skip(parser); // consume 'do'

        // Skip newlines, semicolons, comments after 'do'
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
               token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        // Parse the body commands until 'done'
        while (true) {
            token = parser_peek(parser);
            if (token->type == TOKEN_DONE) {
                break;
            }
            if (token->type == TOKEN_EOF) {
                fprintf(stderr, "Syntax error: unexpected EOF in %s loop\n",
                        name);
                parser->error = true;
                break;
            }

            ASTNode *cmd = ast_parse_conditional(parser, session);
            if (!cmd) {
                token = parser_peek(parser);
                if (token->type == TOKEN_EOL ||
                    token->type == TOKEN_SEMICOLON ||
                    token->type == TOKEN_COMMENT) {
                    parser_skip(parser);
                } else {
                    break;
                }
                continue;
            }

            if (!body) {
                body = cmd;
            } else {
                ASTNode *seq = init_ast(NULL, NODE_SEQUENCE);
                seq->left    = body;
                seq->right   = cmd;
                body         = seq;
            }

            token = parser_peek(parser);
            if (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
                token->type == TOKEN_COMMENT) {
                parser_skip(parser);
            }
        }

        // Expect and consume 'done'
        token = parser_peek(parser);
        if (token->type == TOKEN_DONE) {
            parser_skip(parser);
        } else {
            fprintf(stderr, "Syntax error: expected 'done' to close %s loop\n",
                    name);
            parser->error = true;
        }
    }

    if (parser->error) {
        free_ast(condition);
        free(condition);
        if (body) {
            free_ast(body);
            free(body);
        }
        free_ast(loop_node);
        free(loop_node);
        return NULL;
    }

    loop_node->left  = condition;
    loop_node->right = body;

    // Parse any redirections after 'done' or '}'
    ast_parse_loop_redirects(parser, loop_node, session);

    return loop_node;
}

ASTNode *ast_parse_while(Parser *parser, Session *session) {
    return ast_parse_while_or_until(parser, session, false);
}

ASTNode *ast_parse_until(Parser *parser, Session *session) {
    return ast_parse_while_or_until(parser, session, true);
}
#endif
