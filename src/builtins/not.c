#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins/not.h"
#include "execute.h" /* execute_string */
#include "session.h" /* Session */

// Note: `not` should be special because the comamnd it runs may be special

int builtin_not(int argc, char **argv, Session *session) {
    if (argc < 2) {
        fprintf(stderr, "%s: missing command\n", argv[0]);
        return 1;
    }

    // Join the arguments into a single command string
    size_t cmd_len = 0;
    for (int i = 1; i < argc; i++) {
        cmd_len += strlen(argv[i]) + 1; // +1 for space or null
    }

    // Create the command string
    char *cmd_str = malloc(cmd_len);
    if (!cmd_str) {
        fprintf(stderr, "%s: memory allocation failed\n", argv[0]);
        return 1;
    }
    cmd_str[0] = '\0';
    for (int i = 1; i < argc; i++) {
        strcat(cmd_str, argv[i]);
        if (i < argc - 1) {
            strcat(cmd_str, " ");
        }
    }

    // Execute the command and return the opposite of its result
    int result = execute_string(cmd_str, session);
    free(cmd_str);
    if (result == 0) {
        return 1; // Command succeeded, so 'not' should return failure
    }
    return 0; // Command failed, so 'not' should return success
}
