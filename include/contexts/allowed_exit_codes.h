/** allowed_exit_codes.h
 *
 * Interface for the 'allowed_exit_codes' built-in context manager.
 */

#ifndef CONTEXTS_ALLOWED_EXIT_CODES_H
#define CONTEXTS_ALLOWED_EXIT_CODES_H

#include <stddef.h> /* size_t */

#include "context.h"
#include "session.h"

int context_enter_allowed_exit_codes(Session *session, ContextArg *args,
                                     size_t argc, void **state);
int context_exit_allowed_exit_codes(Session *session, int status, void *state);

#endif /* CONTEXTS_ALLOWED_EXIT_CODES_H */
