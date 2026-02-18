/** ast.h
 *
 * Definitions for the Abstract Syntax Tree (AST) used in command parsing.
 * Also includes functions for parsing input into an AST and freeing the AST.
 */

#ifndef AST_H
#define AST_H

#include "data/array.h"
#include "lexer.h"
#include "session.h"

/* The type of AST nodes */
typedef enum NodeType {
    NODE_COMMAND,
#ifndef TIDESH_DISABLE_PIPES
    NODE_PIPE,
#endif
#ifndef TIDESH_DISABLE_SEQUENCES
    NODE_AND,
    NODE_OR,
    NODE_SEQUENCE,
#endif
#ifndef TIDESH_DISABLE_SUBSHELLS
    NODE_SUBSHELL
#endif
} NodeType;

/* Descibes an I/O redirection */
typedef struct Redirection {
    int                 fd;
    TokenType           type;
    char               *target;
    bool                is_process_substitution;
    struct Redirection *next;
} Redirection;

/* Descibes an AST node */
typedef struct ASTNode {
    /* The type of the node */
    NodeType type;
    /* For command nodes */
    char **argv;
    /* Whether each argument is a process substitution (0: none, 1: <(, 2: >()
     */
    int *arg_is_sub;
    /* The argument count */
    int argc;
    /* I/O redirections */
    Redirection *redirects;
    /* Variable assignments */
    Array *assignments;
    /* For non-command nodes */
    struct ASTNode *left;
    /* For non-command nodes */
    struct ASTNode *right;
    /* Is this command to be run in background? */
    bool background;
} ASTNode;

/* Free all redirections */
void free_redirects(Redirection *r);

/* Free an AST */
void free_ast(ASTNode *node);

/**
 * The main parse function.
 *
 * Note that we are parsing things in the following order:
 *   sequence -> and/or -> pipeline -> command
 *
 * @param lexer The lexer input
 * @param session The session context
 * @return The root AST node of the parsed command structure
 */
ASTNode *parse(LexerInput *lexer, Session *session);

#endif /* AST_H */
