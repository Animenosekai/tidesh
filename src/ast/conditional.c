#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, calloc */
#include <string.h> /* strcmp, strdup */

#include "ast/command.h"
#include "ast/conditional.h"
#include "ast/loop.h"
#include "ast/node.h"
#include "ast/parser.h"
#include "ast/pipeline.h"
#include "data/array.h"
#include "session.h"

#ifndef TIDESH_DISABLE_CONDITIONALS
/* Parse conditional statements (if/then/else/elif/fi and case/in/esac) */
ASTNode *ast_parse_conditional(Parser *parser, Session *session) {
    LexerToken *token = parser_peek(parser);

    if (token->type == TOKEN_CASE) {
        return ast_parse_case(parser, session);
    }

    // Not an if statement, delegate to and_or
    if (token->type != TOKEN_IF) {
        return ast_parse_and_or(parser, session);
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
        ASTNode *condition = ast_parse_and_or(parser, session);
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

            ASTNode *cmd = ast_parse_conditional(parser, session);
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

                ASTNode *cmd = ast_parse_conditional(parser, session);
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
ASTNode *ast_parse_case(Parser *parser, Session *session) {
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
    ast_parse_loop_redirects(parser, case_node, session);

    return case_node;
}
#endif
