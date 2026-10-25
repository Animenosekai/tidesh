/** set.c
 *
 * Implementation of the set builtin command.
 */

#include <ctype.h>   /* isdigit */
#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* printf, fprintf, stderr */
#include <stdlib.h>  /* free, malloc */
#include <string.h>  /* strcmp, strchr, strndup, strlen */

#include "builtins/set.h"
#include "data/array.h"   /* Array, free_array, array_sort */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, dynamic_to_string, free_dynamic */
#include "environ.h"      /* environ_to_array, environ_set, environ_remove */
#include "session.h"      /* Session */

static bool is_positional_or_special(const char *key) {
    if (!key || !*key) {
        return true;
    }
    if (strcmp(key, "#") == 0 || strcmp(key, "@") == 0 ||
        strcmp(key, "*") == 0 || strcmp(key, "?") == 0 ||
        strcmp(key, "!") == 0 || strcmp(key, "$") == 0 ||
        strcmp(key, "_") == 0) {
        return true;
    }
    // Check if key is purely digits (e.g. 0, 1, 2...)
    bool all_digits = true;
    for (const char *p = key; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            all_digits = false;
            break;
        }
    }
    return all_digits;
}

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

static int print_all_variables(Session *session) {
    Array *env_array = environ_to_array(session->environ);
    if (!env_array) {
        return 0;
    }

    array_sort(env_array);
    for (size_t i = 0; i < env_array->count; i++) {
        char *entry = env_array->items[i];
        char *eq    = strchr(entry, '=');
        if (!eq) {
            continue;
        }
        char *key = strndup(entry, eq - entry);
        if (key) {
            if (!is_positional_or_special(key)) {
                printf("%s\n", entry);
            }
            free(key);
        }
    }

    free_array(env_array);
    free(env_array);
    return 0;
}

static const struct {
    const char *name;
} standard_options[] = {
    {"allexport"}, {"errexit"}, {"hashall"}, {"ignoreeof"}, {"monitor"},
    {"noclobber"}, {"noexec"},  {"noglob"},  {"notify"},    {"nounset"},
    {"pipefail"},  {"verbose"}, {"xtrace"},
};

static size_t standard_options_count(void) {
    return sizeof(standard_options) / sizeof(standard_options[0]);
}

static bool get_option_status(Session *session, const char *name) {
    if (strcmp(name, "allexport") == 0)
        return session->options.allexport;
    if (strcmp(name, "errexit") == 0)
        return session->options.errexit;
    if (strcmp(name, "noglob") == 0)
        return !session->features.filename_expansion;
    if (strcmp(name, "nounset") == 0)
        return session->options.nounset;
    if (strcmp(name, "verbose") == 0)
        return session->options.verbose;
    if (strcmp(name, "xtrace") == 0)
        return session->options.xtrace;
    if (strcmp(name, "noexec") == 0)
        return session->options.noexec;
    if (strcmp(name, "monitor") == 0)
        return session->features.job_control;
    if (strcmp(name, "noclobber") == 0)
        return session->options.noclobber;
    if (strcmp(name, "pipefail") == 0)
        return session->options.pipefail;
    if (strcmp(name, "notify") == 0)
        return session->options.notify;
    if (strcmp(name, "hashall") == 0)
        return session->options.hashall;
    if (strcmp(name, "ignoreeof") == 0)
        return session->options.ignoreeof;
    return false;
}

static void print_options_status(Session *session) {
    for (size_t i = 0; i < standard_options_count(); i++) {
        const char *name  = standard_options[i].name;
        bool        is_on = get_option_status(session, name);
        printf("%-15s\t%s\n", name, is_on ? "on" : "off");
    }
}

static void print_options_commands(Session *session) {
    for (size_t i = 0; i < standard_options_count(); i++) {
        const char *name  = standard_options[i].name;
        bool        is_on = get_option_status(session, name);
        printf("set %co %s\n", is_on ? '-' : '+', name);
    }
}

