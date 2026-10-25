/** command.h
 *
 * Implementation of the command builtin.
 */

#ifndef BUILTIN_COMMAND_H
#define BUILTIN_COMMAND_H

#include "session.h" /* Session */

/**
 * Execute the command builtin command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @param session Pointer to Session
 * @return 0 on success, >0 on failure
 */
int builtin_command(int argc, char **argv, Session *session);

#endif /* BUILTIN_COMMAND_H */
