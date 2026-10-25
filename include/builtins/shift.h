/** shift.h
 *
 * Declarations for the shift builtin command.
 */

#ifndef BUILTINS_SHIFT_H
#define BUILTINS_SHIFT_H

#include "session.h"

/**
 * builtin_shift - Shift positional parameters.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param session Current shell session
 * @return 0 on success, non-zero on failure
 */
int builtin_shift(int argc, char **argv, Session *session);

#endif /* BUILTINS_SHIFT_H */
