#include <stdio.h>
#include <stdlib.h>

#include "builtins/type.h"
#include "execute.h" /* get_command_info, CommandInfo, COMMAND_* */
#include "session.h" /* Session */

int builtin_type(int argc, char **argv, Session *session) {
    if (argc < 2) {
        fprintf(stderr, "type: missing operand\n");
        return 1;
    }

    int exit_status = 0;
    for (int i = 1; i < argc; i++) {
        char       *cmd_name = argv[i];
        CommandInfo info     = get_command_info(cmd_name, session);

        switch (info.type) {
            case COMMAND_ALIAS:
                printf("%s is aliased to `%s'\n", cmd_name, info.path);
                break;
            case COMMAND_SPECIAL_BUILTIN:
                printf("%s is a shell special built-in command\n", cmd_name);
                break;
            case COMMAND_BUILTIN:
                printf("%s is a shell built-in command\n", cmd_name);
                break;
            case COMMAND_EXTERNAL:
                printf("%s is %s\n", cmd_name, info.path);
                break;
            case COMMAND_NOT_FOUND:
            default:
                fprintf(stderr, "type: %s: not found\n", cmd_name);
                exit_status = 1;
                break;
        }

        if (info.path) {
            free(info.path);
        }
    }
    return exit_status;
}
