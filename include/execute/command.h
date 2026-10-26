/** command.h
 *
 * Execution of simple commands, argument expansion, builtins, and external
 * processes.
 */

#ifndef EXECUTE_COMMAND_H
#define EXECUTE_COMMAND_H

#include "ast.h"
#include "session.h"

char *expand_word_no_split(const char *raw_value, Session *session);

#ifndef TIDESH_DISABLE_ASSIGNMENTS
char *expand_assignment_value(const char *raw_value, Session *session);
#endif

char *build_command_string(char **argv, int argc);
void  free_expanded_args(int argc, char **argv, char **arg_keys,
                         int *arg_is_sub);

int execute_command(ASTNode *node, Session *session);

#endif /* EXECUTE_COMMAND_H */
