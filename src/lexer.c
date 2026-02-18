#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* size_t, NULL */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* strlen, strncmp, strcmp */

#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, dynamic_prepend, free_dynamic, dynamic_to_string */
#include "environ.h"      /* Environ, environ_get */
#include "lexer.h"
#include "session.h" /* Session */

/* Check if the input is at the end */
static int is_at_end(LexerInput *input) {
    return input->data[input->pos] == '\0';
}

/* Peek (read without advancing) in the input */
static char peek(LexerInput *input) { return input->data[input->pos]; }

/* Peek the next character in the input */
static char peek_next(LexerInput *input) {
    if (is_at_end(input) || input->data[input->pos + 1] == '\0') {
        return '\0';
    }
    return input->data[input->pos + 1];
}

/* Advance (read and advance) in the input */
static char advance(LexerInput *input) {
    return is_at_end(input) ? '\0' : input->data[input->pos++];
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
    input->data    = strdup(data);
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

void free_lexer_token(LexerToken *token) {
    if (token->value) {
        free(token->value);
        token->value = NULL;
    }
    if (token->extra) {
        free(token->extra);
        token->extra = NULL;
    }
}

/* Handle command substitution $(...) or <(...) or >(...) */
static char *command_substitution(LexerInput *input) {
    char    c       = advance(input); // Consume `(`
    size_t  depth   = 1;
    bool    escaped = false;
    Dynamic command = {0};
    init_dynamic(&command);
    while (depth > 0 && !is_at_end(input)) {
        switch (c = advance(input)) {
            case '(':
                if (!escaped) {
                    depth++;
                }
                dynamic_append(&command, c);
                break;
            case ')':
                if (!escaped) {
                    depth--;
                }
                if (depth == 0) {
                    break;
                }
                dynamic_append(&command, c);
                break;
            case '\\':
                if (!escaped) {
                    escaped = true;
                    continue;
                    ;
                }
                // If already escaped, treat as normal character
                escaped = false;
                dynamic_append(&command, c);
                break;
            default:
                dynamic_append(&command, c);
                break;
        }
    }
    char *result_command = dynamic_to_string(&command);
    free_dynamic(&command);
    return result_command;
}

/* Read a single unquoted word from the input */
static Dynamic read_single_word(LexerInput *input) {
    Dynamic word_value = {0};
    init_dynamic(&word_value);
    char c = peek(input);

    bool escaped = false;
    while (!is_at_end(input) && c != ' ' && c != '\t' && c != '\n' &&
           c != '\r') {
        if (!escaped && c == '\\' && peek_next(input) == '$') {
            advance(input); // consume '\\'
            dynamic_append(&word_value, '\\');
            advance(input); // consume '$'
            dynamic_append(&word_value, '$');
            c = peek(input);
            continue;
        }
        if (!escaped && c == '$' && peek_next(input) == '(') {
            advance(input); // consume '$'
            char *command      = command_substitution(input);
            char *substitution = input->execute(command, input->session);
            free(command);
            dynamic_extend(&word_value, substitution);
            free(substitution);
            c = peek(input);
            continue;
        }
        if (!escaped && c == '\\') {
            escaped = true;
            advance(input);
            c = peek(input);
            continue;
        }
        escaped = false;
        dynamic_append(&word_value, c);
        advance(input);
        c = peek(input);
    }

    return word_value;
}

/* Read a quoted word from the input */
static Dynamic read_quoted_word(LexerInput *input) {
    Dynamic word_value = {0};
    init_dynamic(&word_value);

    bool escaped    = false;
    char quote_char = advance(input);
    char c          = peek(input);

    while (!is_at_end(input) && c != quote_char) {
        if (!escaped && c == '\\' && peek_next(input) == '$') {
            advance(input); // consume '\\'
            dynamic_append(&word_value, '\\');
            advance(input); // consume '$'
            dynamic_append(&word_value, '$');
            c = peek(input);
            continue;
        }
        if (!escaped && c == '$' && peek_next(input) == '(') {
            advance(input); // consume '$'
            char *command      = command_substitution(input);
            char *substitution = input->execute(command, input->session);
            free(command);
            dynamic_extend(&word_value, substitution);
            free(substitution);
            c = peek(input);
            continue;
        }
        if (!escaped && c == '\\') {
            escaped = true;
            advance(input);
            c = peek(input);
            continue;
        }
        escaped = false;
        dynamic_append(&word_value, c);
        advance(input);
        c = peek(input);
    }

    if (c == quote_char) {
        advance(input); // consume closing quote
    } else {
        dynamic_prepend(&word_value, quote_char);
    }

    return word_value;
}

/* Skip whitespace characters in the input */
static void skip_whitespaces(LexerInput *input) {
    char c = peek(input);
    while (c == ' ' || c == '\t' || c == '\r') {
        advance(input);
        c = peek(input);
    }
}

LexerToken lexer_next_token(LexerInput *input) {
    // Skip whitespace
    skip_whitespaces(input);
    char c = peek(input);

    LexerToken token;
    token.value = NULL;
    token.extra = NULL;

    if (is_at_end(input)) {
        token.type = TOKEN_EOF;
        return token;
    }

    if (c == '\n') {
        advance(input); // consume newline
        token.type = TOKEN_EOL;
        return token;
    }

    if (c == '\\') {
        // Handle escaped characters
        token.type = TOKEN_WORD;
        advance(input); // consume backslash

        Dynamic word_value = read_single_word(input);
        token.value        = dynamic_to_string(&word_value);
        free_dynamic(&word_value);

        return token;
    }

    switch (c) {
        case '|':
            advance(input);
            if (peek(input) == '|') {
                advance(input);
                token.type = TOKEN_OR;
            } else {
                token.type = TOKEN_PIPE;
            }
            break;
        case '&':
            advance(input);
            if (peek(input) == '&') {
                advance(input);
                token.type = TOKEN_SEQUENCE;
            } else {
                token.type = TOKEN_BACKGROUND;
            }
            break;
        case ';':
            advance(input);
            token.type = TOKEN_SEMICOLON;
            break;
        case '<':
            advance(input);
            char next_char = peek(input);
            if (next_char == '<') {
                advance(input);
                if (peek(input) == '<') { // <<< (here-string)
                    advance(input);
                    token.type = TOKEN_HERESTRING;

                    skip_whitespaces(input);

                    // consume the word after <<<
                    Dynamic word_value = {0};
                    char    next       = peek(input);
                    if (next == '"' || next == '\'') {
                        word_value = read_quoted_word(input);
                    } else {
                        word_value = read_single_word(input);
                    }
                    token.value = dynamic_to_string(&word_value);
                    free_dynamic(&word_value);
                } else { // << (here-doc)
                    bool ident_ignore = false;
                    if (peek(input) == '-') { // <<- (here-doc with tab removal)
                        ident_ignore = true;
                        advance(input);
                    }

                    token.type = TOKEN_HEREDOC;
                    skip_whitespaces(input);

                    // consume the word after <<
                    Dynamic word_value = {0};
                    char    next       = peek(input);
                    if (next == '"' || next == '\'') {
                        word_value = read_quoted_word(input);
                    } else {
                        word_value = read_single_word(input);
                    }
                    char  *end_marker     = dynamic_to_string(&word_value);
                    size_t end_marker_len = word_value.length;
                    free_dynamic(&word_value);

                    Dynamic content_value = {0};
                    init_dynamic(&content_value);

                    // Skip to the next line
                    char curr = peek(input);
                    while (!is_at_end(input) && curr != '\n') {
                        advance(input);
                        curr = peek(input);
                    }
                    if (curr == '\n') {
                        advance(input);
                    }

                    while (!is_at_end(input) &&
                           strncmp(input->data + input->pos, end_marker,
                                   end_marker_len) != 0) {
                        char c = advance(input);
                        dynamic_append(&content_value, c);

                        if (c == '\n') {
                            // If ident_ignore is set, remove leading whitespace
                            if (ident_ignore) {
                                skip_whitespaces(input);
                            }
                        }
                    }

                    // Consume the end marker
                    for (size_t i = 0; i < end_marker_len && !is_at_end(input);
                         i++) {
                        advance(input);
                    }

                    free(end_marker);
                    token.value = dynamic_to_string(&content_value);
                    free_dynamic(&content_value);
                }
            } else if (next_char == '&') {
                // Handle fd duplication <&
                advance(input);
                token.type = TOKEN_FD_DUPLICATION;
            } else if (next_char == '(') {
                // Handle process substitution <(
                token.type  = TOKEN_PROCESS_SUBSTITUTION_IN;
                token.value = command_substitution(input);
            } else {
                token.type = TOKEN_REDIRECT_IN;
            }
            break;
        case '>': {
            advance(input);
            char next_char = peek(input);
            if (next_char == '>') {
                advance(input);
                token.type = TOKEN_REDIRECT_APPEND;
            } else if (next_char == '&') {
                advance(input);
                token.type = TOKEN_REDIRECT_OUT_ERR;
            } else if (next_char == '(') {
                // Handle process substitution >(
                token.type  = TOKEN_PROCESS_SUBSTITUTION_OUT;
                token.value = command_substitution(input);
            } else {
                token.type = TOKEN_REDIRECT_OUT;
            }
            break;
        }
        case '(':
            advance(input);
            token.type = TOKEN_LPAREN;
            break;
        case ')':
            advance(input);
            token.type = TOKEN_RPAREN;
            break;
        case '"':
        case '\'': {
            Dynamic word_value = read_quoted_word(input);
            token.type         = TOKEN_WORD;
            token.value        = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
        } break;
        case '#': {
            // Comment: consume until end of line
            advance(input); // consume '#'
            Dynamic word_value = {0};
            init_dynamic(&word_value);

            char c = peek(input);
            while (!is_at_end(input) && c != '\n') {
                dynamic_append(&word_value, c);
                advance(input);
                c = peek(input);
            }

            token.type  = TOKEN_COMMENT;
            token.value = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
        } break;

        default:
            // Handle words, IO numbers and assignments
            token.type           = TOKEN_WORD;
            bool    is_io_number = true;
            bool    escaped      = false;
            Dynamic word_value   = {0};
            init_dynamic(&word_value);

            while (!is_at_end(input) && c != ' ' && c != '\t' && c != '\n' &&
                   c != '\r' && c != '|' && c != '&' && c != ';' && c != '(' &&
                   c != ')' && c != '#') {
                if (is_io_number && (c < '0' || c > '9')) {
                    if (c == '>' || c == '<') {
                        token.type = TOKEN_IO_NUMBER;
                        break;
                    }
                    is_io_number = false;
                }

                if (!is_io_number && (c == '>' || c == '<')) {
                    break;
                }

                if (!escaped && c == '$' && peek_next(input) == '(') {
                    advance(input); // consume '$'
                    char *command = command_substitution(input);
                    char *substitution =
                        input->execute(command, input->session);
                    free(command);
                    dynamic_extend(&word_value, substitution);
                    free(substitution);
                    c = peek(input);
                    continue;
                }

                if (!escaped && c == '\\') {
                    escaped = true;
                    advance(input);
                    c = peek(input);
                    continue;
                }

                if (!escaped && !is_io_number && c == '=' &&
                    word_value.length > 0) {
                    // Assignment detected
                    advance(input);
                    c          = peek(input);
                    token.type = TOKEN_ASSIGNMENT;
                    if (c == '"' || c == '\'') {
                        Dynamic quoted_value = read_quoted_word(input);
                        token.extra          = dynamic_to_string(&quoted_value);
                        free_dynamic(&quoted_value);
                    } else {
                        Dynamic unquoted_value = read_single_word(input);
                        token.extra = dynamic_to_string(&unquoted_value);
                        free_dynamic(&unquoted_value);
                    }
                    break;
                }

                escaped = false;

                dynamic_append(&word_value, c);
                advance(input);
                c = peek(input);
            }

            token.value = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
            break;
    }
    return token;
}
