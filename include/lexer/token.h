/** token.h
 *
 * Token definitions and token-related utilities for the lexer.
 */

#ifndef LEXER_TOKEN_H
#define LEXER_TOKEN_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

/* The types of token a shell might encounter */
typedef enum TokenType {
    TOKEN_WORD,
    TOKEN_COMMENT, // # ...

#ifndef TIDESH_DISABLE_REDIRECTIONS
    TOKEN_IO_NUMBER, // e.g., n>..., n<...
#endif

#ifndef TIDESH_DISABLE_ASSIGNMENTS
    TOKEN_ASSIGNMENT, // VAR=VALUE
#endif

#ifndef TIDESH_DISABLE_PIPES
    TOKEN_PIPE, // |
#endif

#ifndef TIDESH_DISABLE_REDIRECTIONS
    TOKEN_REDIRECT_IN,    // <
    TOKEN_FD_DUPLICATION, // <&
    TOKEN_HEREDOC,        // <<
    TOKEN_HERESTRING,     // <<<

    TOKEN_REDIRECT_OUT,        // >
    TOKEN_REDIRECT_APPEND,     // >>
    TOKEN_REDIRECT_OUT_ERR,    // >&
    TOKEN_REDIRECT_ALL,        // &>
    TOKEN_REDIRECT_APPEND_ERR, // &>>, >>&
#endif

#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
    TOKEN_PROCESS_SUBSTITUTION_IN,  // <(
    TOKEN_PROCESS_SUBSTITUTION_OUT, // >(
#endif

#ifndef TIDESH_DISABLE_JOB_CONTROL
    TOKEN_BACKGROUND, // &
#endif

#ifndef TIDESH_DISABLE_SEQUENCES
    TOKEN_SEQUENCE, // &&
#endif

#ifndef TIDESH_DISABLE_PIPES
    TOKEN_OR, // ||
#endif

#ifndef TIDESH_DISABLE_SEQUENCES
    TOKEN_SEMICOLON, // ;
#endif

#ifndef TIDESH_DISABLE_SUBSHELLS
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
#endif

#ifndef TIDESH_DISABLE_BLOCKS
    TOKEN_LBRACE, // {
    TOKEN_RBRACE, // }
#endif

#ifndef TIDESH_DISABLE_CONDITIONALS
    TOKEN_IF,               // if
    TOKEN_THEN,             // then
    TOKEN_ELSE,             // else
    TOKEN_ELIF,             // elif
    TOKEN_FI,               // fi
    TOKEN_CASE,             // case
    TOKEN_ESAC,             // esac
    TOKEN_DOUBLE_SEMICOLON, // ;;
    TOKEN_CASE_FALLTHROUGH, // ;&
    TOKEN_CASE_CONTINUE,    // ;;&
#endif

#ifndef TIDESH_DISABLE_LOOPS
    TOKEN_FOR,   // for
    TOKEN_WHILE, // while
    TOKEN_UNTIL, // until
#endif
#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS)
    TOKEN_IN, // in
#endif
#ifndef TIDESH_DISABLE_LOOPS
    TOKEN_DO,   // do
    TOKEN_DONE, // done
#endif
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    TOKEN_WITH, // with
#endif

    TOKEN_TRY,     // try
    TOKEN_CATCH,   // catch
    TOKEN_FINALLY, // finally

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
 * Free the resources associated with a LexerToken
 *
 * @param token Pointer to LexerToken to free
 */
void free_lexer_token(LexerToken *token);

/**
 * Get string representation of a token type
 *
 * @param type The TokenType
 * @return String representation of the token type (e.g. "TOKEN_WORD")
 */
const char *token_type_to_string(TokenType type);

#endif /* LEXER_TOKEN_H */
