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

    // Check if first character is a special single-character variable or a
    // digit
    if (input[*pos] == '?' || input[*pos] == '$' || input[*pos] == '!' ||
        input[*pos] == '#' || input[*pos] == '@' || input[*pos] == '*' ||
        isdigit((unsigned char)input[*pos])) {
        (*pos)++;
    } else if (input[*pos] == '_' && !isalnum((unsigned char)input[*pos + 1]) &&
               input[*pos + 1] != '_') {
        // Standalone $_
        (*pos)++;
    } else {
        while (input[*pos] &&
               (isalnum((unsigned char)input[*pos]) || input[*pos] == '_')) {
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

    // Handle ${#VAR} - length (but ${#} is the positional parameter count)
    if (expr[0] == '#' && expr[1] != '\0') {
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

    // Find modifier operator (check colon variants first, then non-colon)
    char *colon_eq   = strstr(expr, ":=");
    char *colon_dash = strstr(expr, ":-");
    char *colon_plus = strstr(expr, ":+");
    char *colon_q    = strstr(expr, ":?");
    char *eq         = strchr(expr, '=');
    char *dash       = strchr(expr, '-');
    char *plus       = strchr(expr, '+');
    char *q          = strchr(expr, '?');

    // Exclude matches that are part of longer operators
    if (eq && (colon_eq == (eq - 1)))
        eq = NULL;
    if (dash && (colon_dash == (dash - 1)))
        dash = NULL;
    if (plus && (colon_plus == (plus - 1)))
        plus = NULL;
    if (q && (colon_q == (q - 1)))
        q = NULL;

    char *varname     = NULL;
    char *modifier    = NULL;
    char *default_val = NULL;
    bool  check_empty = false;

    // Determine which modifier to use (prefer colon variants, fallback to
    // non-colon)
    if (colon_eq) {
        varname     = strndup(expr, colon_eq - expr);
        default_val = strdup(colon_eq + 2);
        modifier    = ":=";
        check_empty = true;
    } else if (colon_plus) {
        varname     = strndup(expr, colon_plus - expr);
        default_val = strdup(colon_plus + 2);
        modifier    = ":+";
        check_empty = true;
    } else if (colon_q) {
        varname     = strndup(expr, colon_q - expr);
        default_val = strdup(colon_q + 2);
        modifier    = ":?";
        check_empty = true;
    } else if (colon_dash) {
        varname     = strndup(expr, colon_dash - expr);
        default_val = strdup(colon_dash + 2);
        modifier    = ":-";
        check_empty = true;
    } else if (eq) {
        varname     = strndup(expr, eq - expr);
        default_val = strdup(eq + 1);
        modifier    = "=";
        check_empty = false;
    } else if (plus) {
        varname     = strndup(expr, plus - expr);
        default_val = strdup(plus + 1);
        modifier    = "+";
        check_empty = false;
    } else if (q) {
        varname     = strndup(expr, q - expr);
        default_val = strdup(q + 1);
        modifier    = "?";
        check_empty = false;
    } else if (dash) {
        varname     = strndup(expr, dash - expr);
        default_val = strdup(dash + 1);
        modifier    = "-";
        check_empty = false;
    } else {
        // ${VAR}
        varname = strdup(expr);
    }

    char *value = environ_get(session->environ, varname);
    if (!value && strcmp(varname, "#") == 0) {
        value = "0";
    }
    char *result = NULL;

    // Expand nested variables in default_val if present
    if (default_val && strchr(default_val, '$') &&
        session->features.variable_expansion) {
        Array *def_arr = variable_expansion(default_val, session);
        if (def_arr && def_arr->count > 0) {
            free(default_val);
            default_val = strdup(def_arr->items[0]);
            free_array(def_arr);
            free(def_arr);
        }
    }

    if (!modifier) {
        result = value ? strdup(value) : strdup("");
    } else if (strcmp(modifier, ":-") == 0) {
        // ${VAR:-default} - use default if unset or empty
        result = (value && *value) ? strdup(value) : strdup(default_val);
    } else if (strcmp(modifier, "-") == 0) {
        // ${VAR-default} - use default if unset
        result = value ? strdup(value) : strdup(default_val);
    } else if (strcmp(modifier, ":=") == 0) {
        // ${VAR:=default} - set to default if unset or empty
        if (!value || !*value) {
            environ_set(session->environ, varname, default_val);
            result = strdup(default_val);
        } else {
            result = strdup(value);
        }
    } else if (strcmp(modifier, "=") == 0) {
        // ${VAR=default} - set to default if unset
        if (!value) {
            environ_set(session->environ, varname, default_val);
            result = strdup(default_val);
        } else {
            result = strdup(value);
        }
    } else if (strcmp(modifier, ":+") == 0) {
        // ${VAR:+alt} - use alt if set and non-empty
        result = (value && *value) ? strdup(default_val) : strdup("");
    } else if (strcmp(modifier, "+") == 0) {
        // ${VAR+alt} - use alt if set
        result = value ? strdup(default_val) : strdup("");
    } else if (strcmp(modifier, ":?") == 0) {
        // ${VAR:?error} - error if unset or empty
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
    } else if (strcmp(modifier, "?") == 0) {
        // ${VAR?error} - error if unset
        if (!value) {
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
                if (!value && strcmp(varname, "#") == 0) {
                    expanded = strdup("0");
                } else {
                    expanded = value ? strdup(value) : strdup("");
                }
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