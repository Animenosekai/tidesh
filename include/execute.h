/** execute.h
 *
 * Definitions for executing commands represented as an Abstract Syntax Tree
 * (AST). Aggregate header including command lookup, redirection, functions,
 * pipelines, conditionals, loops, context managers, command execution, and
 * string execution.
 */

#ifndef EXECUTE_H
#define EXECUTE_H

#include <stdbool.h> /* bool */

#include "ast.h"
#include "lexer.h"
#include "session.h"

// Execute submodules
#include "execute/command.h"
#include "execute/conditional.h"
#include "execute/function.h"
#include "execute/lookup.h"
#include "execute/loop.h"
#include "execute/pipeline.h"
#include "execute/redirection.h"
#include "execute/with.h"

/**
 * Execute the given AST node in the context of the given session.
 *
 * @param node The AST node to execute
 * @param session The session context
 * @return The exit status of the executed command
 */
int execute(ASTNode *node, Session *session);

/**
 * Execute a command string in the context of the given session.
 *
 * @param cmd The command string to execute
 * @param session The session context
 * @return The exit status of the executed command
 */
int execute_string(const char *cmd, Session *session);

/**
 * Execute a command string and capture its standard output.
 *
 * @param cmd The command string to execute
 * @param session The session context
 * @return The captured standard output of the command
 */
char *execute_string_stdout(const char *cmd, Session *session);

#endif /* EXECUTE_H */
