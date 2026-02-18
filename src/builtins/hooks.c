#include <dirent.h>   /* DIR, opendir, readdir, closedir */
#include <limits.h>   /* PATH_MAX */
#include <stdio.h>    /* printf, fprintf, snprintf */
#include <string.h>   /* strcmp, strrchr, strlen */
#include <sys/stat.h> /* stat, S_ISREG */

#include "builtins/hooks.h"
#include "hooks.h"   /* run_cwd_hook, HOOK_* */
#include "session.h" /* Session */

static const char *hook_types[] = {HOOK_ALL,
                                   HOOK_ENTER,
                                   HOOK_EXIT,
                                   HOOK_ENTER_CHILD,
                                   HOOK_EXIT_CHILD,
                                   HOOK_BEFORE_CMD,
                                   HOOK_AFTER_CMD,
                                   HOOK_BEFORE_PROMPT,
                                   HOOK_AFTER_PROMPT,
                                   HOOK_ERROR,
                                   HOOK_BEFORE_EXEC,
                                   HOOK_AFTER_EXEC,
                                   HOOK_ENTER_SUBSHELL,
                                   HOOK_EXIT_SUBSHELL,
                                   HOOK_ADD_ENVIRON,
                                   HOOK_REMOVE_ENVIRON,
                                   HOOK_CHANGE_ENVIRON,
                                   HOOK_CD,
                                   HOOK_CMD_NOT_FOUND,
                                   HOOK_ADD_ALIAS,
                                   HOOK_REMOVE_ALIAS,
                                   HOOK_CHANGE_ALIAS,
                                   HOOK_SIGNAL,
                                   HOOK_BEFORE_JOB,
                                   HOOK_AFTER_JOB,
                                   HOOK_SYNTAX_ERROR,
                                   HOOK_SESSION_START,
                                   HOOK_SESSION_END,
                                   HOOK_BEFORE_RC,
                                   NULL};

static void print_hook_types(void) {
    printf("Available hook types:\n");
    for (int i = 0; hook_types[i] != NULL; i++) {
        printf("  %s\n", hook_types[i]);
    }
}

bool is_valid_hook_type(const char *hook_name) {
    for (int i = 0; hook_types[i] != NULL; i++) {
        if (strcmp(hook_name, hook_types[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void list_hook_files(const char *dir) {
    if (!dir) {
        fprintf(stderr, "hooks: invalid directory\n");
        return;
    }

    char hooks_dir[PATH_MAX];
    int  written =
        snprintf(hooks_dir, sizeof(hooks_dir), "%s/.tidesh-hooks", dir);
    if (written <= 0 || (size_t)written >= sizeof(hooks_dir)) {
        fprintf(stderr, "hooks: path too long\n");
        return;
    }

    DIR *dir_handle = opendir(hooks_dir);
    if (!dir_handle) {
        printf("No hooks directory found (.tidesh-hooks)\n");
        return;
    }

    printf("Hook files in %s:\n", hooks_dir);

    int            count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir_handle)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        written =
            snprintf(full_path, sizeof(full_path), "%s/%s", hooks_dir, name);
        if (written <= 0 || (size_t)written >= sizeof(full_path))
            continue;

        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        printf("  %s", name);

        // Show if executable
        if (st.st_mode & S_IXUSR) {
            printf(" (executable)");
        }
        printf("\n");
        count++;
    }

    closedir(dir_handle);

    if (count == 0) {
        printf("  (no hook files found)\n");
    }
}

static void print_usage(void) {
    fprintf(stderr, "Usage: hooks [enable|disable|status|list|run "
                    "<hook_name>|path|types]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "  (none) or list  List available hook files\n");
    fprintf(stderr, "  enable          Enable hook execution\n");
    fprintf(stderr, "  disable         Disable hook execution\n");
    fprintf(stderr, "  status          Show hook execution status\n");
    fprintf(stderr, "  run <name>      Manually run a specific hook\n");
    fprintf(stderr, "  path            Show the hooks directory path\n");
    fprintf(stderr, "  types           List all available hook types\n");
}

int builtin_hooks(int argc, char **argv, Session *session) {
    if (!session) {
        return 1;
    }

    // No arguments or "list" command - list hook files
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "list") == 0)) {
        list_hook_files(session->current_working_dir);
        return 0;
    }

    // Parse subcommands
    if (strcmp(argv[1], "enable") == 0) {
        session->hooks_disabled = false;
        return 0;
    }

    if (strcmp(argv[1], "disable") == 0) {
        session->hooks_disabled = true;
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        printf("%s\n", session->hooks_disabled ? "disabled" : "enabled");
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "hooks: run requires a hook name\n");
            fprintf(stderr, "Usage: hooks run <hook_name>\n");
            return 1;
        }

        if (session->hooks_disabled) {
            fprintf(stderr, "hooks: cannot run hook - hooks are disabled\n");
            fprintf(stderr, "Use 'hooks enable' to enable hooks\n");
            return 1;
        }

        // Run the specified hook
        const char *hook_name = argv[2];
        if (!is_valid_hook_type(hook_name)) {
            fprintf(stderr, "hooks: invalid hook name: %s\n", hook_name);
            return 1;
        }
        run_cwd_hook(session, hook_name);
        return 0;
    }

    if (strcmp(argv[1], "path") == 0) {
        if (!session->current_working_dir) {
            fprintf(stderr, "hooks: cannot determine current directory\n");
            return 1;
        }
        printf("%s/.tidesh-hooks\n", session->current_working_dir);
        return 0;
    }

    if (strcmp(argv[1], "types") == 0) {
        print_hook_types();
        return 0;
    }

    if (strcmp(argv[1], "help") == 0) {
        print_usage();
        return 0;
    }

    // Unknown subcommand
    fprintf(stderr, "hooks: unknown subcommand: %s\n", argv[1]);
    return 1;
}
