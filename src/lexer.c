#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* size_t, NULL */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* strlen, strncmp, strcmp, strdup */

#include "data/dynamic.h"
#include "environ.h"
#include "lexer.h"
#include "session.h"

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
#ifndef TIDESH_DISABLE_PIPES
        case '|':
            // Check if pipes feature is enabled
            if (!input->session || input->session->features.pipes) {
                advance(input);
                if (peek(input) == '|') {
                    advance(input);
                    token.type = TOKEN_OR;
                } else {
                    token.type = TOKEN_PIPE;
                }
            } else {
                // Pipes disabled, treat | as a normal word
                Dynamic word_value = read_single_word(input);
                token.type         = TOKEN_WORD;
                token.value        = dynamic_to_string(&word_value);
                free_dynamic(&word_value);
            }
            break;
#endif
        case '&':
            // Check if sequences or job_control feature is enabled
            if (!input->session) {
                advance(input);
                if (peek(input) == '&') {
#ifndef TIDESH_DISABLE_SEQUENCES
                    advance(input);
                    token.type = TOKEN_SEQUENCE;
#else
                    token.type  = TOKEN_WORD;
                    token.value = strdup("&");
#endif
                } else if (peek(input) == '>') {
#ifndef TIDESH_DISABLE_REDIRECTIONS
                    advance(input);
                    if (peek(input) == '>') {
                        advance(input);
                        token.type = TOKEN_REDIRECT_APPEND_ERR;
                    } else {
                        token.type = TOKEN_REDIRECT_ALL;
                    }
#else
                    token.type  = TOKEN_WORD;
                    token.value = strdup("&");
#endif
                } else {
#ifndef TIDESH_DISABLE_JOB_CONTROL
                    token.type = TOKEN_BACKGROUND;
#else
                    token.type  = TOKEN_WORD;
                    token.value = strdup("&");
#endif
                }
            } else {
                advance(input);
                if (peek(input) == '&') {
                    // && operator controlled by sequences
#ifndef TIDESH_DISABLE_SEQUENCES
                    if (input->session->features.sequences) {
                        advance(input);
                        token.type = TOKEN_SEQUENCE;
                    } else {
                        // Back off the second &, treat first & as word
                        token.type  = TOKEN_WORD;
                        token.value = strdup("&");
                    }
#else
                    // Sequences disabled, treat & as a normal word
                    token.type  = TOKEN_WORD;
                    token.value = strdup("&");
#endif
                } else if (peek(input) == '>') {
#ifndef TIDESH_DISABLE_REDIRECTIONS
                    if (input->session->features.redirections) {
                        advance(input);
                        if (peek(input) == '>') {
                            advance(input);
                            token.type = TOKEN_REDIRECT_APPEND_ERR;
                        } else {
                            token.type = TOKEN_REDIRECT_ALL;
                        }
                    } else {
                        token.type  = TOKEN_WORD;
                        token.value = strdup("&");
                    }
#else
                    token.type  = TOKEN_WORD;
                    token.value = strdup("&");
#endif
                } else {
                    // Single & operator controlled by job_control or background
#ifndef TIDESH_DISABLE_JOB_CONTROL
                    if (input->session->features.job_control) {
                        token.type = TOKEN_BACKGROUND;
                    } else {
                        // Job control disabled, treat & as a normal word
                        token.type  = TOKEN_WORD;
                        token.value = strdup("&");
                    }
#else
                    // Job control disabled, treat & as a normal word
                    token.type  = TOKEN_WORD;
                    token.value = strdup("&");
#endif
                }
            }
            break;
