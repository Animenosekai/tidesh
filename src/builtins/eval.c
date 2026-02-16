#include <stdlib.h>
#include <string.h>

#include "builtins/eval.h"
#include "execute.h"
#include "session.h"

int builtin_eval(int argc, char **argv, Session *session) {
    if (argc < 2) {
        return 0;
    }

    size_t total_len = 0;
    for (int i = 1; i < argc; i++) {
        total_len += strlen(argv[i]) + 1; // +1 for space or null terminator
    }

    char *cmd = malloc(total_len);
    if (!cmd) {
        return 1;
    }

    cmd[0] = '\0';
    for (int i = 1; i < argc; i++) {
        strcat(cmd, argv[i]);
        if (i < argc - 1) {
            strcat(cmd, " ");
        }
    }

    // Temporarily disable history to avoid double logging
    bool was_disabled          = session->history->disabled;
    session->history->disabled = true;

    int status = execute_string(cmd, session);

    session->history->disabled = was_disabled;
    free(cmd);
    return status;
}
