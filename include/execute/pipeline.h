/** pipeline.h
 *
 * Execution of pipelines, sequences, and-or operators, subshells, and blocks.
 */

#ifndef EXECUTE_PIPELINE_H
#define EXECUTE_PIPELINE_H

#include "ast.h"
#include "session.h"

int execute_pipe(ASTNode *node, Session *session);
int execute_sequence(ASTNode *node, Session *session);
int execute_and(ASTNode *node, Session *session);
int execute_or(ASTNode *node, Session *session);
int execute_subshell(ASTNode *node, Session *session);
int execute_block(ASTNode *node, Session *session);

#endif /* EXECUTE_PIPELINE_H */
