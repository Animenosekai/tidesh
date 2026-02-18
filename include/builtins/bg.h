/** bg.h
 *
 * Declarations for the 'bg' builtin command.
 */

#ifndef BUILTIN_BG_H
#define BUILTIN_BG_H

#include "session.h"

/**
 * The bg builtin command.
 *
 * Continues a stopped job in the background.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return The exit status (0 on success)
 */
int builtin_bg(int argc, char **argv, Session *session);

#endif /* BUILTIN_BG_H */
