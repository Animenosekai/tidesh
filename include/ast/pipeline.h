/** pipeline.h
 *
 * Parsing of pipelines, sequences, and logical and/or command structures.
 */

#ifndef AST_PIPELINE_H
#define AST_PIPELINE_H

#include "ast/parser.h"

ASTNode *ast_parse_sequence(Parser *parser, Session *session);
ASTNode *ast_parse_and_or(Parser *parser, Session *session);
ASTNode *ast_parse_pipeline(Parser *parser, Session *session);

#endif /* AST_PIPELINE_H */
