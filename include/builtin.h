/** builtins.h
 *
 * Aggregate header for all builtin command declarations.
 */

#ifndef BUILTINS_H
#define BUILTINS_H

// Builtins
#include "builtins/alias.h"
#include "builtins/cd.h"
#include "builtins/clear.h"
#include "builtins/eval.h"
#include "builtins/exit.h"
#include "builtins/export.h"
#include "builtins/help.h"
#include "builtins/history.h"
#include "builtins/info.h"
#include "builtins/popd.h"
#include "builtins/printenv.h"
#include "builtins/pushd.h"
#include "builtins/pwd.h"
#include "builtins/terminal.h"
#include "builtins/unalias.h"
#include "builtins/which.h"

// Commands (prefixed with "my")
#include "commands/myarp.h"
#include "commands/mydump.h"
#include "commands/myenv.h"
#include "commands/myexe.h"
#include "commands/myinfo.h"
#include "commands/mylof.h"
#include "commands/mymaps.h"
#include "commands/mynetstat.h"
#include "commands/myps.h"
#include "commands/mypstree.h"

/**
 * Get the function pointer for a builtin command by name
 *
 * @param name The name of the builtin command
 * @return Function pointer to the builtin command, or NULL if not found
 */
int (*get_builtin(const char *name))(int argc, char **argv, Session *session);

/**
 * Check if a command name corresponds to a builtin command
 *
 * @param name The command name to check
 * @return true if the command is a builtin, false otherwise
 */
bool is_builtin(const char *name);

/**
 * Check if a command name corresponds to a special builtin command
 * (like cd, exit, export)
 *
 * Special builtins have effects on the shell's state and should be executed
 * in the main shell process rather than a child process.
 *
 * @param name The command name to check
 * @return true if the command is a special builtin, false otherwise
 */
bool is_special_builtin(const char *name);

/* List of builtins */
extern const char *builtins[];

#endif /* BUILTINS_H */
