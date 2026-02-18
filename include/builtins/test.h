/** test.h
 *
 * Declarations for the 'test' and '[' builtin commands.
 */

#ifndef BUILTIN_TEST_H
#define BUILTIN_TEST_H

#include "session.h"

/**
 * The test builtin command.
 *
 * Evaluates conditional expressions and returns 0 for true, 1 for false.
 * Supports string, numeric, and file tests.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return 0 if the expression is true, 1 if false, 2 on error
 */
int builtin_test(int argc, char **argv, Session *session);

#endif /* BUILTIN_TEST_H */
