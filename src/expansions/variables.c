/* Variables expansion implementation */

#include <ctype.h>   /* isspace, isalnum, isdigit */
#include <fnmatch.h> /* fnmatch */
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

/* Find the closing brace for ${...}, handling nested braces and quotes */
static int find_closing_brace(char *input, size_t start) {
    int    depth        = 1;
    size_t i            = start;
    bool   in_single    = false;
    bool   in_double    = false;
    int    double_depth = 0;

    while (input[i] && depth > 0) {
        if (input[i] == '\\' && input[i + 1] && (in_double || !in_single)) {
            i += 2;
            continue;
        }
        if (!in_double && input[i] == '\'') {
            in_single = !in_single;
        } else if (!in_single && input[i] == '"') {
            in_double = !in_double;
        } else if (!in_single && !in_double) {
            if (input[i] == '{')
                depth++;
            else if (input[i] == '}')
                depth--;
        } else if (in_double) {
            // Inside double quotes, only ${...} creates nested braces
            if (input[i] == '$' && input[i + 1] == '{') {
                depth++;
                double_depth++;
                i++;
            } else if (input[i] == '}' && double_depth > 0) {
                depth--;
                double_depth--;
            }
        }
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

static bool is_nounset_exempt(const char *varname) {
    return strcmp(varname, "@") == 0 || strcmp(varname, "*") == 0 ||
           strcmp(varname, "#") == 0 || strcmp(varname, "?") == 0 ||
           strcmp(varname, "!") == 0 || strcmp(varname, "$") == 0 ||
           strcmp(varname, "0") == 0 || strcmp(varname, "_") == 0;
}

static char *expand_sub_expression(const char *raw, Session *session) {
    if (!raw) {
        return NULL;
    }
    char *copy = strdup(raw);
    if (strchr(copy, '$') && session && session->features.variable_expansion) {
        Array *arr = variable_expansion(copy, session);
        if (arr && arr->count > 0) {
            free(copy);
            copy = strdup(arr->items[0]);
        }
        if (arr) {
            free_array(arr);
            free(arr);
        }
    }
    return copy;
}

static char *strip_pattern_quotes(char *pat) {
    if (!pat) {
        return NULL;
    }
    Dynamic out = {0};
    init_dynamic(&out);
    bool in_single = false;
    bool in_double = false;
    bool escaped   = false;

    for (size_t i = 0; pat[i]; i++) {
        char c = pat[i];
        if (escaped) {
            escaped = false;
            dynamic_append(&out, c);
            continue;
        }
        if (c == '\\') {
            if (in_single) {
                dynamic_append(&out, '\\');
            } else {
                escaped = true;
                dynamic_append(&out, '\\');
            }
            continue;
        }
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (c == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        dynamic_append(&out, c);
    }

    char *result = dynamic_to_string(&out);
    free_dynamic(&out);
    free(pat);
    return result;
}

static char *remove_smallest_prefix(const char *val, const char *pattern) {
    if (!val) {
        return strdup("");
    }
    if (!pattern || !*pattern) {
        return strdup(val);
    }

    size_t len = strlen(val);
    for (size_t k = 0; k <= len; k++) {
        char *sub = strndup(val, k);
        if (fnmatch(pattern, sub, 0) == 0) {
            free(sub);
            return strdup(val + k);
        }
        free(sub);
    }
    return strdup(val);
}

static char *remove_largest_prefix(const char *val, const char *pattern) {
    if (!val) {
        return strdup("");
    }
    if (!pattern || !*pattern) {
        return strdup(val);
    }

    size_t len = strlen(val);
    for (size_t k = len;; k--) {
        char *sub = strndup(val, k);
        if (fnmatch(pattern, sub, 0) == 0) {
            free(sub);
            return strdup(val + k);
        }
        free(sub);
        if (k == 0) {
            break;
        }
    }
    return strdup(val);
}

static char *remove_smallest_suffix(const char *val, const char *pattern) {
    if (!val) {
        return strdup("");
    }
    if (!pattern || !*pattern) {
        return strdup(val);
    }

    size_t len = strlen(val);
    for (size_t k = len;; k--) {
        if (fnmatch(pattern, val + k, 0) == 0) {
            return strndup(val, k);
        }
        if (k == 0) {
            break;
        }
    }
    return strdup(val);
}

static char *remove_largest_suffix(const char *val, const char *pattern) {
    if (!val) {
        return strdup("");
    }
    if (!pattern || !*pattern) {
        return strdup(val);
    }

    size_t len = strlen(val);
    for (size_t k = 0; k <= len; k++) {
        if (fnmatch(pattern, val + k, 0) == 0) {
            return strndup(val, k);
        }
    }
    return strdup(val);
}

static char *substitute_pattern(const char *val, const char *raw_pattern,
                                const char *raw_replacement, bool replace_all,
                                bool anchor_prefix, bool anchor_suffix) {
    if (!val) {
        return strdup("");
    }
    if (!raw_pattern || !*raw_pattern) {
        return strdup(val);
    }

    size_t      len  = strlen(val);
    const char *repl = raw_replacement ? raw_replacement : "";

    if (anchor_prefix) {
        for (size_t k = len;; k--) {
            char *sub = strndup(val, k);
            if (fnmatch(raw_pattern, sub, 0) == 0) {
                free(sub);
                Dynamic res = {0};
                init_dynamic(&res);
                dynamic_extend(&res, (char *)repl);
                dynamic_extend(&res, (char *)(val + k));
                char *s = dynamic_to_string(&res);
                free_dynamic(&res);
                return s ? s : strdup("");
            }
            free(sub);
            if (k == 0) {
                break;
            }
        }
        return strdup(val);
    }

    if (anchor_suffix) {
        for (size_t k = 0; k <= len; k++) {
            if (fnmatch(raw_pattern, val + k, 0) == 0) {
                Dynamic res = {0};
                init_dynamic(&res);
                char *pref = strndup(val, k);
                dynamic_extend(&res, pref);
                free(pref);
                dynamic_extend(&res, (char *)repl);
                char *s = dynamic_to_string(&res);
                free_dynamic(&res);
                return s ? s : strdup("");
            }
        }
        return strdup(val);
    }

    Dynamic res = {0};
    init_dynamic(&res);
    size_t i = 0;
    while (i < len) {
        bool   matched = false;
        size_t best_j  = i;
        for (size_t j = len; j > i; j--) {
            char *sub = strndup(val + i, j - i);
            if (fnmatch(raw_pattern, sub, 0) == 0) {
                matched = true;
                best_j  = j;
                free(sub);
                break;
            }
            free(sub);
        }

        if (matched) {
            dynamic_extend(&res, (char *)repl);
            i = best_j;
            if (!replace_all) {
                dynamic_extend(&res, (char *)(val + i));
                break;
            }
        } else {
            dynamic_append(&res, val[i]);
            i++;
        }
    }

    char *s = dynamic_to_string(&res);
    free_dynamic(&res);
    return s ? s : strdup("");
}

static char *expand_substring(const char *val, const char *spec,
                              Session *session) {
    if (!val) {
        val = "";
    }
    size_t val_len = strlen(val);

    char *colon      = strchr(spec, ':');
    char *offset_str = NULL;
    char *len_str    = NULL;
    if (colon) {
        offset_str = strndup(spec, colon - spec);
        len_str    = strdup(colon + 1);
    } else {
        offset_str = strdup(spec);
    }

    char *exp_offset = expand_sub_expression(offset_str, session);
    free(offset_str);

    long offset = strtol(exp_offset ? exp_offset : "0", NULL, 10);
    free(exp_offset);

    if (offset < 0) {
        offset = (long)val_len + offset;
    }
    if (offset < 0) {
        offset = 0;
    }
    if (offset > (long)val_len) {
        offset = (long)val_len;
    }

    if (len_str) {
        char *exp_len = expand_sub_expression(len_str, session);
        free(len_str);
        long length = strtol(exp_len ? exp_len : "0", NULL, 10);
        free(exp_len);

        if (length < 0) {
            length = (long)val_len + length - offset;
        }
        if (length < 0) {
            length = 0;
        }
        if (offset + length > (long)val_len) {
            length = (long)val_len - offset;
        }
        return strndup(val + offset, length);
    }

    return strdup(val + offset);
}

/* Expand ${VAR} with modifiers - returns NULL for split marker */
static char *expand_brace_var(char *expr, Session *session, bool *is_split) {
    *is_split = false;

    // Handle ${#VAR} - length (but ${#} is the positional parameter count)
    if (expr[0] == '#' && expr[1] != '\0' && expr[1] != '#' && expr[1] != '%' &&
        expr[1] != '/' && expr[1] != ':') {
        char *varname = strdup(expr + 1);
        char *value   = environ_get(session->environ, varname);
        if (!value && strcmp(varname, "#") == 0) {
            value = "0";
        }
        if (!value && session->options.nounset && !is_nounset_exempt(varname)) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                    varname);
#else
            fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
            free(varname);
            return NULL;
        }
        free(varname);

        char result[32];
        snprintf(result, sizeof(result), "%zu", value ? strlen(value) : 0);
        return strdup(result);
    }

    // Handle ${=VAR...} - split
    if (expr[0] == '=') {
        *is_split = true;
        expr++; // Skip =
    }

    // Parse varname from expr
    size_t pos = 0;
    if (expr[pos] == '?' || expr[pos] == '$' || expr[pos] == '!' ||
        expr[pos] == '#' || expr[pos] == '@' || expr[pos] == '*' ||
        expr[pos] == '0') {
        pos++;
    } else if (isdigit((unsigned char)expr[pos])) {
        while (isdigit((unsigned char)expr[pos])) {
            pos++;
        }
    } else if (expr[pos] == '_' && !isalnum((unsigned char)expr[pos + 1]) &&
               expr[pos + 1] != '_') {
        pos++;
    } else {
        while (expr[pos] &&
               (isalnum((unsigned char)expr[pos]) || expr[pos] == '_')) {
            pos++;
        }
    }

    char *varname = strndup(expr, pos);
    char *op_ptr  = expr + pos;

    char *value = environ_get(session->environ, varname);
    if (!value && strcmp(varname, "#") == 0) {
        value = "0";
    }

    char *result = NULL;

    if (*op_ptr == '\0') {
        // Simple ${VAR}
        if (!value && session->options.nounset && !is_nounset_exempt(varname)) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                    varname);
#else
            fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
            free(varname);
            return NULL;
        }
        result = value ? strdup(value) : strdup("");
    } else if (*op_ptr == '#') {
        // Prefix removal: ${VAR#pattern} or ${VAR##pattern}
        if (!value && session->options.nounset && !is_nounset_exempt(varname)) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                    varname);
