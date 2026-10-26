#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* NULL */

#include "ast/node.h"
#include "ast/parser.h"
#include "ast/pipeline.h"
#include "lexer.h"
#include "session.h"

/* Parse a sequence of commands separated by ;, &, \n */
ASTNode *ast_parse_sequence(Parser *parser, Session *session) {
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
    ASTNode *left = ast_parse_conditional(parser, session);
#else
    ASTNode *left = ast_parse_and_or(parser, session);
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
        } else if (left) {
            ASTNode *last = left;
            while (last && last->type == NODE_SEQUENCE && last->right) {
                last = last->right;
            }
            if (last && (last->type == NODE_CONDITIONAL ||
                         last->type == NODE_FOR || last->type == NODE_WHILE ||
                         last->type == NODE_UNTIL || last->type == NODE_BLOCK ||
                         last->type == NODE_WITH || last->type == NODE_TRY)) {
                if (token->type == TOKEN_EOF || token->type == TOKEN_RPAREN ||
#ifndef TIDESH_DISABLE_BLOCKS
                    token->type == TOKEN_RBRACE ||
#endif
#ifndef TIDESH_DISABLE_LOOPS
                    token->type == TOKEN_DONE ||
#endif
                    token->type == TOKEN_COMMENT) {
                    break;
                }
            } else {
                break;
            }
        } else
#endif
        {
            break;
        }

#ifndef TIDESH_DISABLE_CONDITIONALS
        ASTNode *right = ast_parse_conditional(parser, session);
#else
        ASTNode *right = ast_parse_and_or(parser, session);
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

/* Parse commands connected by && and || */
ASTNode *ast_parse_and_or(Parser *parser, Session *session) {
    ASTNode *left = ast_parse_pipeline(parser, session);
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
        ASTNode *right = ast_parse_pipeline(parser, session);
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
ASTNode *ast_parse_pipeline(Parser *parser, Session *session) {
    ASTNode *left = ast_parse_command(parser, session);
    if (!left)
        return NULL;
#ifndef TIDESH_DISABLE_PIPES
    LexerToken *token = parser_peek(parser);
    if (token->type == TOKEN_PIPE) {
        parser_skip(parser);
        ASTNode *node = init_ast(NULL, NODE_PIPE);
        node->left    = left;
        node->right   = ast_parse_pipeline(parser, session);
        return node;
    }
#endif
    return left;
}