static bool set_option_by_name(Session *session, const char *name,
                               bool enable) {
    if (strcmp(name, "allexport") == 0) {
        session->options.allexport = enable;
        return true;
    }
    if (strcmp(name, "errexit") == 0) {
        session->options.errexit = enable;
        return true;
    }
    if (strcmp(name, "noglob") == 0) {
        session->options.noglob              = enable;
        session->features.filename_expansion = !enable;
        return true;
    }
    if (strcmp(name, "nounset") == 0) {
        session->options.nounset = enable;
        return true;
    }
    if (strcmp(name, "verbose") == 0) {
        session->options.verbose = enable;
        return true;
    }
    if (strcmp(name, "xtrace") == 0) {
        session->options.xtrace = enable;
        return true;
    }
    if (strcmp(name, "noexec") == 0) {
        session->options.noexec = enable;
        return true;
    }
    if (strcmp(name, "monitor") == 0) {
        session->options.monitor      = enable;
        session->features.job_control = enable;
        return true;
    }
    if (strcmp(name, "noclobber") == 0) {
        session->options.noclobber = enable;
        return true;
    }
    if (strcmp(name, "pipefail") == 0) {
        session->options.pipefail = enable;
        return true;
    }
    if (strcmp(name, "notify") == 0) {
        session->options.notify = enable;
        return true;
    }
    if (strcmp(name, "hashall") == 0) {
        session->options.hashall = enable;
        return true;
    }
    if (strcmp(name, "ignoreeof") == 0) {
        session->options.ignoreeof = enable;
        return true;
    }

    // Also support runtime feature flags via -o / +o
    if (strcmp(name, "filename_expansion") == 0) {
        session->features.filename_expansion = enable;
        session->options.noglob              = !enable;
        return true;
    }
    if (strcmp(name, "job_control") == 0) {
        session->features.job_control = enable;
        session->options.monitor      = enable;
        return true;
    }
    if (strcmp(name, "pipes") == 0) {
        session->features.pipes = enable;
        return true;
    }
    if (strcmp(name, "redirections") == 0) {
        session->features.redirections = enable;
        return true;
    }
    if (strcmp(name, "sequences") == 0) {
        session->features.sequences = enable;
        return true;
    }
    if (strcmp(name, "subshells") == 0) {
        session->features.subshells = enable;
        return true;
    }
    if (strcmp(name, "blocks") == 0) {
        session->features.blocks = enable;
        return true;
    }
    if (strcmp(name, "command_substitution") == 0) {
        session->features.command_substitution = enable;
        return true;
    }
    if (strcmp(name, "assignments") == 0) {
        session->features.assignments = enable;
        return true;
    }
    if (strcmp(name, "functions") == 0) {
        session->features.functions = enable;
        return true;
    }
    if (strcmp(name, "variable_expansion") == 0) {
        session->features.variable_expansion = enable;
        return true;
    }
    if (strcmp(name, "tilde_expansion") == 0) {
        session->features.tilde_expansion = enable;
        return true;
    }
    if (strcmp(name, "brace_expansion") == 0) {
        session->features.brace_expansion = enable;
        return true;
    }
    if (strcmp(name, "alias_expansion") == 0 || strcmp(name, "aliases") == 0) {
        session->features.alias_expansion = enable;
        return true;
    }
    if (strcmp(name, "arithmetic_expansion") == 0) {
        session->features.arithmetic_expansion = enable;
        return true;
    }
    if (strcmp(name, "directory_stack") == 0) {
        session->features.directory_stack = enable;
        return true;
    }
    if (strcmp(name, "history") == 0) {
        session->features.history = enable;
        return true;
    }

    return false;
}

static bool set_option_by_flag(Session *session, char flag, bool enable) {
    switch (flag) {
        case 'a':
            session->options.allexport = enable;
            return true;
        case 'e':
            session->options.errexit = enable;
            return true;
        case 'f':
            session->options.noglob              = enable;
            session->features.filename_expansion = !enable;
            return true;
        case 'u':
            session->options.nounset = enable;
            return true;
        case 'v':
            session->options.verbose = enable;
            return true;
        case 'x':
            session->options.xtrace = enable;
            return true;
        case 'n':
            session->options.noexec = enable;
            return true;
        case 'm':
            session->options.monitor      = enable;
            session->features.job_control = enable;
            return true;
        case 'C':
            session->options.noclobber = enable;
            return true;
        case 'b':
            session->options.notify = enable;
            return true;
        case 'h':
            session->options.hashall = enable;
            return true;
        default:
            return false;
    }
}

