#include <stdio.h>  /* fprintf, NULL */
#include <stdlib.h> /* malloc, free, realloc, atoi, calloc */
#include <string.h> /* strlen, strdup, snprintf */

#include "ast.h" /* ASTNode, NodeType, Redirection, free_ast, free_redirects, parse */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_extend, dynamic_append, dynamic_to_string, free_dynamic */
#include "expand.h"       /* full_expansion */
#include "expansions/aliases.h" /* alias_expansion */
#include "lexer.h" /* LexerInput, LexerToken, lexer_next_token, free_lexer_token, TOKEN_* */
#include "session.h" /* Session */

/* Create a new AST node of given type */
static ASTNode *init_ast(ASTNode *node, NodeType type) {
    if (!node)
        node = malloc(sizeof(ASTNode));
    if (!node)
        return NULL;
    node->type        = type;
    node->argv        = NULL;
    node->arg_is_sub  = NULL;
    node->argc        = 0;
    node->redirects   = NULL;
    node->assignments = NULL;
    node->left        = NULL;
    node->right       = NULL;
    node->background  = false;
#ifndef TIDESH_DISABLE_CONDITIONALS
    node->branches    = NULL;
#endif
    return node;
}

void free_redirects(Redirection *redirect) {
    if (!redirect)
        return;

    if (redirect->target) {
        free(redirect->target);
        redirect->target = NULL;
    }

    if (redirect->next) {
        free_redirects(redirect->next);
        free(redirect->next);
        redirect->next = NULL;
    }
}

#ifndef TIDESH_DISABLE_CONDITIONALS
/* Free conditional branches */
static void free_conditional_branches(ConditionalBranch *branch) {
    if (!branch)
        return;

    if (branch->condition) {
        free_ast(branch->condition);
        free(branch->condition);
        branch->condition = NULL;
    }

    if (branch->body) {
        free_ast(branch->body);
        free(branch->body);
        branch->body = NULL;
    }

    if (branch->next) {
        free_conditional_branches(branch->next);
        branch->next = NULL;
    }

    free(branch);
}
#endif

void free_ast(ASTNode *node) {
    if (!node)
        return;

    if (node->argv) {
        for (int i = 0; i < node->argc; i++) {
            if (node->argv[i]) {
                free(node->argv[i]);
            }
        }
        free(node->argv);
        node->argv = NULL;
    }

    if (node->arg_is_sub) {
        free(node->arg_is_sub);
        node->arg_is_sub = NULL;
    }

    if (node->assignments) {
        free_array(node->assignments);
        free(node->assignments);
        node->assignments = NULL;
    }

    if (node->redirects) {
        free_redirects(node->redirects);
        free(node->redirects);
        node->redirects = NULL;
    }

    if (node->left) {
        free_ast(node->left);
        free(node->left);
        node->left = NULL;
    }

    if (node->right) {
        free_ast(node->right);
        free(node->right);
        node->right = NULL;
    }

#ifndef TIDESH_DISABLE_CONDITIONALS
    if (node->branches) {
        free_conditional_branches(node->branches);
        node->branches = NULL;
    }
#endif

}

/* A command parser */
typedef struct Parser {
    /* The lexer input */
    LexerInput *lexer;
    /* The current token */
    LexerToken current_token;
    /* Is there a current token? */
    bool has_token;
    /* Did an error occur? */
    bool error;
} Parser;

/* Peek at the next token without consuming it */
static LexerToken *parser_peek(Parser *parser) {
    if (!parser->has_token) {
        parser->current_token = lexer_next_token(parser->lexer);
        parser->has_token     = true;
    }
    return &parser->current_token;
}

/* Consume and return the next token */
static LexerToken parser_next(Parser *parser) {
    if (!parser->has_token) {
        return lexer_next_token(parser->lexer);
    }
    parser->has_token = false;
    return parser->current_token;
}

/* Consume the next token without returning it */
static void parser_skip(Parser *parser) {
    LexerToken token = parser_next(parser);
    free_lexer_token(&token);
}

