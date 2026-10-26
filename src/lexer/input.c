#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* size_t, NULL */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* strlen */

#include "lexer/input.h"
#include "session.h" /* Session */

int is_at_end(LexerInput *input) { return input->data[input->pos] == '\0'; }

char peek(LexerInput *input) { return input->data[input->pos]; }

char peek_next(LexerInput *input) {
    if (is_at_end(input) || input->data[input->pos + 1] == '\0') {
        return '\0';
    }
    return input->data[input->pos + 1];
}

char advance(LexerInput *input) {
    return is_at_end(input) ? '\0' : input->data[input->pos++];
}

void skip_whitespaces(LexerInput *input) {
    char c = peek(input);
    while (c == ' ' || c == '\t' || c == '\r') {
        advance(input);
        c = peek(input);
    }
}

char *remove_line_continuations(const char *input) {
    if (!input) {
        return NULL;
    }
    size_t len    = strlen(input);
    char  *result = malloc(len + 1);
    if (!result) {
        return NULL;
    }

    size_t r               = 0;
    bool   in_single_quote = false;
    bool   in_double_quote = false;
    size_t i               = 0;

    while (i < len) {
        if (!in_single_quote && !in_double_quote) {
            if (input[i] == '\'') {
                in_single_quote = true;
                result[r++]     = input[i++];
            } else if (input[i] == '"') {
                in_double_quote = true;
                result[r++]     = input[i++];
            } else if (input[i] == '\\') {
                if (i + 1 < len && input[i + 1] == '\n') {
                    // Line continuation: skip '\\' and '\n'
                    i += 2;
                } else if (i + 2 < len && input[i + 1] == '\r' &&
                           input[i + 2] == '\n') {
                    // Line continuation: skip '\\', '\r', and '\n'
                    i += 3;
                } else {
                    // Normal backslash outside quotes: copy '\\' and the next
                    // char (if any)
                    result[r++] = input[i++];
                    if (i < len) {
                        result[r++] = input[i++];
                    }
                }
            } else {
                result[r++] = input[i++];
            }
        } else if (in_single_quote) {
            if (input[i] == '\'') {
                in_single_quote = false;
            }
            result[r++] = input[i++];
        } else if (in_double_quote) {
            if (input[i] == '"') {
                in_double_quote = false;
                result[r++]     = input[i++];
            } else if (input[i] == '\\') {
                if (i + 1 < len && input[i + 1] == '\n') {
                    // Line continuation inside double quotes
                    i += 2;
                } else if (i + 2 < len && input[i + 1] == '\r' &&
                           input[i + 2] == '\n') {
                    // Line continuation inside double quotes (CRLF)
                    i += 3;
                } else {
                    result[r++] = input[i++];
                    if (i < len) {
                        result[r++] = input[i++];
                    }
                }
            } else {
                result[r++] = input[i++];
            }
        }
    }

    result[r] = '\0';
    return result;
}

LexerInput *init_lexer_input(LexerInput *input, char *data,
                             char *(*execute)(const char *cmd,
                                              Session    *session),
                             Session *session) {
    if (!input) {
        input = malloc(sizeof(LexerInput));
        if (!input) {
            return NULL;
        }
    }
    input->data    = remove_line_continuations(data);
    input->pos     = 0;
    input->execute = execute;
    input->session = session;
    return input;
}

void free_lexer_input(LexerInput *input) {
    if (input) {
        free(input->data);
    }
}
