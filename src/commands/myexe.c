#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands/myexe.h"
#include "execute.h"
#include "session.h"

int builtin_myexe(int argc, char **argv, Session *session) {
    (void)session;

    // On utilise 'find' combiné à 'file' et 'grep' pour filtrer les vrais ELF
    char command[4096];

    // Initialisation
    strcpy(command, "find");

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            strncat(command, " ", sizeof(command) - strlen(command) - 1);
            strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
        }
    } else {
        // Par défaut: cherche dans le dossier courant et les dossiers standards
        strncat(command, " /bin /usr/bin /sbin /usr/local/bin .",
                sizeof(command) - strlen(command) - 1);
    }

    // Ajout de la detection ELF
    // Les couleurs sont sympas.

#if defined(__APPLE__)
    strncat(command, " -type f -perm +111 -exec file {} + | grep Mach-O",
            sizeof(command) - strlen(command) - 1);
#else
    strncat(command, " -type f -executable -exec file {} + | grep ELF",
            sizeof(command) - strlen(command) - 1);
#endif

    if (session->terminal->supports_colors) {
        strncat(command, " --color=auto",
                sizeof(command) - strlen(command) - 1);
    } else {
        strncat(command, " --color=never",
                sizeof(command) - strlen(command) - 1);
    }

    // On exécute le tout
    return execute_string(command, session);
}
