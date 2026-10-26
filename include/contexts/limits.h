#ifndef TIDESH_CONTEXTS_LIMITS_H
#define TIDESH_CONTEXTS_LIMITS_H

#include "context.h"
#include "session.h"
#include <stddef.h>

int context_enter_limits(Session *session, ContextArg *args, size_t argc,
                         void **state);
int context_exit_limits(Session *session, int status, void *state);

#endif /* TIDESH_CONTEXTS_LIMITS_H */
