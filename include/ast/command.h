/** command.h
 *
 * Parsing of simple commands, function definitions, subshells, and arguments.
 */

#ifndef AST_COMMAND_H
#define AST_COMMAND_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

#include "ast/parser.h"

void add_argument(ASTNode *node, char *arg, int sub_type);
void add_named_argument(ASTNode *node, const char *key, const char *arg,
                        int sub_type);
bool is_valid_param_name(const char *name);

typedef struct FuncArgItem {
    char *key;
    char *val;
} FuncArgItem;

void add_func_arg_item(FuncArgItem **items, size_t *count, size_t *cap,
                       const char *key, const char *val);
void free_func_arg_items(FuncArgItem *items, size_t count);

ASTNode *ast_parse_command(Parser *parser, Session *session);

#endif /* AST_COMMAND_H */
