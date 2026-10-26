#include <ctype.h>  /* isdigit */
#include <stdlib.h> /* malloc, free, calloc */
#include <string.h> /* strdup, memcpy */

#include "ast/node.h"

bool is_all_digits(const char *str) {
    if (!str || !*str)
        return false;
    for (const char *p = str; *p; p++) {
        if (!isdigit((unsigned char)*p))
            return false;
    }
    return true;
}

/* Create a new AST node of given type */
ASTNode *init_ast(ASTNode *node, NodeType type) {
    if (!node)
        node = malloc(sizeof(ASTNode));
    if (!node)
        return NULL;
    node->type        = type;
    node->argv        = NULL;
    node->arg_is_sub  = NULL;
    node->arg_keys    = NULL;
    node->argc        = 0;
    node->redirects   = NULL;
    node->assignments = NULL;
    node->left        = NULL;
    node->right       = NULL;
    node->background  = false;
#ifndef TIDESH_DISABLE_CONDITIONALS
    node->branches   = NULL;
    node->case_items = NULL;
#endif
#ifndef TIDESH_DISABLE_LOOPS
    node->for_has_in = false;
#endif
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    node->with_contexts = NULL;
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

Redirection *copy_redirects(const Redirection *redirect) {
    if (!redirect)
        return NULL;

    Redirection *cp = calloc(1, sizeof(Redirection));
    if (!cp)
        return NULL;

    cp->fd     = redirect->fd;
    cp->type   = redirect->type;
    cp->target = redirect->target ? strdup(redirect->target) : NULL;
    cp->is_process_substitution = redirect->is_process_substitution;
    cp->next                    = copy_redirects(redirect->next);
    return cp;
}

#ifndef TIDESH_DISABLE_CONDITIONALS
/* Free conditional branches */
void free_conditional_branches(ConditionalBranch *branch) {
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
void free_case_items(CaseItem *item) {
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

ConditionalBranch *copy_conditional_branches(const ConditionalBranch *branch) {
    if (!branch)
        return NULL;
    ConditionalBranch *cp = calloc(1, sizeof(ConditionalBranch));
    if (!cp)
        return NULL;
    cp->condition = copy_ast(branch->condition);
    cp->body      = copy_ast(branch->body);
    cp->next      = copy_conditional_branches(branch->next);
    return cp;
}

CaseItem *copy_case_items(const CaseItem *item) {
    if (!item)
        return NULL;
    CaseItem *cp = calloc(1, sizeof(CaseItem));
    if (!cp)
        return NULL;
    cp->terminator    = item->terminator;
    cp->pattern_count = item->pattern_count;
    if (item->pattern_count > 0 && item->patterns) {
        cp->patterns = calloc(item->pattern_count, sizeof(char *));
        for (int i = 0; i < item->pattern_count; i++) {
            cp->patterns[i] =
                item->patterns[i] ? strdup(item->patterns[i]) : NULL;
        }
    }
    cp->body = copy_ast(item->body);
    cp->next = copy_case_items(item->next);
    return cp;
}
#endif

void free_catch_clauses(CatchClause *clause) {
    while (clause) {
        CatchClause *next = clause->next;
        if (clause->exit_codes) {
            free(clause->exit_codes);
        }
        if (clause->body) {
            free_ast(clause->body);
            free(clause->body);
        }
        free(clause);
        clause = next;
    }
}

CatchClause *copy_catch_clauses(const CatchClause *clause) {
    if (!clause)
        return NULL;
    CatchClause *cp = calloc(1, sizeof(CatchClause));
    if (!cp)
        return NULL;
    cp->code_count   = clause->code_count;
    cp->is_catch_all = clause->is_catch_all;
    if (clause->code_count > 0 && clause->exit_codes) {
        cp->exit_codes = calloc(clause->code_count, sizeof(int));
        if (cp->exit_codes) {
            for (size_t i = 0; i < clause->code_count; i++) {
                cp->exit_codes[i] = clause->exit_codes[i];
            }
        }
    }
    cp->body = copy_ast(clause->body);
    cp->next = copy_catch_clauses(clause->next);
    return cp;
}

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

    if (node->arg_keys) {
        for (int i = 0; i < node->argc; i++) {
            if (node->arg_keys[i]) {
                free(node->arg_keys[i]);
            }
        }
        free(node->arg_keys);
        node->arg_keys = NULL;
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
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    if (node->with_contexts) {
        free_context_invocations(node->with_contexts);
        node->with_contexts = NULL;
    }
#endif
    if (node->catch_clauses) {
        free_catch_clauses(node->catch_clauses);
        node->catch_clauses = NULL;
    }
    if (node->finally_body) {
        free_ast(node->finally_body);
        free(node->finally_body);
        node->finally_body = NULL;
    }
}

ASTNode *copy_ast(const ASTNode *node) {
    if (!node)
        return NULL;

    ASTNode *cp = calloc(1, sizeof(ASTNode));
    if (!cp)
        return NULL;

    cp->type       = node->type;
    cp->background = node->background;
    cp->argc       = node->argc;

    if (node->argc > 0 && node->argv) {
        cp->argv = calloc(node->argc + 1, sizeof(char *));
        if (cp->argv) {
            for (int i = 0; i < node->argc; i++) {
                cp->argv[i] = node->argv[i] ? strdup(node->argv[i]) : NULL;
            }
        }
    }

    if (node->argc > 0 && node->arg_is_sub) {
        cp->arg_is_sub = calloc(node->argc, sizeof(int));
        if (cp->arg_is_sub) {
            memcpy(cp->arg_is_sub, node->arg_is_sub, node->argc * sizeof(int));
        }
    }

    if (node->argc > 0 && node->arg_keys) {
        cp->arg_keys = calloc(node->argc + 1, sizeof(char *));
        if (cp->arg_keys) {
            for (int i = 0; i < node->argc; i++) {
                cp->arg_keys[i] =
                    node->arg_keys[i] ? strdup(node->arg_keys[i]) : NULL;
            }
        }
    }

    if (node->assignments) {
        cp->assignments = array_copy(node->assignments, NULL);
    }

    if (node->redirects) {
        cp->redirects = copy_redirects(node->redirects);
    }

    cp->left  = copy_ast(node->left);
    cp->right = copy_ast(node->right);

#ifndef TIDESH_DISABLE_CONDITIONALS
    if (node->branches) {
        cp->branches = copy_conditional_branches(node->branches);
    }
    if (node->case_items) {
        cp->case_items = copy_case_items(node->case_items);
    }
#endif

#ifndef TIDESH_DISABLE_LOOPS
    cp->for_has_in = node->for_has_in;
#endif

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    if (node->with_contexts) {
        cp->with_contexts = copy_context_invocations(node->with_contexts);
    }
#endif

    if (node->catch_clauses) {
        cp->catch_clauses = copy_catch_clauses(node->catch_clauses);
    }
    if (node->finally_body) {
        cp->finally_body = copy_ast(node->finally_body);
    }

    return cp;
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
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
        case NODE_WITH:
            return "NODE_WITH";
#endif
        case NODE_TRY:
            return "NODE_TRY";
        default:
            return "NODE_UNKNOWN";
    }
}
