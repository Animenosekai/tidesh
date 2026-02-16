/** execute.h
 *
 * Definitions for executing commands represented as an Abstract Syntax Tree
 * (AST). Includes functions for finding commands in the PATH and executing AST
 * nodes.
 */

#ifndef EXECUTE_H
#define EXECUTE_H

#include "ast.h"
#include "lexer.h"
#include "session.h"

/**
 * Find the full path of a command by searching the PATH environment variable.
 *
 * @param cmd The command name to find
 * @param session The session context containing environment variables
 * @return The full path to the command if found, NULL otherwise
 */
char *find_in_path(const char *cmd, Session *session);

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

/**
 * A helper function to build arguments and execute a command.
 *
 * @param cmd The command to execute.
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @param prefix_args The arguments to add to the command.
 * @return The exit status of the executed command.
 */
int exec_wrapper(const char *cmd, int argc, char **argv,
                 const char **prefix_args);

#endif /* EXECUTE_H */
