/** lexer.h
 *
 * Builtin command to run lexical analysis and display tokens.
 */

#ifndef BUILTINS_LEXER_H
#define BUILTINS_LEXER_H

#include "session.h"

int builtin_lexer(int argc, char **argv, Session *session);

#endif /* BUILTINS_LEXER_H */