#ifndef TIDESH_DISABLE_SEQUENCES
        case ';':
            // Check if sequences feature is enabled
            if (!input->session || input->session->features.sequences) {
#ifndef TIDESH_DISABLE_CONDITIONALS
                if (peek_next(input) == ';') {
                    advance(input); // consume first ';'
                    advance(input); // consume second ';'
                    if (peek(input) == '&') {
                        advance(input); // consume '&'
                        token.type = TOKEN_CASE_CONTINUE;
                    } else {
                        token.type = TOKEN_DOUBLE_SEMICOLON;
                    }
                    break;
                } else if (peek_next(input) == '&') {
                    advance(input); // consume ';'
                    advance(input); // consume '&'
                    token.type = TOKEN_CASE_FALLTHROUGH;
                    break;
                }
#endif
                advance(input);
                token.type = TOKEN_SEMICOLON;
            } else {
                // Sequences disabled, treat ; as a normal word
                Dynamic word_value = read_single_word(input);
                token.type         = TOKEN_WORD;
                token.value        = dynamic_to_string(&word_value);
                free_dynamic(&word_value);
            }
            break;
#endif
        case '<': {
#ifndef TIDESH_DISABLE_REDIRECTIONS
            // Check if redirections feature is enabled
            if (!input->session || input->session->features.redirections) {
                advance(input);
                char next_char = peek(input);
                if (next_char == '<') {
                    advance(input);
                    if (peek(input) == '<') { // <<< (here-string)
                        advance(input);
                        token.type = TOKEN_HERESTRING;

                        skip_whitespaces(input);

                        // consume the word after <<<
                        Dynamic word_value = read_single_word(input);
                        token.value        = dynamic_to_string(&word_value);
                        free_dynamic(&word_value);
                    } else { // << (here-doc)
                        bool ident_ignore = false;
                        if (peek(input) ==
                            '-') { // <<- (here-doc with tab removal)
                            ident_ignore = true;
                            advance(input);
                        }

                        token.type = TOKEN_HEREDOC;
                        skip_whitespaces(input);

                        // consume the word after <<
                        Dynamic word_value     = read_single_word(input);
                        char   *end_marker     = dynamic_to_string(&word_value);
                        size_t  end_marker_len = word_value.length;
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
                                // If ident_ignore is set, remove leading
                                // whitespace
                                if (ident_ignore) {
                                    skip_whitespaces(input);
                                }
                            }
                        }

                        // Consume the end marker
                        for (size_t i = 0;
                             i < end_marker_len && !is_at_end(input); i++) {
                            advance(input);
                        }

                        free(end_marker);
                        token.value = dynamic_to_string(&content_value);
                        free_dynamic(&content_value);
                    }
                } else if (next_char == '&') {
                    advance(input);
                    token.type = TOKEN_FD_DUPLICATION;
                } else if (next_char == '(') {
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
                    // Check if command substitution feature is enabled
                    if (!input->session ||
                        input->session->features.command_substitution) {
                        token.type  = TOKEN_PROCESS_SUBSTITUTION_IN;
                        token.value = command_substitution(input);
                    } else {
                        // Command substitution disabled, treat <( as word
                        Dynamic word_value = read_single_word(input);
                        token.type         = TOKEN_WORD;
                        token.value        = dynamic_to_string(&word_value);
                        free_dynamic(&word_value);
                    }
#else
                    // Command substitution disabled, treat <( as word
                    Dynamic word_value = read_single_word(input);
                    token.type         = TOKEN_WORD;
                    token.value        = dynamic_to_string(&word_value);
                    free_dynamic(&word_value);
#endif
                } else {
                    token.type = TOKEN_REDIRECT_IN;
                }
            } else {
                // Redirections disabled, treat < as a normal word
                Dynamic word_value = read_single_word(input);
                token.type         = TOKEN_WORD;
                token.value        = dynamic_to_string(&word_value);
                free_dynamic(&word_value);
            }
#else
            // Redirections disabled, treat < as a normal word
            Dynamic word_value = read_single_word(input);
            token.type         = TOKEN_WORD;
            token.value        = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
