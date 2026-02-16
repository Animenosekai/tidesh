#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "commands/mypstree.h"
#include "session.h"

int builtin_mypstree(int argc, char **argv, Session *session) {
    (void)session;
    // Sujet: mypstree doit lancer "pstree -p"
    // On construit une nouvelle liste d'arguments avec "-p" forcé

    // On alloue assez d'espace pour argv + le flag "-p" + NULL
    char **new_argv = malloc((argc + 2) * sizeof(char *));
    if (!new_argv) {
        perror("malloc");
        return 1;
    }

    new_argv[0] = "pstree";
    new_argv[1] = "-p";

    // Copie les autres arguments (à partir de argv[1])
    for (int i = 1; i < argc; i++) {
        new_argv[i + 1] = argv[i];
    }
    new_argv[argc + 1] = NULL;

    execvp("pstree", new_argv);

    // If we are here, pstree failed to execute
    if (errno == ENOENT) {
#if defined(__APPLE__)
        if (session->terminal->supports_colors) {
            fprintf(stderr, "\033[1;34m!\033[0m \033[1;1mpstree\033[0m command "
                            "not found.\n");
            fprintf(stderr,
                    "\033[90m  Falling back to \033[1;90mps -axjf\033[0;90m. "
                    "\033[1;90mpstree\033[0;90m can be "
                    "installed via Homebrew.\n\033[0m");
        } else {
            fprintf(stderr, "Warning: 'pstree' command not found.\n");
            fprintf(stderr,
                    "Tip: You can allow installing simple ecosystem tools "
                    "or use 'brew install pstree'\n");
            fprintf(stderr, "Fallback: Using 'ps -axjf' for tree view:\n");
        }

        char *ps_argv[] = {"ps", "-axjf", NULL};
        execvp("ps", ps_argv);
#else
        fprintf(stderr, "mypstree: 'pstree' command not found. Please install "
                        "it (usually in psmisc package).\n");
#endif
    } else {
        perror("mypstree");
    }

    return 127;
}
