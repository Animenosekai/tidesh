/** function.h
 *
 * Shell function definition, function storage, and positional parameters
 * management.
 */

#ifndef EXECUTE_FUNCTION_H
#define EXECUTE_FUNCTION_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

#include "ast.h"
#include "data/trie.h"
#include "session.h"

#ifndef TIDESH_DISABLE_FUNCTIONS
typedef struct PosParamBackup {
    char *key;
    char *value;
} PosParamBackup;

ASTNode *load_function_body(Trie *functions, const char *name);
bool     store_function_body(Trie *functions, char *name, ASTNode *body);

bool            is_positional_param(const char *key);
void            clear_positional_params(Session *session);
PosParamBackup *backup_and_clear_positional_params(Session *session,
                                                   size_t  *count_out);
void restore_positional_params(Session *session, PosParamBackup *backups,
                               size_t count);
#endif

int execute_function_def(ASTNode *node, Session *session);

#endif /* EXECUTE_FUNCTION_H */