#else
            fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
            free(varname);
            return NULL;
        }
        bool  longest = (op_ptr[1] == '#');
        char *raw_pat = op_ptr + (longest ? 2 : 1);
        char *pat     = expand_sub_expression(raw_pat, session);
        pat           = strip_pattern_quotes(pat);
        if (longest) {
            result = remove_largest_prefix(value ? value : "", pat);
        } else {
            result = remove_smallest_prefix(value ? value : "", pat);
        }
        free(pat);
    } else if (*op_ptr == '%') {
        // Suffix removal: ${VAR%pattern} or ${VAR%%pattern}
        if (!value && session->options.nounset && !is_nounset_exempt(varname)) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                    varname);
#else
            fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
            free(varname);
            return NULL;
        }
        bool  longest = (op_ptr[1] == '%');
        char *raw_pat = op_ptr + (longest ? 2 : 1);
        char *pat     = expand_sub_expression(raw_pat, session);
        pat           = strip_pattern_quotes(pat);
        if (longest) {
            result = remove_largest_suffix(value ? value : "", pat);
        } else {
            result = remove_smallest_suffix(value ? value : "", pat);
        }
        free(pat);
    } else if (*op_ptr == '/') {
        // Pattern substitution: ${VAR/pattern/repl} or ${VAR//pattern/repl}
        if (!value && session->options.nounset && !is_nounset_exempt(varname)) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                    varname);
