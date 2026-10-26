/** lookup.h
 *
 * Command lookup, PATH resolution, and shebang handling.
 */

#ifndef EXECUTE_LOOKUP_H
#define EXECUTE_LOOKUP_H

#include <stdbool.h> /* bool */

#include "session.h"

/**
 * The type of a command.
 */
typedef enum CommandType {
    COMMAND_NOT_FOUND,
    COMMAND_ALIAS,
    COMMAND_BUILTIN,
    COMMAND_SPECIAL_BUILTIN,
    COMMAND_EXTERNAL,
    COMMAND_FUNCTION
} CommandType;

/**
 * Detailed information about a command.
 */
typedef struct CommandInfo {
    CommandType type;
    char       *path;
} CommandInfo;

char *trim_whitespace(const char *str);
char *extract_first_word(const char *str);

bool  has_shebang(const char *path);
bool  parse_shebang(const char *path, int *interp_argc, char ***interp_argv);
char *find_in_path(const char *cmd, Session *session);
CommandInfo get_command_info(const char *cmd, Session *session);

#endif /* EXECUTE_LOOKUP_H */
