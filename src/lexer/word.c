#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* size_t, NULL */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* strlen, strncmp, strcmp */

#include "data/dynamic.h"
#include "lexer/input.h"
#include "lexer/token.h"
#include "lexer/word.h"
#include "session.h"

#if !defined(TIDESH_DISABLE_COMMAND_SUBSTITUTION) ||                           \
    !defined(TIDESH_DISABLE_EXPANSIONS)
/* Handle command substitution $(...) or <(...) or >(...) */
char *command_substitution(LexerInput *input) {
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
char *parameter_expansion_body(LexerInput *input) {
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

/* Read a single unquoted word from the input */
Dynamic read_single_word(LexerInput *input) {
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
Dynamic read_quoted_word(LexerInput *input) {
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

#ifndef TIDESH_DISABLE_CONDITIONALS
/* Check if a word is a conditional keyword */
TokenType check_conditional_keyword(const char *word) {
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
TokenType check_loop_keyword(const char *word) {
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

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
/* Check if a word is a context manager keyword */
TokenType check_context_keyword(const char *word) {
    if (strcmp(word, "with") == 0) {
        return TOKEN_WITH;
    }
    return TOKEN_WORD;
}
#endif

LexerToken read_word_token(LexerInput *input) {
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
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    // Check if the word is a context manager keyword
    if (!has_quotes && token.type == TOKEN_WORD) {
        token.type = check_context_keyword(token.value);
    }
#endif
    return token;
}