static void update_positional_parameters(Session *session, int argc,
                                         char **argv, int pos_start) {
    // 1. Remove all old numeric positional parameters ($1, $2, ...)
    Array *env_array = environ_to_array(session->environ);
    if (env_array) {
        for (size_t k = 0; k < env_array->count; k++) {
            char *eq = strchr(env_array->items[k], '=');
            if (eq) {
                char *key =
                    strndup(env_array->items[k], eq - env_array->items[k]);
                if (key) {
                    if (is_numeric_positional(key)) {
                        environ_remove(session->environ, key);
                    }
                    free(key);
                }
            }
        }
        free_array(env_array);
        free(env_array);
    }

    int count = argc - pos_start;
    if (count < 0) {
        count = 0;
    }

    // 2. Set new positional parameters ($1, $2, ...)
    for (int k = 0; k < count; k++) {
        char num_str[32];
        snprintf(num_str, sizeof(num_str), "%d", k + 1);
        environ_set(session->environ, num_str, argv[pos_start + k]);
    }

    // 3. Set $#
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%d", count);
    environ_set(session->environ, "#", count_str);

    // 4. Set $@ and $*
    Dynamic all_params = {0};
    init_dynamic(&all_params);
    for (int k = 0; k < count; k++) {
        if (k > 0) {
            dynamic_append(&all_params, ' ');
        }
        dynamic_extend(&all_params, argv[pos_start + k]);
    }
    char *all_str = dynamic_to_string(&all_params);
    environ_set(session->environ, "@", all_str ? all_str : "");
    environ_set(session->environ, "*", all_str ? all_str : "");
    if (all_str) {
        free(all_str);
    }
    free_dynamic(&all_params);
}

int builtin_set(int argc, char **argv, Session *session) {
    if (!session || !session->environ) {
        return 1;
    }

    if (argc == 1) {
        return print_all_variables(session);
    }

    bool only_options = true;
    int  pos_start    = -1;
    int  i            = 1;

    while (i < argc) {
        char *arg = argv[i];

        if (strcmp(arg, "--") == 0) {
            only_options = false;
            pos_start    = i + 1;
            break;
        }

        if (strcmp(arg, "-") == 0) {
            // Turn off -v and -x flags
            session->options.verbose = false;
            session->options.xtrace  = false;
            only_options             = false;
            pos_start                = i + 1;
            break;
        }

        if (arg[0] == '-' || arg[0] == '+') {
            bool enable = (arg[0] == '-');

            if (arg[1] == '\0') {
                only_options = false;
                pos_start    = i;
                break;
            }

            // Check for -o or +o
            if (strcmp(arg, "-o") == 0 || strcmp(arg, "+o") == 0) {
                if (i == argc - 1) {
                    // No option name provided as the last argument
                    if (enable) {
                        print_options_status(session);
                    } else {
                        print_options_commands(session);
                    }
                    i++;
                    continue;
                } else {
                    char *opt_name = argv[++i];
                    if (!set_option_by_name(session, opt_name, enable)) {
                        fprintf(stderr, "set: %s: invalid option name\n",
                                opt_name);
                        return 1;
                    }
                    i++;
                    continue;
                }
            }

            // Single-letter flags, possibly combined (e.g. -ex, +ex)
            for (int c = 1; arg[c] != '\0'; c++) {
                char flag = arg[c];
                if (!set_option_by_flag(session, flag, enable)) {
                    fprintf(stderr, "set: %c%c: invalid option\n", arg[0],
                            flag);
                    return 1;
                }
            }
            i++;
        } else {
            // Found first non-option argument
            only_options = false;
            pos_start    = i;
            break;
        }
    }

    if (!only_options && pos_start >= 0) {
        update_positional_parameters(session, argc, argv, pos_start);
    }

    return 0;
}
