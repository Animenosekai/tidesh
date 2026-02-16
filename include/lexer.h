/** lexer.h
 *
 * Declarations for lexical analysis (lexer).
 * This module provides functions to tokenize input strings into
 * meaningful tokens for further parsing and execution.
 */

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h> /* size_t */

#include "session.h" /* Session */

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

/* The types of token a shell might encounter */
typedef enum TokenType {
    TOKEN_WORD,
    TOKEN_IO_NUMBER,  // e.g., n>..., n<...
    TOKEN_COMMENT,    // # ...
    TOKEN_ASSIGNMENT, // VAR=VALUE

    TOKEN_PIPE, // |

    TOKEN_REDIRECT_IN,             // <
    TOKEN_FD_DUPLICATION,          // <&
    TOKEN_PROCESS_SUBSTITUTION_IN, // <(
    TOKEN_HEREDOC,                 // <<
    TOKEN_HERESTRING,              // <<<

    TOKEN_REDIRECT_OUT,             // >
    TOKEN_REDIRECT_APPEND,          // >>
    TOKEN_REDIRECT_OUT_ERR,         // >&
    TOKEN_PROCESS_SUBSTITUTION_OUT, // >(

    TOKEN_BACKGROUND, // &
    TOKEN_SEQUENCE,   // &&
    TOKEN_OR,         // ||

    TOKEN_SEMICOLON, // ;
    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )

    TOKEN_EOL, // end of command (real newline)
    TOKEN_EOF  // end of file
} TokenType;

/* A token produced by the lexer */
typedef struct LexerToken {
    /* The type of the token */
    TokenType type;
    /* The value of the token (ex: for `TOKEN_WORD`, `TOKEN_IO_NUMBER`, etc.) */
    char *value;
    /* Extra information about the token
    (ex: `TOKEN_ASSIGNMENT` has the variable name as value,
    and the assigned value as extra) */
    char *extra;
} LexerToken;

/**
 * Get the next token from the input
 *
 * @param input Pointer to LexerInput
 * @return The next LexerToken
 */
LexerToken lexer_next_token(LexerInput *input);

/**
 * Free the resources associated with a LexerToken
 *
 * @param token Pointer to LexerToken to free
 */
void free_lexer_token(LexerToken *token);

#endif /* LEXER_H */
