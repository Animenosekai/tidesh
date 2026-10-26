#include <stdlib.h> /* free, NULL */

#include "lexer/token.h"

void free_lexer_token(LexerToken *token) {
    if (!token) {
        return;
    }
    if (token->value) {
        free(token->value);
        token->value = NULL;
    }
    if (token->extra) {
        free(token->extra);
        token->extra = NULL;
    }
}

const char *token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_WORD:
            return "TOKEN_WORD";
        case TOKEN_COMMENT:
            return "TOKEN_COMMENT";
#ifndef TIDESH_DISABLE_REDIRECTIONS
        case TOKEN_IO_NUMBER:
            return "TOKEN_IO_NUMBER";
        case TOKEN_REDIRECT_IN:
            return "TOKEN_REDIRECT_IN";
        case TOKEN_FD_DUPLICATION:
            return "TOKEN_FD_DUPLICATION";
        case TOKEN_HEREDOC:
            return "TOKEN_HEREDOC";
        case TOKEN_HERESTRING:
            return "TOKEN_HERESTRING";
        case TOKEN_REDIRECT_OUT:
            return "TOKEN_REDIRECT_OUT";
        case TOKEN_REDIRECT_APPEND:
            return "TOKEN_REDIRECT_APPEND";
        case TOKEN_REDIRECT_OUT_ERR:
            return "TOKEN_REDIRECT_OUT_ERR";
        case TOKEN_REDIRECT_ALL:
            return "TOKEN_REDIRECT_ALL";
        case TOKEN_REDIRECT_APPEND_ERR:
            return "TOKEN_REDIRECT_APPEND_ERR";
#endif
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
        case TOKEN_PROCESS_SUBSTITUTION_IN:
            return "TOKEN_PROCESS_SUBSTITUTION_IN";
        case TOKEN_PROCESS_SUBSTITUTION_OUT:
            return "TOKEN_PROCESS_SUBSTITUTION_OUT";
#endif
#ifndef TIDESH_DISABLE_JOB_CONTROL
        case TOKEN_BACKGROUND:
            return "TOKEN_BACKGROUND";
#endif
#ifndef TIDESH_DISABLE_SEQUENCES
        case TOKEN_SEQUENCE:
            return "TOKEN_SEQUENCE";
        case TOKEN_SEMICOLON:
            return "TOKEN_SEMICOLON";
#endif
#ifndef TIDESH_DISABLE_PIPES
        case TOKEN_PIPE:
            return "TOKEN_PIPE";
        case TOKEN_OR:
            return "TOKEN_OR";
#endif
#ifndef TIDESH_DISABLE_SUBSHELLS
        case TOKEN_LPAREN:
            return "TOKEN_LPAREN";
        case TOKEN_RPAREN:
            return "TOKEN_RPAREN";
#endif
#ifndef TIDESH_DISABLE_BLOCKS
        case TOKEN_LBRACE:
            return "TOKEN_LBRACE";
        case TOKEN_RBRACE:
            return "TOKEN_RBRACE";
#endif
#ifndef TIDESH_DISABLE_CONDITIONALS
        case TOKEN_IF:
            return "TOKEN_IF";
        case TOKEN_THEN:
            return "TOKEN_THEN";
        case TOKEN_ELSE:
            return "TOKEN_ELSE";
        case TOKEN_ELIF:
            return "TOKEN_ELIF";
        case TOKEN_FI:
            return "TOKEN_FI";
        case TOKEN_CASE:
            return "TOKEN_CASE";
        case TOKEN_ESAC:
            return "TOKEN_ESAC";
        case TOKEN_DOUBLE_SEMICOLON:
            return "TOKEN_DOUBLE_SEMICOLON";
        case TOKEN_CASE_FALLTHROUGH:
            return "TOKEN_CASE_FALLTHROUGH";
        case TOKEN_CASE_CONTINUE:
            return "TOKEN_CASE_CONTINUE";
#endif
#ifndef TIDESH_DISABLE_LOOPS
        case TOKEN_FOR:
            return "TOKEN_FOR";
        case TOKEN_WHILE:
            return "TOKEN_WHILE";
        case TOKEN_UNTIL:
            return "TOKEN_UNTIL";
        case TOKEN_DO:
            return "TOKEN_DO";
        case TOKEN_DONE:
            return "TOKEN_DONE";
#endif
#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS)
        case TOKEN_IN:
            return "TOKEN_IN";
#endif
#ifndef TIDESH_DISABLE_ASSIGNMENTS
        case TOKEN_ASSIGNMENT:
            return "TOKEN_ASSIGNMENT";
#endif
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
        case TOKEN_WITH:
            return "TOKEN_WITH";
#endif
        case TOKEN_EOL:
            return "TOKEN_EOL";
        case TOKEN_EOF:
            return "TOKEN_EOF";
        default:
            return "UNKNOWN";
    }
}
