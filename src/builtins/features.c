/** features.c
 *
 * Builtin command to manage runtime feature flags.
 */

#include <stddef.h> /* offsetof */
#include <stdio.h>  /* printf, fprintf */
#include <string.h> /* strcmp */

#include "builtins/features.h"
#include "features.h"
#include "session.h"

#ifdef TIDESH_DISABLE_EXPANSIONS
#define FEATURE_DISABLED_EXPANSIONS true
#else
#define FEATURE_DISABLED_EXPANSIONS false
#endif

#ifdef TIDESH_DISABLE_ALIASES
#define FEATURE_DISABLED_ALIASES true
#else
#define FEATURE_DISABLED_ALIASES false
#endif

#ifdef TIDESH_DISABLE_JOB_CONTROL
#define FEATURE_DISABLED_JOB_CONTROL true
#else
#define FEATURE_DISABLED_JOB_CONTROL false
#endif

#ifdef TIDESH_DISABLE_HISTORY
#define FEATURE_DISABLED_HISTORY true
#else
#define FEATURE_DISABLED_HISTORY false
#endif

#ifdef TIDESH_DISABLE_DIRSTACK
#define FEATURE_DISABLED_DIRSTACK true
#else
#define FEATURE_DISABLED_DIRSTACK false
#endif

#ifdef TIDESH_DISABLE_PIPES
#define FEATURE_DISABLED_PIPES true
#else
#define FEATURE_DISABLED_PIPES false
#endif

#ifdef TIDESH_DISABLE_REDIRECTIONS
#define FEATURE_DISABLED_REDIRECTIONS true
#else
#define FEATURE_DISABLED_REDIRECTIONS false
#endif

#ifdef TIDESH_DISABLE_SEQUENCES
#define FEATURE_DISABLED_SEQUENCES true
#else
#define FEATURE_DISABLED_SEQUENCES false
#endif

#ifdef TIDESH_DISABLE_SUBSHELLS
#define FEATURE_DISABLED_SUBSHELLS true
#else
#define FEATURE_DISABLED_SUBSHELLS false
#endif

#ifdef TIDESH_DISABLE_COMMAND_SUBSTITUTION
#define FEATURE_DISABLED_COMMAND_SUBSTITUTION true
#else
#define FEATURE_DISABLED_COMMAND_SUBSTITUTION false
#endif

#ifdef TIDESH_DISABLE_ASSIGNMENTS
#define FEATURE_DISABLED_ASSIGNMENTS true
#else
#define FEATURE_DISABLED_ASSIGNMENTS false
#endif

typedef struct FeatureDef {
    const char *name;
    const char *description;
    size_t      offset;
    bool        compile_time_disabled;
} FeatureDef;

static const FeatureDef feature_defs[] = {
    {"variable_expansion", "$VAR, ${VAR}",
     offsetof(Features, variable_expansion), FEATURE_DISABLED_EXPANSIONS},
    {"tilde_expansion", "~ and ~user", offsetof(Features, tilde_expansion),
     FEATURE_DISABLED_EXPANSIONS},
    {"brace_expansion", "{a,b,c} and {1..10}",
     offsetof(Features, brace_expansion), FEATURE_DISABLED_EXPANSIONS},
    {"filename_expansion", "globbing (*, ?, [...])",
     offsetof(Features, filename_expansion), FEATURE_DISABLED_EXPANSIONS},
    {"alias_expansion", "alias substitution",
     offsetof(Features, alias_expansion), FEATURE_DISABLED_ALIASES},
    {"job_control", "bg/fg/jobs", offsetof(Features, job_control),
     FEATURE_DISABLED_JOB_CONTROL},
    {"history", "command history", offsetof(Features, history),
     FEATURE_DISABLED_HISTORY},
    {"directory_stack", "pushd/popd/dirs", offsetof(Features, directory_stack),
     FEATURE_DISABLED_DIRSTACK},
    {"prompt_expansion", "prompt customization",
     offsetof(Features, prompt_expansion), false},
    {"completion", "tab completion", offsetof(Features, completion), false},
    {"pipes", "pipe operator |", offsetof(Features, pipes),
     FEATURE_DISABLED_PIPES},
    {"redirections", "redirections >, <, >>", offsetof(Features, redirections),
     FEATURE_DISABLED_REDIRECTIONS},
    {"sequences", ";, &&, ||", offsetof(Features, sequences),
     FEATURE_DISABLED_SEQUENCES},
    {"subshells", "( ... )", offsetof(Features, subshells),
     FEATURE_DISABLED_SUBSHELLS},
    {"command_substitution", "$(...) and <(...)",
     offsetof(Features, command_substitution),
     FEATURE_DISABLED_COMMAND_SUBSTITUTION},
    {"assignments", "VAR=value", offsetof(Features, assignments),
     FEATURE_DISABLED_ASSIGNMENTS},
};

