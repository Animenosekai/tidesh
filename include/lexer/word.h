/** word.h
 *
 * Word tokenization, quoted strings, parameter expansions, and keywords for
 * lexer.
 */

#ifndef LEXER_WORD_H
#define LEXER_WORD_H

#include "data/dynamic.h"
#include "lexer/input.h"
#include "lexer/token.h"

#if !defined(TIDESH_DISABLE_COMMAND_SUBSTITUTION) ||                           \
    !defined(TIDESH_DISABLE_EXPANSIONS)
char *command_substitution(LexerInput *input);
#endif

#ifndef TIDESH_DISABLE_EXPANSIONS
char *parameter_expansion_body(LexerInput *input);
#endif

Dynamic read_single_word(LexerInput *input);
Dynamic read_quoted_word(LexerInput *input);

#ifndef TIDESH_DISABLE_CONDITIONALS
TokenType check_conditional_keyword(const char *word);
#endif

#ifndef TIDESH_DISABLE_LOOPS
TokenType check_loop_keyword(const char *word);
#endif

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
TokenType check_context_keyword(const char *word);
#endif

LexerToken read_word_token(LexerInput *input);

#endif /* LEXER_WORD_H */
