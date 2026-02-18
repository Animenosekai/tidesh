/* Variables expansion implementation */

#include <ctype.h>   /* isspace, isalnum, isdigit */
#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* fprintf, snprintf, stderr */
#include <stdlib.h>  /* malloc, free, strtol */
#include <string.h>  /* strdup, strndup, strlen, strncpy, strstr, strcmp */

#include "data/array.h" /* Array, init_array, array_add, array_extend, free_array */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, free_dynamic, dynamic_to_string */
#include "environ.h"      /* Environ, environ_get, environ_set */
#include "expansions/variables.h" /* variable_expansion */
#include "session.h"              /* Session */

/* Parse a variable name starting at input[*pos], advance *pos */
static char *parse_varname(char *input, size_t *pos) {
    size_t start = *pos;

    // Check if first character is a special variable or a digit
    if (input[*pos] == '?' || input[*pos] == '$' || input[*pos] == '!' ||
        input[*pos] == '_' || isdigit(input[*pos])) {
        (*pos)++;
    } else {
        while (input[*pos] && (isalnum(input[*pos]) || input[*pos] == '_')) {
            (*pos)++;
        }
    }

    size_t len     = *pos - start;
    char  *varname = malloc(len + 1);
    strncpy(varname, input + start, len);
    varname[len] = '\0';
    return varname;
}

/* Find the closing brace for ${...}, handling nested braces */
static int find_closing_brace(char *input, size_t start) {
    int    depth = 1;
    size_t i     = start;

    while (input[i] && depth > 0) {
        if (input[i] == '{')
            depth++;
        else if (input[i] == '}')
            depth--;
        i++;
    }

    return (depth == 0) ? (int)i - 1 : -1;
}

/* Split a string by whitespace */
static Array *split_by_whitespace(char *str) {
    Array *result = init_array(NULL);

    if (!str || !*str)
        return result;

    size_t  len   = strlen(str);
    Dynamic token = {0};
    init_dynamic(&token);

    for (size_t i = 0; i <= len; i++) {
        if (i == len || isspace(str[i])) {
            if (token.length > 0) {
                char *token_str = dynamic_to_string(&token);
                array_add(result, token_str);
                free(token_str);
                init_dynamic(&token);
            }
        } else {
            dynamic_append(&token, str[i]);
        }
    }

    free_dynamic(&token);
    return result;
}

/* Expand ${VAR} with modifiers - returns NULL for split marker */
static char *expand_brace_var(char *expr, Session *session, bool *is_split) {
    *is_split = false;

    // Handle ${#VAR} - length
    if (expr[0] == '#') {
        char *varname = strdup(expr + 1);
        char *value   = environ_get(session->environ, varname);
        free(varname);

        if (value) {
            char *result = malloc(32);
            snprintf(result, 32, "%zu", strlen(value));
            return result;
        }
        return strdup("0");
    }

    // Handle ${=VAR...} - split
    if (expr[0] == '=') {
        *is_split = true;
        expr++; // Skip =
    }

    // Find modifier operator
    char *colon      = strstr(expr, ":-");
    char *colon_eq   = strstr(expr, ":=");
    char *colon_plus = strstr(expr, ":+");
    char *colon_q    = strstr(expr, ":?");

    char *varname     = NULL;
    char *modifier    = NULL;
    char *default_val = NULL;

    if (colon_eq && (!colon || colon_eq < colon) &&
        (!colon_plus || colon_eq < colon_plus) &&
        (!colon_q || colon_eq < colon_q)) {
        // ${VAR:=default}
        varname     = strndup(expr, colon_eq - expr);
        default_val = strdup(colon_eq + 2);
        modifier    = ":=";
    } else if (colon_plus && (!colon || colon_plus < colon) &&
               (!colon_q || colon_plus < colon_q)) {
        // ${VAR:+alt}
        varname     = strndup(expr, colon_plus - expr);
        default_val = strdup(colon_plus + 2);
        modifier    = ":+";
    } else if (colon_q && (!colon || colon_q < colon)) {
        // ${VAR:?error}
        varname     = strndup(expr, colon_q - expr);
        default_val = strdup(colon_q + 2);
        modifier    = ":?";
    } else if (colon) {
        // ${VAR:-default}
        varname     = strndup(expr, colon - expr);
        default_val = strdup(colon + 2);
        modifier    = ":-";
    } else {
        // ${VAR}
        varname = strdup(expr);
    }

    char *value  = environ_get(session->environ, varname);
    char *result = NULL;

    if (!modifier) {
        result = value ? strdup(value) : strdup("");
    } else if (strcmp(modifier, ":-") == 0) {
        result = (value && *value) ? strdup(value) : strdup(default_val);
    } else if (strcmp(modifier, ":=") == 0) {
        if (!value || !*value) {
            environ_set(session->environ, varname, default_val);
            result = strdup(default_val);
        } else {
            result = strdup(value);
        }
    } else if (strcmp(modifier, ":+") == 0) {
        result = (value && *value) ? strdup(default_val) : strdup("");
    } else if (strcmp(modifier, ":?") == 0) {
        if (!value || !*value) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: %s: %s\n", PROJECT_NAME, varname, default_val);
#else
            fprintf(stderr, "tidesh: %s: %s\n", varname, default_val);
#endif
            free(varname);
            free(default_val);
            return NULL;
        } else {
            result = strdup(value);
        }
    }

    free(varname);
    free(default_val);
    return result;
}

