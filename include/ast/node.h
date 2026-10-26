/** node.h
 *
 * Abstract Syntax Tree (AST) node structures and lifecycle functions.
 */

#ifndef AST_NODE_H
#define AST_NODE_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

#include "context.h"
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
#ifndef TIDESH_DISABLE_BLOCKS
    ,
    NODE_BLOCK
#endif
#ifndef TIDESH_DISABLE_CONDITIONALS
    ,
    NODE_CONDITIONAL,
    NODE_CASE
#endif
#ifndef TIDESH_DISABLE_FUNCTIONS
    ,
    NODE_FUNCTION_DEF
#endif
#ifndef TIDESH_DISABLE_LOOPS
    ,
    NODE_FOR,
    NODE_WHILE,
    NODE_UNTIL
#endif
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    ,
    NODE_WITH
#endif
} NodeType;

/* Describes an I/O redirection */
typedef struct Redirection {
    int                 fd;
    TokenType           type;
    char               *target;
    bool                is_process_substitution;
    struct Redirection *next;
} Redirection;

#ifndef TIDESH_DISABLE_CONDITIONALS
/* Describes a branch in a conditional statement */
typedef struct ConditionalBranch {
    /* Condition AST node (for if/elif) */
    struct ASTNode *condition;
    /* Commands to execute if condition succeeds */
    struct ASTNode *body;
    /* Next branch (for elif/else) */
    struct ConditionalBranch *next;
} ConditionalBranch;

/* The terminator for a case clause */
typedef enum CaseTerminator {
    CASE_TERM_BREAK,       /* ;; */
    CASE_TERM_FALLTHROUGH, /* ;& */
    CASE_TERM_CONTINUE     /* ;;& */
} CaseTerminator;

/* Describes a clause (item) in a case statement */
typedef struct CaseItem {
    /* Array of pattern strings (e.g., from pattern1 | pattern2) */
    char **patterns;
    /* Number of patterns in this clause */
    int pattern_count;
    /* Commands to execute if pattern matches */
    struct ASTNode *body;
    /* How to proceed after this clause (;; / ;& / ;;&) */
    CaseTerminator terminator;
    /* Next case clause */
    struct CaseItem *next;
} CaseItem;
#endif

/* Describes an AST node */
typedef struct ASTNode {
    /* The type of the node */
    NodeType type;
    /* For command nodes */
    char **argv;
    /* Whether each argument is a process substitution (0: none, 1: <(, 2: >())
     */
    int *arg_is_sub;
    /* Named argument keys (arg_keys[i] is parameter name if named, NULL if
     * positional) */
    char **arg_keys;
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
#ifndef TIDESH_DISABLE_CONDITIONALS
    /* For conditional nodes */
    ConditionalBranch *branches;
    /* For case nodes */
    CaseItem *case_items;
#endif
#ifndef TIDESH_DISABLE_LOOPS
    /* For for loop nodes */
    bool for_has_in;
#endif
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    /* For with context manager nodes */
    ContextInvocation *with_contexts;
#endif
} ASTNode;

/* Check if string contains only digits */
bool is_all_digits(const char *str);

/* Create and initialize an AST node */
ASTNode *init_ast(ASTNode *node, NodeType type);

/* Free all redirections */
void free_redirects(Redirection *r);

/* Copy all redirections */
Redirection *copy_redirects(const Redirection *r);

#ifndef TIDESH_DISABLE_CONDITIONALS
void               free_conditional_branches(ConditionalBranch *branch);
ConditionalBranch *copy_conditional_branches(const ConditionalBranch *branch);
void               free_case_items(CaseItem *item);
CaseItem          *copy_case_items(const CaseItem *item);
#endif

/* Free an AST */
void free_ast(ASTNode *node);

/* Deep copy an AST */
ASTNode *copy_ast(const ASTNode *node);

/**
 * Get string representation of an AST node type
 *
 * @param type The NodeType
 * @return String representation of the node type (e.g. "NODE_COMMAND")
 */
const char *node_type_to_string(NodeType type);

#endif /* AST_NODE_H */
