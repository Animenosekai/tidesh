/* Tilde expansion implementation */

#include <ctype.h>   /* isspace, isdigit */
#include <pwd.h>     /* getpwnam, struct passwd */
#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* fprintf, stderr */
#include <stdlib.h>  /* malloc, free, strtol */
#include <string.h>  /* strdup, strndup */
#include <unistd.h>  /* getuid */

#include "data/array.h"   /* Array, init_array, array_add, free_array */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, free_dynamic, dynamic_to_string */
#include "dirstack.h"     /* dirstack_peek */
#include "environ.h"      /* Environ, environ_get */
#include "expansions/tildes.h" /* tilde_expansion */
#include "session.h"           /* Session */

/* Expand a tilde prefix */
static char *expand_tilde_prefix(char *prefix, Session *session) {
    if (prefix[0] == '\0') {
        // ~ -> HOME
        char *home = environ_get(session->environ, "HOME");
        return home ? strdup(home) : strdup("");
    }

    if (prefix[0] == '+' && prefix[1] == '\0') {
        // ~+ -> PWD (current directory)
        return session->current_working_dir
                   ? strdup(session->current_working_dir)
                   : strdup("");
    }

    if (prefix[0] == '-' && prefix[1] == '\0') {
        // ~- -> OLDPWD (previous directory)
        return session->previous_working_dir
                   ? strdup(session->previous_working_dir)
                   : strdup("");
    }

    // Check for ~N (directory stack)
    if (isdigit(prefix[0])) {
        char *endptr;
        long  index = strtol(prefix, &endptr, 10);
        if (*endptr == '\0') {
            char *dirstack_entry = NULL;
#ifndef TIDESH_DISABLE_DIRSTACK
            if (session->dirstack) {
                dirstack_entry =
                    dirstack_peek(session->dirstack, (size_t)index);
            }
#endif
            if (dirstack_entry == NULL) {
#ifdef PROJECT_NAME
                fprintf(stderr, "%s: No such entry ~%ld in directory stack\n",
                        PROJECT_NAME, index);
#else
                fprintf(stderr,
                        "tidesh: No such entry ~%ld in directory stack\n",
                        index);
#endif
            }
            return dirstack_entry;
        }
    }

    // ~user -> /home/user
    struct passwd *pw = getpwnam(prefix);
    if (pw) {
        return strdup(pw->pw_dir);
    }

    // Not found, return as-is
    Dynamic result = {0};
    init_dynamic(&result);
    dynamic_append(&result, '~');
    dynamic_extend(&result, (char *)prefix);
    char *ret = dynamic_to_string(&result);
    free_dynamic(&result);
    return ret;
}

/* Check if tilde should be expanded at this position */
static bool should_expand_tilde(char *input, size_t pos) {
    // Tilde expands at start of word or after :
    if (pos == 0)
        return true;
    if (input[pos - 1] == ':')
        return true;
    if (isspace(input[pos - 1]))
        return true;
    return false;
}

/* Find the end of a tilde prefix */
static size_t find_tilde_end(char *input, size_t start) {
    size_t i = start;

    // Skip ~ itself
    i++;

    // Special cases: ~+ and ~-
    if (input[i] == '+' || input[i] == '-') {
        return i + 1;
    }

    // Username or number
    while (input[i] && !isspace(input[i]) && input[i] != '/' &&
           input[i] != ':') {
        i++;
    }

    return i;
}

Array *tilde_expansion(char *input, Session *session) {
    Array *results = init_array(NULL);

    if (!input)
        return results;

    Dynamic buffer = {0};
    init_dynamic(&buffer);
    size_t i = 0;

    while (input[i]) {
        if (input[i] == '~' && should_expand_tilde(input, i)) {
            size_t tilde_end = find_tilde_end(input, i);

            // Extract tilde prefix (without the ~)
            char *prefix = strndup(input + i + 1, tilde_end - i - 1);

            // Expand it
            char *expanded = expand_tilde_prefix(prefix, session);
            free(prefix);

            if (!expanded) {
                free_dynamic(&buffer);
                free_array(results);
                free(results);
                return NULL;
            }

            // Append to buffer
            dynamic_extend(&buffer, expanded);
            free(expanded);

            i = tilde_end;
        } else {
            // Regular character
            dynamic_append(&buffer, input[i++]);
        }
    }

    char *result = dynamic_to_string(&buffer);
    array_add(results, result);
    free(result);
    free_dynamic(&buffer);

    return results;
}
