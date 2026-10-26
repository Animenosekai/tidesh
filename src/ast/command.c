#include <ctype.h>  /* isalpha, isalnum */
#include <stdio.h>  /* fprintf, snprintf */
#include <stdlib.h> /* malloc, free, realloc, calloc, atoi */
#include <string.h> /* strcmp, strdup, strlen, strchr, strndup, strtok_r */

#include "ast/command.h"
#include "ast/conditional.h"
#include "ast/loop.h"
#include "ast/node.h"
#include "ast/parser.h"
#include "ast/pipeline.h"
#include "ast/try.h"
#include "ast/with.h"
#include "data/dynamic.h"
#include "expand.h"
#include "expansions/aliases.h"
#include "lexer.h"
#include "session.h"

/* Add an argument to a command node */
void add_argument(ASTNode *node, char *arg, int sub_type) {
    if (!arg)
        return;
    node->argc++;
    node->argv       = realloc(node->argv, (node->argc + 1) * sizeof(char *));
    node->arg_is_sub = realloc(node->arg_is_sub, node->argc * sizeof(int));
    if (node->arg_keys) {
        node->arg_keys =
            realloc(node->arg_keys, (node->argc + 1) * sizeof(char *));
        node->arg_keys[node->argc - 1] = NULL;
        node->arg_keys[node->argc]     = NULL;
    }
    node->argv[node->argc - 1]       = strdup(arg);
    node->arg_is_sub[node->argc - 1] = sub_type;
    node->argv[node->argc]           = NULL;
}

/* Add a named argument (or positional if key is NULL) to a command node */
void add_named_argument(ASTNode *node, const char *key, const char *arg,
                        int sub_type) {
    if (!arg)
        return;
    node->argc++;
    node->argv       = realloc(node->argv, (node->argc + 1) * sizeof(char *));
    node->arg_is_sub = realloc(node->arg_is_sub, node->argc * sizeof(int));
    if (!node->arg_keys) {
        node->arg_keys = calloc(node->argc + 1, sizeof(char *));
    } else {
        node->arg_keys =
            realloc(node->arg_keys, (node->argc + 1) * sizeof(char *));
    }
    node->argv[node->argc - 1]       = strdup(arg);
    node->arg_is_sub[node->argc - 1] = sub_type;
    node->arg_keys[node->argc - 1]   = key ? strdup(key) : NULL;
    node->argv[node->argc]           = NULL;
    node->arg_keys[node->argc]       = NULL;
}

bool is_valid_param_name(const char *name) {
    if (!name || !*name)
        return false;
    if (!isalpha((unsigned char)name[0]) && name[0] != '_')
        return false;
    for (size_t i = 1; name[i]; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_')
            return false;
    }
    return true;
}

void add_func_arg_item(FuncArgItem **items, size_t *count, size_t *cap,
                       const char *key, const char *val) {
    if (*count >= *cap) {
        *cap   = (*cap == 0) ? 4 : (*cap * 2);
        *items = realloc(*items, *cap * sizeof(FuncArgItem));
    }
    (*items)[*count].key = key ? strdup(key) : NULL;
    (*items)[*count].val = val ? strdup(val) : strdup("");
    (*count)++;
}

void free_func_arg_items(FuncArgItem *items, size_t count) {
    if (!items)
        return;
    for (size_t i = 0; i < count; i++) {
        if (items[i].key)
            free(items[i].key);
        if (items[i].val)
            free(items[i].val);
    }
    free(items);
}

