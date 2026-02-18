/** type.h
 *
 * Declarations for the 'type' builtin command.
 */

#ifndef BUILTIN_TYPE_H
#define BUILTIN_TYPE_H

#include "session.h"

/**
 * The type builtin command.
 *
 * Shows the type of a command (alias, builtin, special builtin, or external).
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return The exit status (0 if all found, 1 if any not found)
 */
int builtin_type(int argc, char **argv, Session *session);

#endif /* BUILTIN_TYPE_H */