/* Forward declarations of parsing functions */
static ASTNode *parse_sequence(Parser *parser, Session *session);
static ASTNode *parse_and_or(Parser *parser, Session *session);
static ASTNode *parse_pipeline(Parser *parser, Session *session);
static ASTNode *parse_command(Parser *parser, Session *session);
#ifndef TIDESH_DISABLE_CONDITIONALS
static ASTNode *parse_conditional(Parser *parser, Session *session);
#endif

ASTNode *parse(LexerInput *lexer, Session *session) {
    Parser   parser = {.lexer = lexer, .has_token = false, .error = false};
    ASTNode *tree   = parse_sequence(&parser, session);
    if (parser.has_token)
        free_lexer_token(&parser.current_token);
    return tree;
}

/* Parse a sequence of commands separated by ;, &, \n */
static ASTNode *parse_sequence(Parser *parser, Session *session) {
    // Skip leading newlines, semicolons, and comments
    while (parser_peek(parser)->type == TOKEN_EOL ||
#ifndef TIDESH_DISABLE_SEQUENCES
           parser_peek(parser)->type == TOKEN_SEMICOLON ||
#endif
           parser_peek(parser)->type == TOKEN_COMMENT) {
        parser_skip(parser);
    }

    if (parser_peek(parser)->type == TOKEN_RPAREN ||
        parser_peek(parser)->type == TOKEN_EOF) {
        return NULL;
    }

#ifndef TIDESH_DISABLE_CONDITIONALS
    ASTNode *left = parse_conditional(parser, session);
#else
    ASTNode *left = parse_and_or(parser, session);
#endif
    if (!left)
        return NULL;

    while (true) {
        LexerToken *token = parser_peek(parser);

        if (token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            continue;
        }

#ifndef TIDESH_DISABLE_JOB_CONTROL
        if (token->type == TOKEN_BACKGROUND) {
            parser_skip(parser);
            left->background = true;
            token            = parser_peek(parser);
            if (token->type == TOKEN_EOF || token->type == TOKEN_EOL ||
#ifndef TIDESH_DISABLE_SEQUENCES
                token->type == TOKEN_SEMICOLON ||
#endif
                token->type == TOKEN_RPAREN || token->type == TOKEN_COMMENT) {
                continue;
            }
        } else
#endif
#ifndef TIDESH_DISABLE_SEQUENCES
            if (token->type == TOKEN_SEMICOLON || token->type == TOKEN_EOL ||
                token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
            if (token->type == TOKEN_EOF || token->type == TOKEN_EOL ||
                token->type == TOKEN_SEMICOLON || token->type == TOKEN_RPAREN ||
                token->type == TOKEN_COMMENT) {
                continue;
            }
        } else
#endif
        {
            break;
        }

#ifndef TIDESH_DISABLE_CONDITIONALS
        ASTNode *right = parse_conditional(parser, session);
#else
        ASTNode *right = parse_and_or(parser, session);
#endif
        if (!right)
            break;
#ifndef TIDESH_DISABLE_SEQUENCES
        ASTNode *node = init_ast(NULL, NODE_SEQUENCE);
        node->left    = left;
        node->right   = right;
        left          = node;
#else
        left = right;
#endif
    }

    return left;
}

