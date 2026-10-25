/** exec.h
 *
 * Builtin command to replace shell process image or modify file descriptor
 * redirections.
 */

#ifndef BUILTIN_EXEC_H
#define BUILTIN_EXEC_H

#include "session.h" /* Session */

/**
 * Execute the exec builtin command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @param session Pointer to Session
 * @return 0 on success (when redirecting shell), 126 or 127 on failure, 2 on
 * option error
 */
int builtin_exec(int argc, char **argv, Session *session);

#endif /* BUILTIN_EXEC_H */
