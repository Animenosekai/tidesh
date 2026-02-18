#include <stdbool.h> /* bool */
#include <stdio.h>   /* printf, fprintf */
#include <stdlib.h>  /* free */

#include "builtin.h" /* get_builtin */
#include "builtins/which.h"
#include "execute.h" /* get_command_info, CommandInfo, COMMAND_* */
#include "session.h" /* Session */

int builtin_which(int argc, char **argv, Session *session) {
    if (argc < 2) {
        fprintf(stderr, "which: missing operand\n");
        return 1;
    }

    int exit_status = 0;
    for (int i = 1; i < argc; i++) {
        char       *cmd_name = argv[i];
        CommandInfo info     = get_command_info(cmd_name, session);

        switch (info.type) {
            case COMMAND_ALIAS:
                printf("%s: aliased to %s\n", cmd_name, info.path);
                break;
            case COMMAND_SPECIAL_BUILTIN:
                printf("%s: shell special built-in command\n", cmd_name);
                break;
            case COMMAND_BUILTIN:
                printf("%s: shell built-in command\n", cmd_name);
                break;
            case COMMAND_EXTERNAL:
                printf("%s\n", info.path);
                break;
            case COMMAND_NOT_FOUND:
            default:
                printf("%s: not found\n", cmd_name);
                exit_status = 1;
                break;
        }

        if (info.path) {
            free(info.path);
        }
    }
    return exit_status;
}