#ifndef TIDESH_DISABLE_CONDITIONALS
/* Parse conditional statements (if/then/else/elif/fi) */
static ASTNode *parse_conditional(Parser *parser, Session *session) {
    LexerToken *token = parser_peek(parser);

    // Not a conditional, delegate to and_or
    if (token->type != TOKEN_IF) {
        return parse_and_or(parser, session);
    }

    // Consume 'if'
    parser_skip(parser);

    // Create conditional node
    ASTNode *conditional = init_ast(NULL, NODE_CONDITIONAL);

    // Parse branches
    ConditionalBranch *first_branch   = NULL;
    ConditionalBranch *current_branch = NULL;

    while (true) {
        // Parse the condition (a command)
        ASTNode *condition = parse_and_or(parser, session);
        if (!condition) {
            fprintf(stderr,
                    "Syntax error: expected condition in if statement\n");
            parser->error = true;
            free_ast(conditional);
            free(conditional);
            return NULL;
        }

        // Expect 'then'
        token = parser_peek(parser);

        // Skip newlines before 'then'
        while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
               token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        if (token->type != TOKEN_THEN) {
            fprintf(stderr, "Syntax error: expected 'then' after condition\n");
            parser->error = true;
            free_ast(condition);
            free(condition);
            free_ast(conditional);
            free(conditional);
            return NULL;
        }

        parser_skip(parser);

        // Skip newlines after 'then'
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
               token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        // Parse the body (commands until elif/else/fi)
        ASTNode *body = NULL;
        while (true) {
            token = parser_peek(parser);
            if (token->type == TOKEN_ELIF || token->type == TOKEN_ELSE ||
                token->type == TOKEN_FI) {
                break;
            }
            if (token->type == TOKEN_EOF) {
                fprintf(stderr,
                        "Syntax error: unexpected EOF in if statement\n");
                parser->error = true;
                break;
            }

            ASTNode *cmd = parse_conditional(parser, session);
            if (!cmd) {
                // Skip trailing separators
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

            // Append command to body
            if (!body) {
                body = cmd;
            } else {
                ASTNode *seq = init_ast(NULL, NODE_SEQUENCE);
                seq->left    = body;
                seq->right   = cmd;
                body         = seq;
            }

            // Skip separators after command
            token = parser_peek(parser);
            if (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
                token->type == TOKEN_COMMENT) {
                parser_skip(parser);
            }
        }

        // Create the branch
        ConditionalBranch *branch = calloc(1, sizeof(ConditionalBranch));
        branch->condition         = condition;
        branch->body              = body;
        branch->next              = NULL;

        if (!first_branch) {
            first_branch   = branch;
            current_branch = branch;
        } else {
            current_branch->next = branch;
            current_branch       = branch;
        }

        // Check for elif, else, or fi
        token = parser_peek(parser);
        if (token->type == TOKEN_ELIF) {
            parser_skip(parser);
            // Continue loop to parse next condition
            continue;
        } else if (token->type == TOKEN_ELSE) {
            parser_skip(parser);

            // Skip newlines after 'else'
            token = parser_peek(parser);
            while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
                   token->type == TOKEN_COMMENT) {
                parser_skip(parser);
                token = parser_peek(parser);
            }

            // Create a special else branch with no condition
            ConditionalBranch *else_branch =
                calloc(1, sizeof(ConditionalBranch));
            else_branch->condition = NULL; // No condition for else

            // Parse else body
            ASTNode *else_body = NULL;
            while (true) {
                token = parser_peek(parser);
                if (token->type == TOKEN_FI) {
                    break;
                }
                if (token->type == TOKEN_EOF) {
                    fprintf(stderr,
                            "Syntax error: unexpected EOF in else block\n");
                    parser->error = true;
                    break;
                }

                ASTNode *cmd = parse_conditional(parser, session);
                if (!cmd) {
                    // Skip trailing separators
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

                // Append command to body
                if (!else_body) {
                    else_body = cmd;
                } else {
                    ASTNode *seq = init_ast(NULL, NODE_SEQUENCE);
                    seq->left    = else_body;
                    seq->right   = cmd;
                    else_body    = seq;
                }

                // Skip separators after command
                token = parser_peek(parser);
                if (token->type == TOKEN_EOL ||
                    token->type == TOKEN_SEMICOLON ||
                    token->type == TOKEN_COMMENT) {
                    parser_skip(parser);
                }
            }

            else_branch->body    = else_body;
            else_branch->next    = NULL;
            current_branch->next = else_branch;
            current_branch       = else_branch;
            break;
        } else if (token->type == TOKEN_FI) {
            break;
        } else {
            fprintf(stderr, "Syntax error: expected 'elif', 'else', or 'fi' in "
                            "if statement\n");
            parser->error = true;
            break;
        }
    }

    // Consume 'fi'
    token = parser_peek(parser);
    if (token->type == TOKEN_FI) {
        parser_skip(parser);
    } else {
        fprintf(stderr, "Syntax error: expected 'fi' to close if statement\n");
        parser->error = true;
    }

    // Attach branches to conditional node
    conditional->branches = first_branch;
    return conditional;
}
#else
/* Stub for parsing conditional when disabled */
static ASTNode *parse_conditional(Parser *parser, Session *session) {
    return parse_and_or(parser, session);
}
#endif

/* Parse commands connected by && and || */
static ASTNode *parse_and_or(Parser *parser, Session *session) {
    ASTNode *left = parse_pipeline(parser, session);
    if (!left)
        return NULL;

#ifndef TIDESH_DISABLE_SEQUENCES
    while (true) {
        LexerToken *token = parser_peek(parser);
        NodeType    type;

        if (token->type == TOKEN_SEQUENCE) {
            type = NODE_AND;
        } else if (token->type == TOKEN_OR) {
            type = NODE_OR;
        } else {
            break;
        }

        parser_skip(parser);
        ASTNode *right = parse_pipeline(parser, session);
        if (!right)
            break;
        ASTNode *node = init_ast(NULL, type);
        node->left    = left;
        node->right   = right;
        left          = node;
    }
#endif

    return left;
}

/* Parse a pipeline of commands (commands separated by | ) */
static ASTNode *parse_pipeline(Parser *parser, Session *session) {
    ASTNode *left = parse_command(parser, session);
    if (!left)
        return NULL;
#ifndef TIDESH_DISABLE_PIPES
    LexerToken *token = parser_peek(parser);
    if (token->type == TOKEN_PIPE) {
        parser_skip(parser);
        ASTNode *node = init_ast(NULL, NODE_PIPE);
        node->left    = left;
        node->right   = parse_pipeline(parser, session);
        return node;
    }
#endif
    return left;
}

/* Add an argument to a command node */
static void add_argument(ASTNode *node, char *arg, int sub_type) {
    if (!arg)
        return;
    node->argc++;
    node->argv       = realloc(node->argv, (node->argc + 1) * sizeof(char *));
    node->arg_is_sub = realloc(node->arg_is_sub, node->argc * sizeof(int));
    node->argv[node->argc - 1]       = strdup(arg);
    node->arg_is_sub[node->argc - 1] = sub_type;
    node->argv[node->argc]           = NULL;
}

/* Parse a single command (with possible redirections and assignments) */
static ASTNode *parse_command(Parser *parser, Session *session) {
    LexerToken *peek = parser_peek(parser);
#ifndef TIDESH_DISABLE_SUBSHELLS
    if (peek->type == TOKEN_LPAREN) {
        parser_skip(parser);
        ASTNode   *subshell_body = parse_sequence(parser, session);
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

    ASTNode *cmd        = init_ast(NULL, NODE_COMMAND);
    bool     first_word = true;

    while (true) {
        LexerToken *token = parser_peek(parser);

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
             token->type <= TOKEN_REDIRECT_OUT_ERR)) {
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
            }
            redirect->next = cmd->redirects;
            cmd->redirects = redirect;
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
#endif /* TIDESH_DISABLE_ASSIGNMENTS */
        }

#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
        if (token->type == TOKEN_WORD ||
            token->type == TOKEN_PROCESS_SUBSTITUTION_IN ||
            token->type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
            LexerToken word  = parser_next(parser);
            Array     *parts = NULL;

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
#endif /* TIDESH_DISABLE_COMMAND_SUBSTITUTION */
        }
        break;
    }

    if (cmd->argc == 0 && !cmd->assignments && !cmd->redirects) {
        free_ast(cmd);
        free(cmd);
        return NULL;
    }
    return cmd;
}
