#include <stddef.h> /* size_t, NULL */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* strcmp, strlen */

#include "data/array.h" /* init_array, array_extend, free_array, Array */
#include "data/dynamic.h"
#include "execute.h" /* execute_string_stdout */
#include "expand.h"  /* Array */
#ifndef TIDESH_DISABLE_ALIASES
#include "expansions/aliases.h" /* alias_expansion */
#endif
#include "expansions/arithmetic.h" /* expand_arithmetic */
#include "expansions/braces.h"     /* brace_expansion */
#include "expansions/filenames.h"  /* filename_expansion */
#include "expansions/tildes.h"     /* tilde_expansion */
#include "expansions/variables.h"  /* variable_expansion */
#include "session.h"               /* Session */

/* Helper to apply an expansion function to all items in an Array */
static Array *apply(Array *inputs, Array *(*expansion_func)(char *, Session *),
                    Session *session) {
    Array *results = init_array(NULL);

    for (size_t i = 0; i < inputs->count; i++) {
        Array *expanded = expansion_func(inputs->items[i], session);
        if (expanded == NULL) {
            free_array(results);
            free(results);
            return NULL;
        }
        array_extend(results, expanded);
        free_array(expanded);
        free(expanded);
    }

    return results;
}

char *expand_command_substitution(const char *input, Session *session) {
    if (!input) {
        return NULL;
    }
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
    if (!session || !session->features.command_substitution) {
        return strdup(input);
    }
    if (strstr(input, "$(") == NULL) {
        return strdup(input);
    }

    Dynamic result = {0};
    init_dynamic(&result);
    size_t i   = 0;
    size_t len = strlen(input);

    while (i < len) {
        if (input[i] == '\\' && input[i + 1] == '$') {
            dynamic_append(&result, '\\');
            dynamic_append(&result, '$');
            i += 2;
            continue;
        }

        if (input[i] == '$' && input[i + 1] == '(') {
            if (i + 2 < len && input[i + 2] == '(') {
                // Arithmetic expansion $(( expression )), copy verbatim
                dynamic_append(&result, '$');
                dynamic_append(&result, '(');
                dynamic_append(&result, '(');
                i += 3;
                size_t pdepth = 0;
                while (i < len) {
                    if (input[i] == '\\' && i + 1 < len) {
                        dynamic_append(&result, input[i++]);
                        dynamic_append(&result, input[i++]);
                        continue;
                    }
                    if (input[i] == '(') {
                        pdepth++;
                        dynamic_append(&result, input[i++]);
                    } else if (input[i] == ')') {
                        if (pdepth > 0) {
                            pdepth--;
                            dynamic_append(&result, input[i++]);
                        } else if (i + 1 < len && input[i + 1] == ')') {
                            dynamic_append(&result, input[i++]);
                            dynamic_append(&result, input[i++]);
                            break;
                        } else {
                            dynamic_append(&result, input[i++]);
                        }
                    } else {
                        dynamic_append(&result, input[i++]);
                    }
                }
                continue;
            }

            i += 2; // skip "$("
            size_t  depth   = 1;
            Dynamic cmd_dyn = {0};
            init_dynamic(&cmd_dyn);

            while (i < len && depth > 0) {
                char c = input[i++];
                if (c == '\\' && i < len) {
                    dynamic_append(&cmd_dyn, c);
                    dynamic_append(&cmd_dyn, input[i++]);
                    continue;
                }
                if (c == '\'') {
                    dynamic_append(&cmd_dyn, c);
                    while (i < len && input[i] != '\'') {
                        dynamic_append(&cmd_dyn, input[i++]);
                    }
                    if (i < len) {
                        dynamic_append(&cmd_dyn,
                                       input[i++]); // consume closing '
                    }
                    continue;
                }
                if (c == '"') {
                    dynamic_append(&cmd_dyn, c);
                    while (i < len && input[i] != '"') {
                        if (input[i] == '\\' && i + 1 < len) {
                            dynamic_append(&cmd_dyn, input[i++]);
                        }
                        dynamic_append(&cmd_dyn, input[i++]);
                    }
                    if (i < len) {
                        dynamic_append(&cmd_dyn,
                                       input[i++]); // consume closing "
                    }
                    continue;
                }
                if (c == '(') {
                    depth++;
                    dynamic_append(&cmd_dyn, c);
                } else if (c == ')') {
                    depth--;
                    if (depth > 0) {
                        dynamic_append(&cmd_dyn, c);
                    }
                } else {
                    dynamic_append(&cmd_dyn, c);
                }
            }

            char *cmd = dynamic_to_string(&cmd_dyn);
            free_dynamic(&cmd_dyn);

            if (cmd) {
                char *output = execute_string_stdout(cmd, session);
                if (output) {
                    dynamic_extend(&result, output);
                    free(output);
                }
                free(cmd);
            }
        } else {
            dynamic_append(&result, input[i++]);
        }
    }

    char *ret = dynamic_to_string(&result);
    free_dynamic(&result);
    return ret;
#else
    return strdup(input);
#endif
}

Array *full_expansion(char *input, Session *session) {
    if (!session) {
        Array *results = init_array(NULL);
        if (input)
            array_add(results, input);
        return results;
    }

    // Arithmetic expansion $((...))
    char *after_arith = NULL;
    if (session->features.arithmetic_expansion) {
        after_arith = expand_arithmetic(input, session);
        if (after_arith == NULL) {
            return NULL;
        }
    } else {
        after_arith = strdup(input);
    }

    // Command substitution $(...)
    char *after_cmd_sub = NULL;
    if (session->features.command_substitution) {
        after_cmd_sub = expand_command_substitution(after_arith, session);
        free(after_arith);
        if (after_cmd_sub == NULL) {
            return NULL;
        }
    } else {
        after_cmd_sub = after_arith;
    }

    // Variable expansion
    Array *after_variables = NULL;
    if (session->features.variable_expansion) {
        after_variables = variable_expansion(after_cmd_sub, session);
        free(after_cmd_sub);
        if (after_variables == NULL) {
            return NULL;
        }
    } else {
        after_variables = init_array(NULL);
        array_add(after_variables, after_cmd_sub);
        free(after_cmd_sub);
    }

    // Tilde expansion
    Array *after_tildes = NULL;
    if (session->features.tilde_expansion) {
        after_tildes = apply(after_variables, tilde_expansion, session);
        free_array(after_variables);
        free(after_variables);
        if (after_tildes == NULL) {
            return NULL;
        }
    } else {
        after_tildes = after_variables;
    }

    // Brace expansion
    Array *after_braces = NULL;
    if (session->features.brace_expansion) {
        after_braces = apply(after_tildes, brace_expansion, session);
        free_array(after_tildes);
        free(after_tildes);
        if (after_braces == NULL) {
            return NULL;
        }
    } else {
        after_braces = after_tildes;
    }

    // Filename expansion (globbing)
    Array *after_filenames = NULL;
    if (session->features.filename_expansion) {
        after_filenames = apply(after_braces, filename_expansion, session);
        free_array(after_braces);
        free(after_braces);
    } else {
        after_filenames = after_braces;
    }

    return after_filenames;
}
