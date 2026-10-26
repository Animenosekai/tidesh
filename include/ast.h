/** ast.h
 *
 * Definitions for the Abstract Syntax Tree (AST) used in command parsing.
 * Aggregate header including AST nodes, parser state, grammar parsers,
 * and the main parse entrypoint.
 */

#ifndef AST_H
#define AST_H

#include "context.h"
#include "data/array.h"
#include "lexer.h"
#include "session.h"

// AST submodules
#include "ast/command.h"
#include "ast/conditional.h"
#include "ast/loop.h"
#include "ast/node.h"
#include "ast/parser.h"
#include "ast/pipeline.h"
#include "ast/with.h"

/**
 * The main parse function.
 *
 * Note that we are parsing things in the following order:
 *   sequence -> and/or -> pipeline -> command
 *
 * @param lexer The lexer input
 * @param session The session context
 * @return The root AST node of the parsed command structure
 */
ASTNode *parse(LexerInput *lexer, Session *session);

#endif /* AST_H */
