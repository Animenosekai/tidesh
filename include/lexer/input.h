/** input.h
 *
 * Input stream management and cursor navigation for the lexer.
 */

#ifndef LEXER_INPUT_H
#define LEXER_INPUT_H

#include <stddef.h> /* size_t */

typedef struct Session Session;

typedef struct LexerInput {
    /* The input data */
    char *data;
    /* The current position in the input data */
    size_t pos;
    /* A function used to execute commands and get their output */
    char *(*execute)(const char *cmd, Session *session);
    /* The current session */
    Session *session;
} LexerInput;

/**
 * Initialize a LexerInput structure
 *
 * @param input Pointer to LexerInput to initialize, or NULL to allocate a new
 * one
 * @param data Input string to tokenize
 * @param execute Function pointer to execute commands for command substitution
 * @param session Pointer to current Session
 * @return Pointer to initialized LexerInput, or NULL on failure
 */
LexerInput *init_lexer_input(LexerInput *input, char *data,
                             char *(*execute)(const char *cmd,
                                              Session    *session),
                             Session *session);

/**
 * Free the resources associated with a LexerInput
 *
 * @param input Pointer to LexerInput to free
 */
void free_lexer_input(LexerInput *input);

/**
 * Remove line continuations (<backslash><newline>) from input data
 *
 * @param input Input string
 * @return Newly allocated string with continuations removed
 */
char *remove_line_continuations(const char *input);

/* Cursor navigation helpers */
int  is_at_end(LexerInput *input);
char peek(LexerInput *input);
char peek_next(LexerInput *input);
char advance(LexerInput *input);
void skip_whitespaces(LexerInput *input);

#endif /* LEXER_INPUT_H */