Array *variable_expansion(char *input, Session *session) {
    Array *results = init_array(NULL);

    if (!input)
        return results;

    Dynamic buffer = {0};
    init_dynamic(&buffer);
    size_t i = 0;

    bool has_split = false;

    while (input[i]) {
        if (input[i] == '\\' && input[i + 1] == '$') {
            dynamic_append(&buffer, '$');
            i += 2;
            continue;
        }
        if (input[i] == '$') {
            i++; // Skip $

            if (!input[i]) {
                // Trailing $
                dynamic_append(&buffer, '$');
                break;
            }

            char *expanded = NULL;
            bool  is_split = false;

            if (input[i] == '{') {
                // ${...}
                i++; // Skip {
                int close = find_closing_brace(input, i);
                if (close == -1) {
                    // Malformed, treat as literal
                    dynamic_append(&buffer, '$');
                    dynamic_append(&buffer, '{');
                    continue;
                }

                char *expr = strndup(input + i, close - i);
                expanded   = expand_brace_var(expr, session, &is_split);
                if (expanded == NULL) {
                    free(expr);
                    free_dynamic(&buffer);
                    return NULL;
                }
                free(expr);
                i = close + 1;
            } else if (input[i] == '=') {
                // $=VAR
                i++; // Skip =
                char *varname = parse_varname(input, &i);
                char *value   = environ_get(session->environ, varname);
                expanded      = value ? strdup(value) : strdup("");
                free(varname);
                is_split = true;
            } else {
                // $VAR
                char *varname = parse_varname(input, &i);
                char *value   = environ_get(session->environ, varname);
                expanded      = value ? strdup(value) : strdup("");
                free(varname);
            }

            if (is_split && expanded) {
                // Flush current buffer
                if (buffer.length > 0) {
                    char *buf_str = dynamic_to_string(&buffer);
                    array_add(results, buf_str);
                    free(buf_str);
                    init_dynamic(&buffer);
                }

                // Split and add
                Array *split = split_by_whitespace(expanded);
                array_extend(results, split);
                free_array(split);
                free(split);
                has_split = true;
            } else if (expanded) {
                // Append to buffer
                dynamic_extend(&buffer, expanded);
            }

            free(expanded);
        } else {
            // Regular character
            dynamic_append(&buffer, input[i++]);
        }
    }

    // Flush remaining buffer
    if (has_split) {
        if (buffer.length > 0) {
            char *buf_str = dynamic_to_string(&buffer);
            array_add(results, buf_str);
            free(buf_str);
        }
    } else {
        char *buf_str = dynamic_to_string(&buffer);
        array_add(results, buf_str);
        free(buf_str);
    }

    free_dynamic(&buffer);
    return results;
}