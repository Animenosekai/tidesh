#include <ctype.h>  /* isdigit */
#include <stdio.h>  /* fprintf, NULL */
#include <stdlib.h> /* malloc, free, realloc, atoi, calloc */
#include <string.h> /* strlen, strdup, snprintf */

#include "ast.h" /* ASTNode, NodeType, Redirection, free_ast, free_redirects, parse */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_extend, dynamic_append, dynamic_to_string, free_dynamic */
#include "expand.h"       /* full_expansion */
#include "expansions/aliases.h" /* alias_expansion */
#include "lexer.h" /* LexerInput, LexerToken, lexer_next_token, free_lexer_token, TOKEN_* */
#include "session.h" /* Session */

static bool is_all_digits(const char *str) {
    if (!str || !*str)
        return false;
    for (const char *p = str; *p; p++) {
        if (!isdigit((unsigned char)*p))
            return false;
    }
    return true;
}

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
    node->branches = NULL;
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

/* Free case items */
static void free_case_items(CaseItem *item) {
    while (item) {
        CaseItem *next = item->next;
        if (item->patterns) {
            for (int i = 0; i < item->pattern_count; i++) {
                if (item->patterns[i]) {
                    free(item->patterns[i]);
                }
            }
            free(item->patterns);
            item->patterns = NULL;
        }
        if (item->body) {
            free_ast(item->body);
            free(item->body);
            item->body = NULL;
        }
        free(item);
        item = next;
    }
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
    if (node->case_items) {
        free_case_items(node->case_items);
        node->case_items = NULL;
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
static void     add_argument(ASTNode *node, char *arg, int sub_type);
static ASTNode *parse_sequence(Parser *parser, Session *session);
static ASTNode *parse_and_or(Parser *parser, Session *session);
static ASTNode *parse_pipeline(Parser *parser, Session *session);
static ASTNode *parse_command(Parser *parser, Session *session);
#ifndef TIDESH_DISABLE_CONDITIONALS
static ASTNode *parse_conditional(Parser *parser, Session *session);
static ASTNode *parse_case(Parser *parser, Session *session);
#endif
#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS)
static void parse_loop_redirects(Parser *parser, ASTNode *node,
                                 Session *session);
#endif
#ifndef TIDESH_DISABLE_LOOPS
static ASTNode *parse_for(Parser *parser, Session *session);
static ASTNode *parse_while(Parser *parser, Session *session);
static ASTNode *parse_until(Parser *parser, Session *session);
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
#ifndef TIDESH_DISABLE_BLOCKS
        parser_peek(parser)->type == TOKEN_RBRACE ||
#endif
#ifndef TIDESH_DISABLE_LOOPS
        parser_peek(parser)->type == TOKEN_DONE ||
#endif
#ifndef TIDESH_DISABLE_CONDITIONALS
        parser_peek(parser)->type == TOKEN_ESAC ||
        parser_peek(parser)->type == TOKEN_DOUBLE_SEMICOLON ||
        parser_peek(parser)->type == TOKEN_CASE_FALLTHROUGH ||
        parser_peek(parser)->type == TOKEN_CASE_CONTINUE ||
#endif
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
                token->type == TOKEN_RPAREN ||
#ifndef TIDESH_DISABLE_BLOCKS
                token->type == TOKEN_RBRACE ||
#endif
#ifndef TIDESH_DISABLE_LOOPS
                token->type == TOKEN_DONE ||
#endif
                token->type == TOKEN_COMMENT) {
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
#ifndef TIDESH_DISABLE_BLOCKS
                token->type == TOKEN_RBRACE ||
#endif
#ifndef TIDESH_DISABLE_LOOPS
                token->type == TOKEN_DONE ||
#endif
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
/* Parse conditional statements (if/then/else/elif/fi and case/in/esac) */
static ASTNode *parse_conditional(Parser *parser, Session *session) {
    LexerToken *token = parser_peek(parser);

    if (token->type == TOKEN_CASE) {
        return parse_case(parser, session);
    }

    // Not an if statement, delegate to and_or
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

/* Parse a case statement (case WORD in ... esac) */
static ASTNode *parse_case(Parser *parser, Session *session) {
    LexerToken *token = parser_peek(parser);
    if (token->type != TOKEN_CASE) {
        return NULL;
    }

    // Consume 'case'
    parser_skip(parser);

    // Skip newlines or comments before the case word
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // We expect the word to match
    if (token->type == TOKEN_EOF || token->type == TOKEN_SEMICOLON) {
        fprintf(stderr, "Syntax error: expected word after 'case'\n");
        parser->error = true;
        return NULL;
    }

    LexerToken word_tok = parser_next(parser);
    if (!word_tok.value || word_tok.type == TOKEN_EOF ||
        word_tok.type == TOKEN_EOL || word_tok.type == TOKEN_SEMICOLON) {
        fprintf(stderr, "Syntax error: expected word after 'case'\n");
        parser->error = true;
        free_lexer_token(&word_tok);
        return NULL;
    }

    ASTNode *case_node = init_ast(NULL, NODE_CASE);
    int      sub_type  = 0;
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
    if (word_tok.type == TOKEN_PROCESS_SUBSTITUTION_IN) {
        sub_type = 1;
    } else if (word_tok.type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
        sub_type = 2;
    }
#endif
    add_argument(case_node, word_tok.value, sub_type);
    free_lexer_token(&word_tok);

    // Skip newlines or comments before 'in'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // Expect 'in'
    if (token->type != TOKEN_IN &&
        !(token->type == TOKEN_WORD && token->value &&
          strcmp(token->value, "in") == 0)) {
        fprintf(stderr, "Syntax error: expected 'in' in case statement\n");
        parser->error = true;
        free_ast(case_node);
        free(case_node);
        return NULL;
    }
    parser_skip(parser); // consume 'in'

    // Skip newlines, semicolons, comments after 'in'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
           token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    CaseItem *first_item   = NULL;
    CaseItem *current_item = NULL;

    // Parse case clauses until 'esac'
    while (true) {
        token = parser_peek(parser);

        // Check if finished with 'esac'
        if (token->type == TOKEN_ESAC) {
            parser_skip(parser); // consume 'esac'
            break;
        }

        if (token->type == TOKEN_EOF) {
            fprintf(stderr, "Syntax error: unexpected EOF in case statement\n");
            parser->error = true;
            break;
        }

        // Optional leading '('
        if (token->type == TOKEN_LPAREN) {
            parser_skip(parser); // consume '('
            token = parser_peek(parser);
            while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
                parser_skip(parser);
                token = parser_peek(parser);
            }
        }

        // Parse patterns separated by '|' until ')'
        Array *patterns          = init_array(NULL);
        bool   expecting_pattern = true;

        while (true) {
            token = parser_peek(parser);
            while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
                parser_skip(parser);
                token = parser_peek(parser);
            }

            if (expecting_pattern) {
                if (token->type == TOKEN_EOF || token->type == TOKEN_RPAREN ||
                    token->type == TOKEN_PIPE) {
                    fprintf(stderr,
                            "Syntax error: expected pattern in case clause\n");
                    parser->error = true;
                    break;
                }

                LexerToken pat_tok = parser_next(parser);
                array_add(patterns, pat_tok.value ? pat_tok.value : "");
                free_lexer_token(&pat_tok);
                expecting_pattern = false;
            } else {
                if (token->type == TOKEN_PIPE) {
                    parser_skip(parser); // consume '|'
                    expecting_pattern = true;
                } else if (token->type == TOKEN_RPAREN) {
                    parser_skip(parser); // consume ')'
                    break;
                } else {
                    fprintf(
                        stderr,
                        "Syntax error: expected '|' or ')' after pattern\n");
                    parser->error = true;
                    break;
                }
            }
        }

        if (parser->error) {
            free_array(patterns);
            free(patterns);
            free_ast(case_node);
            free(case_node);
            return NULL;
        }

        // Skip newlines or comments after ')'
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }

        // Parse body commands until ';;', ';&', ';;&', 'esac', or EOF
        ASTNode *body = NULL;
        while (true) {
            token = parser_peek(parser);
            if (token->type == TOKEN_DOUBLE_SEMICOLON ||
                token->type == TOKEN_CASE_FALLTHROUGH ||
                token->type == TOKEN_CASE_CONTINUE ||
                token->type == TOKEN_ESAC) {
                break;
            }
            if (token->type == TOKEN_EOF) {
                fprintf(stderr, "Syntax error: unexpected EOF in case body\n");
                parser->error = true;
                break;
            }

            ASTNode *cmd = parse_conditional(parser, session);
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

        if (parser->error) {
            if (body) {
                free_ast(body);
                free(body);
            }
            free_array(patterns);
            free(patterns);
            free_ast(case_node);
            free(case_node);
            return NULL;
        }

        // Determine clause terminator
        CaseTerminator term = CASE_TERM_BREAK;
        token               = parser_peek(parser);
        if (token->type == TOKEN_DOUBLE_SEMICOLON) {
            term = CASE_TERM_BREAK;
            parser_skip(parser); // consume ';;'
        } else if (token->type == TOKEN_CASE_FALLTHROUGH) {
            term = CASE_TERM_FALLTHROUGH;
            parser_skip(parser); // consume ';&'
        } else if (token->type == TOKEN_CASE_CONTINUE) {
            term = CASE_TERM_CONTINUE;
            parser_skip(parser); // consume ';;&'
        } else if (token->type == TOKEN_ESAC) {
            term = CASE_TERM_BREAK;
        } else {
            fprintf(stderr, "Syntax error: expected ';;' or 'esac'\n");
            parser->error = true;
            if (body) {
                free_ast(body);
                free(body);
            }
            free_array(patterns);
            free(patterns);
            free_ast(case_node);
            free(case_node);
            return NULL;
        }

        // Create the CaseItem
        CaseItem *item      = calloc(1, sizeof(CaseItem));
        item->pattern_count = patterns->count;
        item->patterns      = malloc(patterns->count * sizeof(char *));
        for (size_t i = 0; i < patterns->count; i++) {
            item->patterns[i] = strdup(patterns->items[i]);
        }
        free_array(patterns);
        free(patterns);
        item->body       = body;
        item->terminator = term;
        item->next       = NULL;

        if (!first_item) {
            first_item   = item;
            current_item = item;
        } else {
            current_item->next = item;
            current_item       = item;
        }

        // Skip newlines, semicolons, comments after clause terminator
        token = parser_peek(parser);
        while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
               token->type == TOKEN_COMMENT) {
            parser_skip(parser);
            token = parser_peek(parser);
        }
    }

    if (parser->error) {
        free_ast(case_node);
        free(case_node);
        return NULL;
    }

    case_node->case_items = first_item;

    // Parse any redirections after 'esac'
    parse_loop_redirects(parser, case_node, session);

    return case_node;
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

#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS)
static void parse_loop_redirects(Parser *parser, ASTNode *node,
                                 Session *session) {
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
static bool is_valid_var_name(const char *name) {
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
static ASTNode *parse_for(Parser *parser, Session *session) {
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

    // Skip newlines or comments before 'in' or ';' or 'do'
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

        // Parse wordlist until ';', newline, 'do', or EOF
        while (true) {
            token = parser_peek(parser);
            if (token->type == TOKEN_SEMICOLON || token->type == TOKEN_EOL ||
                token->type == TOKEN_DO || token->type == TOKEN_EOF ||
                token->type == TOKEN_COMMENT) {
                break;
            }

            LexerToken word     = parser_next(parser);
            int        sub_type = 0;
            if (word.type == TOKEN_PROCESS_SUBSTITUTION_IN) {
                sub_type = 1;
            } else if (word.type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
                sub_type = 2;
            }
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

    // Skip newlines, semicolons, comments before 'do'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
           token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // Expect 'do'
    if (token->type != TOKEN_DO) {
        fprintf(stderr, "Syntax error: expected 'do' in for loop\n");
        parser->error = true;
        free_ast(for_node);
        free(for_node);
        return NULL;
    }
    parser_skip(parser); // consume 'do'

    // Skip newlines, semicolons, comments after 'do'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
           token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // Parse the body commands until 'done'
    ASTNode *body = NULL;
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

        ASTNode *cmd = parse_conditional(parser, session);
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
        fprintf(stderr, "Syntax error: expected 'done' to close for loop\n");
        parser->error = true;
    }

    for_node->left = body;

    // Parse any redirections after 'done' (e.g. for ... done > out.txt)
    parse_loop_redirects(parser, for_node, session);

    return for_node;
}

/* Parse a while or until loop */
static ASTNode *parse_while_or_until(Parser *parser, Session *session,
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

    // Parse the condition commands until 'do'
    ASTNode *condition = NULL;
    while (true) {
        token = parser_peek(parser);
        if (token->type == TOKEN_DO) {
            break;
        }
        if (token->type == TOKEN_EOF) {
            fprintf(stderr,
                    "Syntax error: unexpected EOF in %s loop condition\n",
                    name);
            parser->error = true;
            break;
        }

        ASTNode *cmd = parse_conditional(parser, session);
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

    // Expect and consume 'do'
    token = parser_peek(parser);
    if (token->type != TOKEN_DO) {
        fprintf(stderr, "Syntax error: expected 'do' in %s loop\n", name);
        parser->error = true;
        free_ast(condition);
        free(condition);
        free_ast(loop_node);
        free(loop_node);
        return NULL;
    }
    parser_skip(parser); // consume 'do'

    // Skip newlines, semicolons, comments after 'do'
    token = parser_peek(parser);
    while (token->type == TOKEN_EOL || token->type == TOKEN_SEMICOLON ||
           token->type == TOKEN_COMMENT) {
        parser_skip(parser);
        token = parser_peek(parser);
    }

    // Parse the body commands until 'done'
    ASTNode *body = NULL;
    while (true) {
        token = parser_peek(parser);
        if (token->type == TOKEN_DONE) {
            break;
        }
        if (token->type == TOKEN_EOF) {
            fprintf(stderr, "Syntax error: unexpected EOF in %s loop\n", name);
            parser->error = true;
            break;
        }

        ASTNode *cmd = parse_conditional(parser, session);
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

    loop_node->left  = condition;
    loop_node->right = body;

    // Parse any redirections after 'done'
    parse_loop_redirects(parser, loop_node, session);

    return loop_node;
}

static ASTNode *parse_while(Parser *parser, Session *session) {
    return parse_while_or_until(parser, session, false);
}

static ASTNode *parse_until(Parser *parser, Session *session) {
    return parse_while_or_until(parser, session, true);
}
#endif

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

#ifndef TIDESH_DISABLE_BLOCKS
    if (peek->type == TOKEN_LBRACE) {
        parser_skip(parser);
        ASTNode   *block_body = parse_sequence(parser, session);
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
    if (peek->type == TOKEN_CASE) {
        return parse_case(parser, session);
    }
#endif

#ifndef TIDESH_DISABLE_LOOPS
    if (peek->type == TOKEN_FOR) {
        return parse_for(parser, session);
    }
    if (peek->type == TOKEN_WHILE) {
        return parse_while(parser, session);
    }
    if (peek->type == TOKEN_UNTIL) {
        return parse_until(parser, session);
    }
#endif

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
#endif /* TIDESH_DISABLE_ASSIGNMENTS */
        }

#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
        if (token->type == TOKEN_WORD ||
            token->type == TOKEN_PROCESS_SUBSTITUTION_IN ||
            token->type == TOKEN_PROCESS_SUBSTITUTION_OUT) {
            LexerToken word  = parser_next(parser);
            Array     *parts = NULL;

#ifndef TIDESH_DISABLE_FUNCTIONS
            // Check for function definition: name ()
            if (session && session->features.functions && first_word &&
                word.type == TOKEN_WORD) {
                if (parser_peek(parser)->type == TOKEN_LPAREN) {
                    parser_skip(parser); // consume '('
                    if (parser_peek(parser)->type == TOKEN_RPAREN) {
                        parser_skip(parser); // consume ')'
                        ASTNode *body = parse_command(parser, session);
                        if (body) {
                            cmd->type = NODE_FUNCTION_DEF;
                            add_argument(cmd, word.value, 0);
                            cmd->left = body;
                            free_lexer_token(&word);
                            return cmd;
                        }
                    }
                }
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

const char *node_type_to_string(NodeType type) {
    switch (type) {
        case NODE_COMMAND:
            return "NODE_COMMAND";
#ifndef TIDESH_DISABLE_PIPES
        case NODE_PIPE:
            return "NODE_PIPE";
#endif
#ifndef TIDESH_DISABLE_SEQUENCES
        case NODE_AND:
            return "NODE_AND";
        case NODE_OR:
            return "NODE_OR";
        case NODE_SEQUENCE:
            return "NODE_SEQUENCE";
#endif
#ifndef TIDESH_DISABLE_SUBSHELLS
        case NODE_SUBSHELL:
            return "NODE_SUBSHELL";
#endif
#ifndef TIDESH_DISABLE_BLOCKS
        case NODE_BLOCK:
            return "NODE_BLOCK";
#endif
#ifndef TIDESH_DISABLE_CONDITIONALS
        case NODE_CONDITIONAL:
            return "NODE_CONDITIONAL";
        case NODE_CASE:
            return "NODE_CASE";
#endif
#ifndef TIDESH_DISABLE_FUNCTIONS
        case NODE_FUNCTION_DEF:
            return "NODE_FUNCTION_DEF";
#endif
#ifndef TIDESH_DISABLE_LOOPS
        case NODE_FOR:
            return "NODE_FOR";
        case NODE_WHILE:
            return "NODE_WHILE";
        case NODE_UNTIL:
            return "NODE_UNTIL";
#endif
        default:
            return "NODE_UNKNOWN";
    }
}
