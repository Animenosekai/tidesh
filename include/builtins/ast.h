/** ast.h
 *
 * Builtin command to parse shell commands and display the Abstract Syntax Tree.
 */

#ifndef BUILTINS_AST_H
#define BUILTINS_AST_H

#include "session.h"

int builtin_ast(int argc, char **argv, Session *session);

#endif /* BUILTINS_AST_H */