#endif
            break;
        }
        case '>': {
#ifndef TIDESH_DISABLE_REDIRECTIONS
            // Check if redirections feature is enabled
            if (!input->session || input->session->features.redirections) {
                advance(input);
                char next_char = peek(input);
                if (next_char == '>') {
                    advance(input);
                    if (peek(input) == '&') {
                        advance(input);
                        token.type = TOKEN_REDIRECT_APPEND_ERR;
                    } else {
                        token.type = TOKEN_REDIRECT_APPEND;
                    }
                } else if (next_char == '&') {
                    advance(input);
                    token.type = TOKEN_REDIRECT_OUT_ERR;
                } else if (next_char == '|') {
                    advance(input);
                    token.type = TOKEN_REDIRECT_OUT;
                } else if (next_char == '(') {
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
                    // Check if command substitution feature is enabled
                    if (!input->session ||
                        input->session->features.command_substitution) {
                        token.type  = TOKEN_PROCESS_SUBSTITUTION_OUT;
                        token.value = command_substitution(input);
                    } else {
                        // Command substitution disabled, treat >( as word
                        Dynamic word_value = read_single_word(input);
                        token.type         = TOKEN_WORD;
                        token.value        = dynamic_to_string(&word_value);
                        free_dynamic(&word_value);
                    }
#else
                    // Command substitution disabled, treat >( as word
                    Dynamic word_value = read_single_word(input);
                    token.type         = TOKEN_WORD;
                    token.value        = dynamic_to_string(&word_value);
                    free_dynamic(&word_value);
#endif
                } else {
                    token.type = TOKEN_REDIRECT_OUT;
                }
            } else {
                // Redirections disabled, treat > as a normal word
                Dynamic word_value = read_single_word(input);
                token.type         = TOKEN_WORD;
                token.value        = dynamic_to_string(&word_value);
                free_dynamic(&word_value);
            }
#else
            // Redirections disabled, treat > as a normal word
            Dynamic word_value = read_single_word(input);
            token.type         = TOKEN_WORD;
            token.value        = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
#endif
            break;
        }
        case '(':
#ifndef TIDESH_DISABLE_SUBSHELLS
            // Check if subshells feature is enabled
            if (!input->session || input->session->features.subshells) {
                advance(input);
                token.type = TOKEN_LPAREN;
            } else {
                // Subshells disabled, treat ( as a normal word
                Dynamic word_value = read_single_word(input);
                token.type         = TOKEN_WORD;
                token.value        = dynamic_to_string(&word_value);
                free_dynamic(&word_value);
            }
#else
            // Subshells disabled, treat ( as a normal word
            Dynamic word_value = read_single_word(input);
            token.type         = TOKEN_WORD;
            token.value        = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
#endif
            break;
        case '{':
#ifndef TIDESH_DISABLE_BLOCKS
            // Check if blocks feature is enabled and followed by whitespace or
            // delimiter
            if (!input->session || input->session->features.blocks) {
                char next = peek_next(input);
                if (next == ' ' || next == '\t' || next == '\n' ||
                    next == '\r' || next == '\0' || next == ';') {
                    advance(input);
                    token.type = TOKEN_LBRACE;
                    break;
                }
            }
#endif
            return read_word_token(input);
        case '}':
#ifndef TIDESH_DISABLE_BLOCKS
            // Check if blocks feature is enabled and followed by whitespace or
            // delimiter
            if (!input->session || input->session->features.blocks) {
                char next = peek_next(input);
                if (next == ' ' || next == '\t' || next == '\n' ||
                    next == '\r' || next == '\0' || next == ';' ||
                    next == '&' || next == '|' ||
                    strncmp(input->data + input->pos, "}else", 5) == 0 ||
                    strncmp(input->data + input->pos, "}elif", 5) == 0) {
                    advance(input);
                    token.type = TOKEN_RBRACE;
                    break;
                }
            }
#endif
            return read_word_token(input);
        case ')':
#ifndef TIDESH_DISABLE_SUBSHELLS
            // Check if subshells feature is enabled
            if (!input->session || input->session->features.subshells) {
                advance(input);
                token.type = TOKEN_RPAREN;
            } else {
                // Subshells disabled, treat ) as a normal word
                Dynamic word_value = read_single_word(input);
                token.type         = TOKEN_WORD;
                token.value        = dynamic_to_string(&word_value);
                free_dynamic(&word_value);
            }
#else
            // Subshells disabled, treat ) as a normal word
            Dynamic word_value = read_single_word(input);
            token.type         = TOKEN_WORD;
            token.value        = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
#endif
            break;
        case '"':
        case '\'':
        default:
            return read_word_token(input);
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
    }
    return token;
}
