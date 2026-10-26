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

/* Remove line continuations (<backslash><newline>) from input data */
static char *remove_line_continuations(const char *input) {
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

#if !defined(TIDESH_DISABLE_COMMAND_SUBSTITUTION) ||                           \
    !defined(TIDESH_DISABLE_EXPANSIONS)
/* Handle command substitution $(...) or <(...) or >(...) */
static char *command_substitution(LexerInput *input) {
    char    c               = advance(input); // Consume `(`
    size_t  depth           = 1;
    bool    escaped         = false;
    bool    in_single_quote = false;
    bool    in_double_quote = false;
    Dynamic command         = {0};
    init_dynamic(&command);

    while (depth > 0 && !is_at_end(input)) {
        c = advance(input);
        if (escaped) {
            escaped = false;
            dynamic_append(&command, c);
            continue;
        }

        if (c == '\\') {
            if (in_single_quote) {
                dynamic_append(&command, '\\');
            } else {
                escaped = true;
                dynamic_append(&command, '\\');
            }
            continue;
        }

        if (in_single_quote) {
            if (c == '\'') {
                in_single_quote = false;
            }
            dynamic_append(&command, c);
            continue;
        }

        if (c == '\'') {
            if (!in_double_quote) {
                in_single_quote = true;
            }
            dynamic_append(&command, c);
            continue;
        }

        if (c == '"') {
            in_double_quote = !in_double_quote;
            dynamic_append(&command, c);
            continue;
        }

        if (!in_double_quote && c == '(') {
            depth++;
            dynamic_append(&command, c);
            continue;
        }

        if (!in_double_quote && c == ')') {
            depth--;
            if (depth == 0) {
                break;
            }
            dynamic_append(&command, c);
            continue;
        }

        dynamic_append(&command, c);
    }

    char *result_command = dynamic_to_string(&command);
    free_dynamic(&command);
    return result_command;
}
#endif /* !TIDESH_DISABLE_COMMAND_SUBSTITUTION || !TIDESH_DISABLE_EXPANSIONS   \
        */

#ifndef TIDESH_DISABLE_EXPANSIONS
/* Handle parameter expansion body ${...} */
static char *parameter_expansion_body(LexerInput *input) {
    char    c               = advance(input); // Consume '{'
    size_t  depth           = 1;
    bool    escaped         = false;
    bool    in_single_quote = false;
    bool    in_double_quote = false;
    Dynamic body            = {0};
    init_dynamic(&body);

    while (depth > 0 && !is_at_end(input)) {
        c = advance(input);
        if (escaped) {
            escaped = false;
            dynamic_append(&body, c);
            continue;
        }

        if (c == '\\') {
            if (in_single_quote) {
                dynamic_append(&body, '\\');
            } else {
                escaped = true;
                dynamic_append(&body, '\\');
            }
            continue;
        }

        if (in_single_quote) {
            if (c == '\'') {
                in_single_quote = false;
            }
            dynamic_append(&body, c);
            continue;
        }

        if (c == '\'') {
            if (!in_double_quote) {
                in_single_quote = true;
            }
            dynamic_append(&body, c);
            continue;
        }

        if (c == '"') {
            in_double_quote = !in_double_quote;
            dynamic_append(&body, c);
            continue;
        }

        if (c == '$' && peek(input) == '(') {
#if !defined(TIDESH_DISABLE_COMMAND_SUBSTITUTION) ||                           \
    !defined(TIDESH_DISABLE_EXPANSIONS)
            char *sub = command_substitution(input);
            dynamic_extend(&body, "$(");
            if (sub) {
                dynamic_extend(&body, sub);
                free(sub);
            }
            dynamic_append(&body, ')');
#else
            dynamic_append(&body, c);
#endif
            continue;
        }

        if (c == '$' && peek(input) == '{') {
            char *sub = parameter_expansion_body(input);
            dynamic_extend(&body, "${");
            if (sub) {
                dynamic_extend(&body, sub);
                free(sub);
            }
            dynamic_append(&body, '}');
            continue;
        }

        if (!in_double_quote && c == '{') {
            depth++;
            dynamic_append(&body, c);
            continue;
        }

        if (!in_double_quote && c == '}') {
            depth--;
            if (depth == 0) {
                break;
            }
            dynamic_append(&body, c);
            continue;
        }

        dynamic_append(&body, c);
    }

    char *result_body = dynamic_to_string(&body);
    free_dynamic(&body);
    return result_body;
}
#endif /* !TIDESH_DISABLE_EXPANSIONS */

static Dynamic read_quoted_word(LexerInput *input);

/* Read a single unquoted word from the input */
static Dynamic read_single_word(LexerInput *input) {
    Dynamic word_value = {0};
    init_dynamic(&word_value);
    char c = peek(input);

    bool escaped     = false;
    int  brace_depth = 0;
    while (!is_at_end(input) && c != ' ' && c != '\t' && c != '\n' &&
           c != '\r') {
        if (!escaped &&
            (c == ';' || c == '&' || c == '|' || c == '<' || c == '>' ||
             c == '(' || c == ')' || (brace_depth == 0 && c == '}'))) {
            break;
        }
        if (!escaped && (c == '"' || c == '\'')) {
            Dynamic quoted_value = read_quoted_word(input);
            if (quoted_value.value) {
                dynamic_extend(&word_value, quoted_value.value);
            }
            free_dynamic(&quoted_value);
            c = peek(input);
            continue;
        }
        if (c == '{') {
            brace_depth++;
        } else if (c == '}' && brace_depth > 0) {
            brace_depth--;
        }
        if (!escaped && c == '\\' && peek_next(input) == '$') {
            advance(input); // consume '\\'
            dynamic_append(&word_value, '\\');
            advance(input); // consume '$'
            dynamic_append(&word_value, '$');
            c = peek(input);
            continue;
        }
        if (!escaped && c == '$' && peek_next(input) == '(') {
#if !defined(TIDESH_DISABLE_COMMAND_SUBSTITUTION) ||                           \
    !defined(TIDESH_DISABLE_EXPANSIONS)
            advance(input); // consume '$'
            char *command = command_substitution(input);
            dynamic_extend(&word_value, "$(");
            dynamic_extend(&word_value, command);
            dynamic_append(&word_value, ')');
            free(command);
#else
            dynamic_append(&word_value, c);
            advance(input);
#endif
            c = peek(input);
            continue;
        }
        if (!escaped && c == '$' && peek_next(input) == '{') {
#ifndef TIDESH_DISABLE_EXPANSIONS
            advance(input); // consume '$'
            char *param = parameter_expansion_body(input);
            dynamic_extend(&word_value, "${");
            if (param) {
                dynamic_extend(&word_value, param);
                free(param);
            }
            dynamic_append(&word_value, '}');
#else
            dynamic_append(&word_value, c);
            advance(input);
#endif
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

    while (!is_at_end(input) && (escaped || c != quote_char)) {
        if (quote_char == '\'') {
            dynamic_append(&word_value, c);
            advance(input);
            c = peek(input);
            continue;
        }
        if (!escaped && c == '\\' && peek_next(input) == '$') {
            advance(input); // consume '\\'
            dynamic_append(&word_value, '\\');
            advance(input); // consume '$'
            dynamic_append(&word_value, '$');
            c = peek(input);
            continue;
        }
        if (!escaped && c == '$' && peek_next(input) == '(') {
#if !defined(TIDESH_DISABLE_COMMAND_SUBSTITUTION) ||                           \
    !defined(TIDESH_DISABLE_EXPANSIONS)
            advance(input); // consume '$'
            char *command = command_substitution(input);
            dynamic_extend(&word_value, "$(");
            dynamic_extend(&word_value, command);
            dynamic_append(&word_value, ')');
            free(command);
#else
            dynamic_append(&word_value, c);
            advance(input);
#endif
            c = peek(input);
            continue;
        }
        if (!escaped && c == '$' && peek_next(input) == '{') {
#ifndef TIDESH_DISABLE_EXPANSIONS
            advance(input); // consume '$'
            char *param = parameter_expansion_body(input);
            dynamic_extend(&word_value, "${");
            if (param) {
                dynamic_extend(&word_value, param);
                free(param);
            }
            dynamic_append(&word_value, '}');
#else
            dynamic_append(&word_value, c);
            advance(input);
#endif
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

    if (c == quote_char && !escaped) {
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

#ifndef TIDESH_DISABLE_CONDITIONALS
/* Check if a word is a conditional keyword */
static TokenType check_conditional_keyword(const char *word) {
    if (strcmp(word, "if") == 0) {
        return TOKEN_IF;
    } else if (strcmp(word, "then") == 0) {
        return TOKEN_THEN;
    } else if (strcmp(word, "else") == 0) {
        return TOKEN_ELSE;
    } else if (strcmp(word, "elif") == 0) {
        return TOKEN_ELIF;
    } else if (strcmp(word, "fi") == 0) {
        return TOKEN_FI;
    } else if (strcmp(word, "case") == 0) {
        return TOKEN_CASE;
    } else if (strcmp(word, "esac") == 0) {
        return TOKEN_ESAC;
    } else if (strcmp(word, "in") == 0) {
        return TOKEN_IN;
    }
    return TOKEN_WORD;
}
#endif

#ifndef TIDESH_DISABLE_LOOPS
/* Check if a word is a loop keyword */
static TokenType check_loop_keyword(const char *word) {
    if (strcmp(word, "for") == 0) {
        return TOKEN_FOR;
    } else if (strcmp(word, "while") == 0) {
        return TOKEN_WHILE;
    } else if (strcmp(word, "until") == 0) {
        return TOKEN_UNTIL;
    } else if (strcmp(word, "in") == 0) {
        return TOKEN_IN;
    } else if (strcmp(word, "do") == 0) {
        return TOKEN_DO;
    } else if (strcmp(word, "done") == 0) {
        return TOKEN_DONE;
    }
    return TOKEN_WORD;
}
#endif

static LexerToken read_word_token(LexerInput *input) {
    LexerToken token;
    token.type  = TOKEN_WORD;
    token.value = NULL;
    token.extra = NULL;

    bool    is_io_number = true;
    bool    has_quotes   = false;
    bool    escaped      = false;
    int     brace_depth  = 0;
    Dynamic word_value   = {0};
    init_dynamic(&word_value);
    char c = peek(input);

    while (!is_at_end(input) && c != ' ' && c != '\t' && c != '\n' &&
           c != '\r' && c != '|' && c != '&' && c != ';' && c != '(' &&
           c != ')' && (brace_depth > 0 || c != '}')) {
        if (!escaped && (c == '"' || c == '\'')) {
            has_quotes           = true;
            is_io_number         = false;
            Dynamic quoted_value = read_quoted_word(input);
            if (quoted_value.value) {
                dynamic_extend(&word_value, quoted_value.value);
            }
            free_dynamic(&quoted_value);
            c = peek(input);
            continue;
        }

        if (!escaped && c == '{') {
            brace_depth++;
        } else if (!escaped && c == '}' && brace_depth > 0) {
            brace_depth--;
        }
#ifndef TIDESH_DISABLE_REDIRECTIONS
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
#endif

        if (!escaped && c == '$' && peek_next(input) == '(') {
#if !defined(TIDESH_DISABLE_COMMAND_SUBSTITUTION) ||                           \
    !defined(TIDESH_DISABLE_EXPANSIONS)
            advance(input); // consume '$'
            char *command = command_substitution(input);
            dynamic_extend(&word_value, "$(");
            dynamic_extend(&word_value, command);
            dynamic_append(&word_value, ')');
            free(command);
#else
            dynamic_append(&word_value, c);
            advance(input);
#endif
            c = peek(input);
            continue;
        }

        if (!escaped && c == '$' && peek_next(input) == '{') {
#ifndef TIDESH_DISABLE_EXPANSIONS
            advance(input); // consume '$'
            char *param = parameter_expansion_body(input);
            dynamic_extend(&word_value, "${");
            if (param) {
                dynamic_extend(&word_value, param);
                free(param);
            }
            dynamic_append(&word_value, '}');
#else
            dynamic_append(&word_value, c);
            advance(input);
#endif
            c = peek(input);
            continue;
        }

        if (!escaped && c == '\\') {
            escaped = true;
            advance(input);
            c = peek(input);
            continue;
        }

        if (!escaped && !has_quotes && !is_io_number && c == '=' &&
            word_value.length > 0) {
#ifndef TIDESH_DISABLE_ASSIGNMENTS
            // Assignment detected
            advance(input);
            c                      = peek(input);
            token.type             = TOKEN_ASSIGNMENT;
            Dynamic unquoted_value = read_single_word(input);
            token.extra            = dynamic_to_string(&unquoted_value);
            free_dynamic(&unquoted_value);
            break;
#else
            // Assignments disabled, treat = as part of the word
            dynamic_append(&word_value, c);
            advance(input);
#endif
        }

        escaped = false;

        dynamic_append(&word_value, c);
        advance(input);
        c = peek(input);
    }

    token.value = dynamic_to_string(&word_value);
    free_dynamic(&word_value);
#ifndef TIDESH_DISABLE_CONDITIONALS
    // Check if the word is a conditional keyword
    if (!has_quotes && token.type == TOKEN_WORD) {
        token.type = check_conditional_keyword(token.value);
    }
#endif
#ifndef TIDESH_DISABLE_LOOPS
    // Check if the word is a loop keyword
    if (!has_quotes && token.type == TOKEN_WORD) {
        token.type = check_loop_keyword(token.value);
    }
#endif
    return token;
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
        case ';':
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
#ifndef TIDESH_DISABLE_SEQUENCES
            // Check if sequences feature is enabled
            if (!input->session || input->session->features.sequences) {
                advance(input);
                token.type = TOKEN_SEMICOLON;
            } else {
                // Sequences disabled, treat ; as a normal word
                Dynamic word_value = read_single_word(input);
                token.type         = TOKEN_WORD;
                token.value        = dynamic_to_string(&word_value);
                free_dynamic(&word_value);
            }
#else
            // Sequences disabled, treat ; as a normal word
            Dynamic word_value = read_single_word(input);
            token.type         = TOKEN_WORD;
            token.value        = dynamic_to_string(&word_value);
            free_dynamic(&word_value);
#endif
            break;
        case '<':
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
                    // Handle fd duplication <&
                    advance(input);
                    token.type = TOKEN_FD_DUPLICATION;
                } else if (next_char == '(') {
                    // Handle process substitution <(
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
                    if (input->session &&
                        !input->session->features.command_substitution) {
                        // Command substitution disabled, treat <( as word
                        Dynamic word_value = read_single_word(input);
                        token.type         = TOKEN_WORD;
                        token.value        = dynamic_to_string(&word_value);
                        free_dynamic(&word_value);
                    } else {
                        token.type  = TOKEN_PROCESS_SUBSTITUTION_IN;
                        token.value = command_substitution(input);
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
                    // Handle process substitution >(
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
                    if (input->session &&
                        !input->session->features.command_substitution) {
                        // Command substitution disabled, treat >( as word
                        Dynamic word_value = read_single_word(input);
                        token.type         = TOKEN_WORD;
                        token.value        = dynamic_to_string(&word_value);
                        free_dynamic(&word_value);
                    } else {
                        token.type  = TOKEN_PROCESS_SUBSTITUTION_OUT;
                        token.value = command_substitution(input);
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
                    next == '&' || next == '|') {
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
#ifndef TIDESH_DISABLE_ASSIGNMENTS
        case TOKEN_ASSIGNMENT:
            return "TOKEN_ASSIGNMENT";
#endif
#ifndef TIDESH_DISABLE_PIPES
        case TOKEN_PIPE:
            return "TOKEN_PIPE";
        case TOKEN_OR:
            return "TOKEN_OR";
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
#endif
#if !defined(TIDESH_DISABLE_LOOPS) || !defined(TIDESH_DISABLE_CONDITIONALS)
        case TOKEN_IN:
            return "TOKEN_IN";
#endif
#ifndef TIDESH_DISABLE_LOOPS
        case TOKEN_DO:
            return "TOKEN_DO";
        case TOKEN_DONE:
            return "TOKEN_DONE";
#endif
        case TOKEN_EOL:
            return "TOKEN_EOL";
        case TOKEN_EOF:
            return "TOKEN_EOF";
        default:
            return "TOKEN_UNKNOWN";
    }
}
