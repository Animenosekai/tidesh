/** lexer.h
 *
 * Declarations for lexical analysis (lexer).
 * This module provides functions to tokenize input strings into
 * meaningful tokens for further parsing and execution.
 */

#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

#include "session.h" /* Session */

// Lexer submodules
#include "lexer/input.h"
#include "lexer/token.h"
#include "lexer/word.h"

/**
 * Get the next token from the input
 *
 * @param input Pointer to LexerInput
 * @return The next LexerToken
 */
LexerToken lexer_next_token(LexerInput *input);

#endif /* LEXER_H */
