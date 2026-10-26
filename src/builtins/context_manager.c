/** context_manager.c
 *
 * Implementation of the 'context-manager' builtin command.
 */

#include <stdio.h>  /* printf, fprintf, stderr */
#include <stdlib.h> /* free */
#include <string.h> /* strcmp, strdup */

#include "builtins/context_manager.h"
#include "context.h"
#include "data/array.h"
#include "data/trie.h"
#include "session.h"

static void print_usage(void) {
    printf("Usage:\n"
           "  context-manager register <name> --enter <enter_cmd> --exit "
           "<exit_cmd>\n"
           "  context-manager unregister <name>\n"
           "  context-manager list\n"
           "\n"
           "Options:\n"
           "  -e, --enter <cmd>    Command executed upon entering context "
           "(stdout is saved as state)\n"
           "  -x, --exit <cmd>     Command executed upon exiting context "
           "($1=saved state, $2=exit status)\n"
           "  -h, --help           Show this help message\n");
}

static int list_context_managers(Session *session) {
    printf(
        "Built-in context managers:\n"
        "  cwd(path)            Temporarily change directory (restores "
        "original on exit)\n"
        "  env(KEY=VAL, ...)    Temporarily set environment variables "
        "(restores on exit)\n"
        "  options(flags...)    Temporarily alter shell options (restores "
        "on exit)\n"
        "  session              Snapshot entire session state and revert all "
        "changes on exit\n"
        "  umask(mask)          Temporarily change file mode creation mask "
        "(restores on exit)\n"
        "  allowed_exit_codes(codes...) Configure allowed exit codes for "
        "try...catch (default: 0)\n");

    if (session && session->context_managers) {
        printf("\nCustom context managers:\n");
        for (CustomContextManager *mgr = session->context_managers; mgr;
             mgr                       = mgr->next) {
            printf("  %s\n", mgr->name);
            if (mgr->enter_cmd && mgr->enter_cmd[0] != '\0') {
                printf("    enter: %s\n", mgr->enter_cmd);
            }
            if (mgr->exit_cmd && mgr->exit_cmd[0] != '\0') {
                printf("    exit:  %s\n", mgr->exit_cmd);
            }
        }
    }
    return 0;
}

int builtin_context_manager(int argc, char **argv, Session *session) {
    if (!session)
        return 1;

    if (argc <= 1) {
        return list_context_managers(session);
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "ls") == 0) {
        return list_context_managers(session);
    }

    if (strcmp(argv[1], "unregister") == 0 || strcmp(argv[1], "remove") == 0 ||
        strcmp(argv[1], "rm") == 0) {
        if (argc < 3) {
            fprintf(stderr, "context-manager: missing name for unregister\n");
            return 1;
        }
        if (unregister_custom_context_manager(session, argv[2]) != 0) {
            fprintf(stderr, "context-manager: context manager '%s' not found\n",
                    argv[2]);
            return 1;
        }
        return 0;
    }

    const char *name      = NULL;
    const char *enter_cmd = NULL;
    const char *exit_cmd  = NULL;

    int idx = 1;
    if (strcmp(argv[idx], "register") == 0) {
        idx++;
    }

    if (idx < argc && argv[idx][0] != '-') {
        name = argv[idx];
        idx++;
    }

    while (idx < argc) {
        if (strcmp(argv[idx], "--enter") == 0 || strcmp(argv[idx], "-e") == 0) {
            if (idx + 1 >= argc) {
                fprintf(stderr,
                        "context-manager: option '%s' requires an argument\n",
                        argv[idx]);
                return 1;
            }
            enter_cmd = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "--exit") == 0 ||
                   strcmp(argv[idx], "-x") == 0) {
            if (idx + 1 >= argc) {
                fprintf(stderr,
                        "context-manager: option '%s' requires an argument\n",
                        argv[idx]);
                return 1;
            }
            exit_cmd = argv[idx + 1];
            idx += 2;
        } else if (name == NULL && argv[idx][0] != '-') {
            name = argv[idx];
            idx++;
        } else {
            fprintf(stderr, "context-manager: unexpected argument '%s'\n",
                    argv[idx]);
            print_usage();
            return 1;
        }
    }

    if (!name) {
        fprintf(stderr, "context-manager: missing name for context manager\n");
        return 1;
    }

    if (!enter_cmd && !exit_cmd) {
        fprintf(stderr, "context-manager: at least one of --enter or --exit "
                        "must be specified\n");
        return 1;
    }

    if (register_custom_context_manager(session, name,
                                        enter_cmd ? enter_cmd : "",
                                        exit_cmd ? exit_cmd : "") != 0) {
        return 1;
    }

    return 0;
}
