/** shift.c
 *
 * Implementation of the shift builtin command.
 */

#include <ctype.h>   /* isdigit */
#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* fprintf, stderr, snprintf */
#include <stdlib.h>  /* strtol, free */
#include <string.h>  /* strchr, strndup */

#include "builtins/shift.h"
#include "data/array.h"   /* Array, free_array */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, dynamic_to_string, free_dynamic */
#include "environ.h" /* environ_get, environ_set, environ_remove, environ_to_array */
#include "session.h" /* Session */

static bool is_numeric_positional(const char *key) {
    if (!key || !*key) {
        return false;
    }
    if (strcmp(key, "0") == 0) {
        return false; // Preserve $0
    }
    for (const char *p = key; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }
    return true;
}

int builtin_shift(int argc, char **argv, Session *session) {
    if (!session || !session->environ) {
        return 1;
    }

    if (argc > 2) {
        fprintf(stderr, "shift: too many arguments\n");
        return 1;
    }

    long shift_n = 1;
    if (argc == 2) {
        const char *arg = argv[1];
        if (!*arg) {
            fprintf(stderr, "shift: %s: numeric argument required\n", arg);
            return 1;
        }

        // Must be non-negative integer digits
        for (const char *p = arg; *p; p++) {
            if (!isdigit((unsigned char)*p)) {
                fprintf(stderr, "shift: %s: numeric argument required\n", arg);
                return 1;
            }
        }

        char *endptr = NULL;
        shift_n      = strtol(arg, &endptr, 10);
        if (*endptr != '\0' || shift_n < 0) {
            fprintf(stderr, "shift: %s: numeric argument required\n", arg);
            return 1;
        }
    }

    // Get current parameter count
    char *count_str = environ_get(session->environ, "#");
    int   count     = count_str ? atoi(count_str) : 0;

    if (shift_n > count) {
        fprintf(stderr, "shift: %ld: can't shift that many\n", shift_n);
        return 1;
    }

    if (shift_n == 0) {
        return 0;
    }

    int new_count = count - (int)shift_n;

    // Shift parameters: 1 receives (1 + shift_n), etc.
    for (int i = 1; i <= new_count; i++) {
        char src_key[32];
        snprintf(src_key, sizeof(src_key), "%d", i + (int)shift_n);
        char *val = environ_get(session->environ, src_key);

        char dst_key[32];
        snprintf(dst_key, sizeof(dst_key), "%d", i);
        environ_set(session->environ, dst_key, val ? val : "");
    }

    // Unset the old parameters beyond new_count
    for (int i = new_count + 1; i <= count; i++) {
        char rem_key[32];
        snprintf(rem_key, sizeof(rem_key), "%d", i);
        environ_remove(session->environ, rem_key);
    }

    // Clean up any extra numeric positional keys that might exceed new_count
    Array *env_array = environ_to_array(session->environ);
    if (env_array) {
        for (size_t k = 0; k < env_array->count; k++) {
            char *eq = strchr(env_array->items[k], '=');
            if (eq) {
                char *key =
                    strndup(env_array->items[k], eq - env_array->items[k]);
                if (key) {
                    if (is_numeric_positional(key)) {
                        int num = atoi(key);
                        if (num > new_count) {
                            environ_remove(session->environ, key);
                        }
                    }
                    free(key);
                }
            }
        }
        free_array(env_array);
        free(env_array);
    }

    // Update $#
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d", new_count);
    environ_set(session->environ, "#", count_buf);

    // Update $@ and $*
    Dynamic all_params = {0};
    init_dynamic(&all_params);
    for (int i = 1; i <= new_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%d", i);
        char *val = environ_get(session->environ, key);
        if (i > 1) {
            dynamic_append(&all_params, ' ');
        }
        if (val) {
            dynamic_extend(&all_params, val);
        }
    }
    char *all_str = dynamic_to_string(&all_params);
    environ_set(session->environ, "@", all_str ? all_str : "");
    environ_set(session->environ, "*", all_str ? all_str : "");
    if (all_str) {
        free(all_str);
    }
    free_dynamic(&all_params);

    return 0;
}