#else
            fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
            free(varname);
            return NULL;
        }
        bool  replace_all   = (op_ptr[1] == '/');
        char *spec          = op_ptr + (replace_all ? 2 : 1);
        bool  anchor_prefix = (*spec == '#');
        bool  anchor_suffix = (*spec == '%');
        if (anchor_prefix || anchor_suffix) {
            spec++;
        }
        char *slash = strchr(spec, '/');
        char *pat   = NULL;
        char *repl  = NULL;
        if (slash) {
            pat  = strndup(spec, slash - spec);
            repl = expand_sub_expression(slash + 1, session);
        } else {
            pat  = strdup(spec);
            repl = strdup("");
        }
        char *exp_pat = expand_sub_expression(pat, session);
        free(pat);
        exp_pat = strip_pattern_quotes(exp_pat);
        result  = substitute_pattern(value ? value : "", exp_pat, repl,
                                     replace_all, anchor_prefix, anchor_suffix);
        free(exp_pat);
        free(repl);
    } else if (*op_ptr == ':' && (op_ptr[1] != '-' && op_ptr[1] != '=' &&
                                  op_ptr[1] != '?' && op_ptr[1] != '+')) {
        // Substring expansion: ${VAR:offset} or ${VAR:offset:length}
        if (!value && session->options.nounset && !is_nounset_exempt(varname)) {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                    varname);
#else
            fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
            free(varname);
            return NULL;
        }
        result = expand_substring(value ? value : "", op_ptr + 1, session);
    } else {
        // Default value / assignment / alternative / error modifiers:
        // :-, -, :=, =, :+, +, :?, ?
        char *modifier    = NULL;
        char *default_val = NULL;

        if (strncmp(op_ptr, ":=", 2) == 0) {
            modifier    = ":=";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 2, session));
        } else if (strncmp(op_ptr, ":-", 2) == 0) {
            modifier    = ":-";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 2, session));
        } else if (strncmp(op_ptr, ":+", 2) == 0) {
            modifier    = ":+";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 2, session));
        } else if (strncmp(op_ptr, ":?", 2) == 0) {
            modifier    = ":?";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 2, session));
        } else if (*op_ptr == '-') {
            modifier    = "-";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 1, session));
        } else if (*op_ptr == '=') {
            modifier    = "=";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 1, session));
        } else if (*op_ptr == '+') {
            modifier    = "+";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 1, session));
        } else if (*op_ptr == '?') {
            modifier    = "?";
            default_val = strip_pattern_quotes(
                expand_sub_expression(op_ptr + 1, session));
        }

        if (!modifier) {
            // Unrecognized modifier
            result = value ? strdup(value) : strdup("");
        } else if (strcmp(modifier, ":-") == 0) {
            result = (value && *value) ? strdup(value)
                                       : strdup(default_val ? default_val : "");
        } else if (strcmp(modifier, "-") == 0) {
            result =
                value ? strdup(value) : strdup(default_val ? default_val : "");
        } else if (strcmp(modifier, ":=") == 0) {
            if (!value || !*value) {
                environ_set(session->environ, varname,
                            default_val ? default_val : "");
                result = strdup(default_val ? default_val : "");
            } else {
                result = strdup(value);
            }
        } else if (strcmp(modifier, "=") == 0) {
            if (!value) {
                environ_set(session->environ, varname,
                            default_val ? default_val : "");
                result = strdup(default_val ? default_val : "");
            } else {
                result = strdup(value);
            }
        } else if (strcmp(modifier, ":+") == 0) {
            result = (value && *value) ? strdup(default_val ? default_val : "")
                                       : strdup("");
        } else if (strcmp(modifier, "+") == 0) {
            result =
                value ? strdup(default_val ? default_val : "") : strdup("");
        } else if (strcmp(modifier, ":?") == 0) {
            if (!value || !*value) {
#ifdef PROJECT_NAME
                fprintf(stderr, "%s: %s: %s\n", PROJECT_NAME, varname,
                        default_val ? default_val
                                    : "parameter null or not set");
#else
                fprintf(stderr, "tidesh: %s: %s\n", varname,
                        default_val ? default_val
                                    : "parameter null or not set");
#endif
                free(varname);
                free(default_val);
                return NULL;
            }
            result = strdup(value);
        } else if (strcmp(modifier, "?") == 0) {
            if (!value) {
#ifdef PROJECT_NAME
                fprintf(stderr, "%s: %s: %s\n", PROJECT_NAME, varname,
                        default_val ? default_val : "parameter not set");
#else
                fprintf(stderr, "tidesh: %s: %s\n", varname,
                        default_val ? default_val : "parameter not set");
#endif
                free(varname);
                free(default_val);
                return NULL;
            }
            result = strdup(value);
        }

        if (default_val) {
            free(default_val);
        }
    }

    free(varname);
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
                if (!value && session->options.nounset &&
                    strcmp(varname, "@") != 0 && strcmp(varname, "*") != 0 &&
                    strcmp(varname, "#") != 0 && strcmp(varname, "?") != 0 &&
                    strcmp(varname, "!") != 0 && strcmp(varname, "$") != 0 &&
                    strcmp(varname, "0") != 0 && strcmp(varname, "_") != 0) {
#ifdef PROJECT_NAME
                    fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                            varname);
#else
                    fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
                    free(varname);
                    free_dynamic(&buffer);
                    return NULL;
                }
                expanded = value ? strdup(value) : strdup("");
                free(varname);
                is_split = true;
            } else {
                // $VAR
                char *varname = parse_varname(input, &i);
                char *value   = environ_get(session->environ, varname);
                if (!value && strcmp(varname, "#") == 0) {
                    expanded = strdup("0");
                } else if (!value && session->options.nounset &&
                           strcmp(varname, "@") != 0 &&
                           strcmp(varname, "*") != 0 &&
                           strcmp(varname, "?") != 0 &&
                           strcmp(varname, "!") != 0 &&
                           strcmp(varname, "$") != 0 &&
                           strcmp(varname, "0") != 0 &&
                           strcmp(varname, "_") != 0) {
#ifdef PROJECT_NAME
                    fprintf(stderr, "%s: %s: unbound variable\n", PROJECT_NAME,
                            varname);
#else
                    fprintf(stderr, "tidesh: %s: unbound variable\n", varname);
#endif
                    free(varname);
                    free_dynamic(&buffer);
                    return NULL;
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