static size_t feature_defs_count(void) {
    return sizeof(feature_defs) / sizeof(feature_defs[0]);
}

static bool *feature_flag_ptr(Session *session, const FeatureDef *def) {
    return (bool *)((char *)&session->features + def->offset);
}

static const FeatureDef *find_feature(const char *name) {
    for (size_t i = 0; i < feature_defs_count(); i++) {
        if (strcmp(feature_defs[i].name, name) == 0) {
            return &feature_defs[i];
        }
    }
    return NULL;
}

static void print_usage(void) {
    fprintf(
        stderr,
        "Usage: features [list|status|enable|disable] [name|all|expansions]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "  (none) or list     List all features and status\n");
    fprintf(stderr, "  status [name]      Show feature status\n");
    fprintf(stderr, "  enable <name|all>  Enable a feature or all features\n");
    fprintf(stderr, "  disable <name|all> Disable a feature or all features\n");
    fprintf(stderr,
            "  enable expansions  Enable variable/tilde/brace/filename\n");
    fprintf(stderr,
            "  disable expansions Disable variable/tilde/brace/filename\n");
}

static void list_features(Session *session) {
    for (size_t i = 0; i < feature_defs_count(); i++) {
        const FeatureDef *def = &feature_defs[i];
        bool              on  = *feature_flag_ptr(session, def);
        printf("  %-20s %s%s\n", def->name, on ? "enabled" : "disabled",
               def->compile_time_disabled ? " (compile-time)" : "");
    }
}

static int set_all_features(Session *session, bool enabled) {
    for (size_t i = 0; i < feature_defs_count(); i++) {
        const FeatureDef *def  = &feature_defs[i];
        bool             *flag = feature_flag_ptr(session, def);
        if (enabled && def->compile_time_disabled) {
            *flag = false;
        } else {
            *flag = enabled;
        }
    }

    features_apply_compile_time_disables(&session->features);
    return 0;
}

static int set_expansions(Session *session, bool enabled) {
    if (enabled) {
        features_enable_all_expansions(&session->features);
    } else {
        features_disable_all_expansions(&session->features);
    }

    features_apply_compile_time_disables(&session->features);
    return 0;
}

static int set_feature(Session *session, const char *name, bool enabled) {
    const FeatureDef *def = find_feature(name);
    if (!def) {
        fprintf(stderr, "features: unknown feature '%s'\n", name);
        return 1;
    }
    if (enabled && def->compile_time_disabled) {
        fprintf(stderr, "features: '%s' disabled at compile time\n", name);
        return 1;
    }

    *feature_flag_ptr(session, def) = enabled;
    return 0;
}

static int print_feature_status(Session *session, const char *name) {
    const FeatureDef *def = find_feature(name);
    if (!def) {
        fprintf(stderr, "features: unknown feature '%s'\n", name);
        return 1;
    }

    printf("%s\n", *feature_flag_ptr(session, def) ? "enabled" : "disabled");
    return 0;
}

int builtin_features(int argc, char **argv, Session *session) {
    if (!session) {
        return 1;
    }

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "list") == 0)) {
        list_features(session);
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        if (argc == 2) {
            list_features(session);
            return 0;
        }
        return print_feature_status(session, argv[2]);
    }

    if (strcmp(argv[1], "enable") == 0 || strcmp(argv[1], "disable") == 0) {
        if (argc < 3) {
            print_usage();
            return 1;
        }

        bool enable = strcmp(argv[1], "enable") == 0;

        if (strcmp(argv[2], "all") == 0) {
            return set_all_features(session, enable);
        }

        if (strcmp(argv[2], "expansions") == 0) {
            return set_expansions(session, enable);
        }

        return set_feature(session, argv[2], enable);
    }

    print_usage();
    return 1;
}
