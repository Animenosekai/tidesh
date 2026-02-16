/** terminal.h
 *
 * Declarations for the terminal builtin command.
 */

#ifndef BUILTIN_TERMINAL_H
#define BUILTIN_TERMINAL_H

#include "session.h" /* Session */

/**
 * Builtin 'terminal' command.
 * Displays terminal information and manages terminal settings.
 *
 * @param argc Number of arguments
 * @param argv Argument array
 * @param session Pointer to Session
 * @return Exit status
 */
int builtin_terminal(int argc, char **argv, Session *session);

#endif /* BUILTIN_TERMINAL_H */