/* Parse a single command (with possible redirections and assignments) */
ASTNode *ast_parse_command(Parser *parser, Session *session) {
    LexerToken *peek = parser_peek(parser);
#ifndef TIDESH_DISABLE_SUBSHELLS
    if (peek->type == TOKEN_LPAREN) {
        parser_skip(parser);
        ASTNode   *subshell_body = ast_parse_sequence(parser, session);
        LexerToken token         = parser_next(parser);
        if (token.type != TOKEN_RPAREN) {
            fprintf(stderr, "Syntax error: expected ')'\n");
            parser->error = true;
        }
        free_lexer_token(&token);
        if (!subshell_body)
            return NULL;
        ASTNode *subshell = init_ast(NULL, NODE_SUBSHELL);
        subshell->left    = subshell_body;
        return subshell;
    }
#endif

#ifndef TIDESH_DISABLE_BLOCKS
    if (peek->type == TOKEN_LBRACE) {
        parser_skip(parser);
        ASTNode   *block_body = ast_parse_sequence(parser, session);
        LexerToken token      = parser_next(parser);
        if (token.type != TOKEN_RBRACE) {
            fprintf(stderr, "Syntax error: expected '}'\n");
            parser->error = true;
        }
        free_lexer_token(&token);
        if (!block_body)
            return NULL;
        ASTNode *block = init_ast(NULL, NODE_BLOCK);
        block->left    = block_body;
        return block;
    }
#endif

#ifndef TIDESH_DISABLE_CONDITIONALS
    if (peek->type == TOKEN_IF) {
        return ast_parse_conditional(parser, session);
    }
    if (peek->type == TOKEN_CASE) {
        return ast_parse_case(parser, session);
    }
#endif

#ifndef TIDESH_DISABLE_LOOPS
    if (peek->type == TOKEN_FOR) {
        return ast_parse_for(parser, session);
    }
    if (peek->type == TOKEN_WHILE) {
        return ast_parse_while(parser, session);
    }
    if (peek->type == TOKEN_UNTIL) {
        return ast_parse_until(parser, session);
    }
#endif

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    if (peek->type == TOKEN_WITH) {
        return ast_parse_with(parser, session);
    }
#endif

    if (peek->type == TOKEN_TRY) {
        size_t      lookahead = 1;
        LexerToken *next      = parser_peek_nth(parser, lookahead);
        while (next &&
               (next->type == TOKEN_EOL || next->type == TOKEN_COMMENT)) {
            lookahead++;
            next = parser_peek_nth(parser, lookahead);
        }
        if (next && (next->type == TOKEN_LBRACE || next->type == TOKEN_DO)) {
            return ast_parse_try(parser, session);
        }
        peek->type = TOKEN_WORD;
    }

    if (peek->type == TOKEN_CATCH || peek->type == TOKEN_FINALLY) {
        peek->type = TOKEN_WORD;
    }

    ASTNode *cmd        = init_ast(NULL, NODE_COMMAND);
    bool     first_word = true;

    while (true) {
        LexerToken *token = parser_peek(parser);

#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS)
        if (token->type == TOKEN_IN) {
            token->type = TOKEN_WORD;
        }
#endif
#ifndef TIDESH_DISABLE_LOOPS
        if (!first_word &&
            (token->type == TOKEN_FOR || token->type == TOKEN_WHILE ||
             token->type == TOKEN_UNTIL || token->type == TOKEN_DO ||
             token->type == TOKEN_DONE)) {
            token->type = TOKEN_WORD;
        }
#endif
#ifndef TIDESH_DISABLE_CONDITIONALS
        if (!first_word &&
            (token->type == TOKEN_CASE || token->type == TOKEN_ESAC)) {
            token->type = TOKEN_WORD;
        }
#endif
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
        if (!first_word && token->type == TOKEN_WITH) {
            token->type = TOKEN_WORD;
        }
#endif
        if (!first_word &&
            (token->type == TOKEN_TRY || token->type == TOKEN_CATCH ||
             token->type == TOKEN_FINALLY)) {
            token->type = TOKEN_WORD;
        }

#ifndef TIDESH_DISABLE_REDIRECTIONS
        int fd = -1;
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
                if (token->type == TOKEN_HERESTRING) {
                    Array *expansion = full_expansion(token->value, session);
                    if (expansion) {
                        Dynamic joined = {0};
                        init_dynamic(&joined);
                        for (size_t i = 0; i < expansion->count; i++) {
                            dynamic_extend(&joined, expansion->items[i]);
                            if (i < expansion->count - 1)
                                dynamic_append(&joined, ' ');
                        }
                        redirect->target = dynamic_to_string(&joined);
                        free_dynamic(&joined);
                        free_array(expansion);
                        free(expansion);
                    } else {
                        redirect->target = strdup(token->value);
                    }
                } else {
                    redirect->target = strdup(token->value);
                }

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

            if (!cmd->redirects) {
                cmd->redirects = redirect;
            } else {
                Redirection *curr = cmd->redirects;
                while (curr->next) {
                    curr = curr->next;
                }
                curr->next = redirect;
            }
            continue;
        }
#endif /* TIDESH_DISABLE_REDIRECTIONS */

#ifndef TIDESH_DISABLE_ASSIGNMENTS
        if (token->type == TOKEN_ASSIGNMENT) {
            LexerToken assign = parser_next(parser);
            size_t     len    = strlen(assign.value) + strlen(assign.extra) + 2;
            char      *full   = malloc(len);
            snprintf(full, len, "%s=%s", assign.value, assign.extra);

            if (first_word) {
                if (!cmd->assignments)
                    cmd->assignments = init_array(NULL);
                array_add(cmd->assignments, full);
            } else {
                add_argument(cmd, full, 0);
            }

            free(full);
            free_lexer_token(&assign);
            continue;
        }
#endif /* TIDESH_DISABLE_ASSIGNMENTS */

#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
        if (token->type == TOKEN_WORD ||
            token->type == TOKEN_PROCESS_SUBSTITUTION_IN ||
            token->type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
            LexerToken word  = parser_next(parser);
            Array     *parts = NULL;

#ifndef TIDESH_DISABLE_FUNCTIONS
            // Check for function definition name(...) { ... } or function call
            // name(...)
            if ((!session || session->features.functions) && first_word &&
                word.type == TOKEN_WORD &&
                parser_peek(parser)->type == TOKEN_LPAREN) {
                parser_skip(parser); // consume '('

                FuncArgItem *items      = NULL;
                size_t       item_count = 0;
                size_t       item_cap   = 0;

                while (true) {
                    LexerToken *t = parser_peek(parser);
                    while (t->type == TOKEN_EOL || t->type == TOKEN_COMMENT) {
                        parser_skip(parser);
                        t = parser_peek(parser);
                    }
                    if (t->type == TOKEN_EOF || parser->error) {
                        fprintf(
                            stderr,
                            "Syntax error: unexpected EOF in argument list\n");
                        parser->error = true;
                        break;
                    }
                    if (t->type == TOKEN_RPAREN) {
                        parser_skip(parser); // consume ')'
                        break;
                    }
                    if (t->type == TOKEN_WORD && strcmp(t->value, ",") == 0) {
                        parser_skip(parser);
                        continue;
                    }

                    if (t->type == TOKEN_ASSIGNMENT) {
                        LexerToken assign_tok = parser_next(parser);
                        char  *val = assign_tok.extra ? assign_tok.extra : "";
                        char  *val_copy = strdup(val);
                        size_t vlen     = strlen(val_copy);
                        if (vlen > 0 && val_copy[vlen - 1] == ',') {
                            val_copy[vlen - 1] = '\0';
                        }
                        add_func_arg_item(&items, &item_count, &item_cap,
                                          assign_tok.value, val_copy);
                        free(val_copy);
                        free_lexer_token(&assign_tok);
                        continue;
                    }

                    if (t->type == TOKEN_WORD) {
                        LexerToken w_tok = parser_next(parser);
                        char      *w =
                            w_tok.value ? strdup(w_tok.value) : strdup("");
                        free_lexer_token(&w_tok);

                        // Check if followed by '=': KEY = VAL
                        LexerToken *next_tok = parser_peek(parser);
                        if (next_tok->type == TOKEN_WORD &&
                            strcmp(next_tok->value, "=") == 0) {
                            parser_skip(parser); // consume '='
                            t = parser_peek(parser);
                            while (t->type == TOKEN_EOL ||
                                   t->type == TOKEN_COMMENT) {
                                parser_skip(parser);
                                t = parser_peek(parser);
                            }
                            char *val_allocated = NULL;
                            if (t->type == TOKEN_WORD ||
                                t->type == TOKEN_ASSIGNMENT) {
                                LexerToken v_tok = parser_next(parser);
                                val_allocated    = v_tok.value
                                                       ? strdup(v_tok.value)
                                                       : strdup("");
                                free_lexer_token(&v_tok);
                                size_t vlen = strlen(val_allocated);
                                if (vlen > 0 &&
                                    val_allocated[vlen - 1] == ',') {
                                    val_allocated[vlen - 1] = '\0';
                                }
                            }
                            add_func_arg_item(&items, &item_count, &item_cap, w,
                                              val_allocated ? val_allocated
                                                            : "");
                            free(val_allocated);
                            free(w);
                            continue;
                        }

                        // Check if w contains '=': KEY=VAL
                        char *eq = strchr(w, '=');
                        if (eq && eq != w) {
                            size_t klen   = (size_t)(eq - w);
                            char  *k      = strndup(w, klen);
                            char  *v      = eq + 1;
                            char  *v_copy = strdup(v);
                            size_t vlen   = strlen(v_copy);
                            if (vlen > 0 && v_copy[vlen - 1] == ',') {
                                v_copy[vlen - 1] = '\0';
                            }
                            add_func_arg_item(&items, &item_count, &item_cap, k,
                                              v_copy);
                            free(k);
                            free(v_copy);
                            free(w);
                            continue;
                        }

                        // Positional argument or parameter name
                        size_t wlen = strlen(w);
                        if (wlen > 0 && w[wlen - 1] == ',') {
                            w[wlen - 1] = '\0';
                        }
                        if (strchr(w, ',')) {
                            char *saveptr = NULL;
                            char *tok     = strtok_r(w, ",", &saveptr);
                            while (tok) {
                                if (*tok) {
                                    add_func_arg_item(&items, &item_count,
                                                      &item_cap, NULL, tok);
                                }
                                tok = strtok_r(NULL, ",", &saveptr);
                            }
                        } else {
                            add_func_arg_item(&items, &item_count, &item_cap,
                                              NULL, w);
                        }
                        free(w);
                        continue;
                    }

                    LexerToken other_tok = parser_next(parser);
                    if (other_tok.value) {
                        char  *ov    = strdup(other_tok.value);
                        size_t ovlen = strlen(ov);
                        if (ovlen > 0 && ov[ovlen - 1] == ',') {
                            ov[ovlen - 1] = '\0';
                        }
                        add_func_arg_item(&items, &item_count, &item_cap, NULL,
                                          ov);
                        free(ov);
                    }
                    free_lexer_token(&other_tok);
                }

                bool can_be_def = true;
                for (size_t i = 0; i < item_count; i++) {
                    if (items[i].key != NULL ||
                        !is_valid_param_name(items[i].val)) {
                        can_be_def = false;
                        break;
                    }
                }

                LexerToken *next1       = parser_peek(parser);
                bool        is_func_def = false;
                if (can_be_def) {
                    if (next1->type == TOKEN_LBRACE ||
                        next1->type == TOKEN_LPAREN) {
                        is_func_def = true;
                    } else if (next1->type == TOKEN_EOL) {
                        LexerToken *next2 = parser_peek2(parser);
                        if (next2->type == TOKEN_LBRACE ||
                            next2->type == TOKEN_LPAREN) {
                            parser_skip(parser); // consume newline
                            is_func_def = true;
                        }
                    }
                }

                if (is_func_def) {
                    ASTNode *body = ast_parse_command(parser, session);
                    if (body) {
                        cmd->type = NODE_FUNCTION_DEF;
                        add_argument(cmd, word.value, 0);
                        for (size_t i = 0; i < item_count; i++) {
                            add_argument(cmd, items[i].val, 0);
                        }
                        cmd->left = body;
                        free_func_arg_items(items, item_count);
                        free_lexer_token(&word);
                        return cmd;
                    }
                    free_func_arg_items(items, item_count);
                    free_lexer_token(&word);
                    free_ast(cmd);
                    free(cmd);
                    return NULL;
                }

                // Otherwise, it is a function / command call
                cmd->type = NODE_COMMAND;
                add_argument(cmd, word.value, 0); // argv[0] = command name
                for (size_t i = 0; i < item_count; i++) {
                    if (items[i].key) {
                        add_named_argument(cmd, items[i].key, items[i].val, 0);
                    } else {
                        add_named_argument(cmd, NULL, items[i].val, 0);
                    }
                }
                free_func_arg_items(items, item_count);
                free_lexer_token(&word);
                first_word = false;
                continue;
            }
#endif

            if (word.type == TOKEN_PROCESS_SUBSTITUTION_IN) {
                add_argument(cmd, word.value, 1);
                free_lexer_token(&word);
                first_word = false;
                continue;
            } else if (word.type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
                add_argument(cmd, word.value, 2);
                free_lexer_token(&word);
                first_word = false;
                continue;
            } else if (first_word) {
#ifndef TIDESH_DISABLE_ALIASES
                parts = alias_expansion(word.value, session);
#else
                parts = init_array(NULL);
                array_add(parts, word.value);
#endif
            } else {
                parts = init_array(NULL);
                array_add(parts, word.value);
            }

            for (size_t i = 0; i < parts->count; i++) {
                add_argument(cmd, parts->items[i], 0);
            }

            free_array(parts);
            free(parts);
            free_lexer_token(&word);
            first_word = false;
            continue;
        }
#endif /* TIDESH_DISABLE_COMMAND_SUBSTITUTION */
        break;
    }

    if (cmd->argc == 0 && !cmd->assignments && !cmd->redirects) {
        free_ast(cmd);
        free(cmd);
        return NULL;
    }
    return cmd;
}
