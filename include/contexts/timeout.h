#ifndef TIDESH_CONTEXTS_TIMEOUT_H
#define TIDESH_CONTEXTS_TIMEOUT_H

#include "context.h"
#include "session.h"
#include <stddef.h>

int context_enter_timeout(Session *session, ContextArg *args, size_t argc,
                          void **state);
int context_exit_timeout(Session *session, int status, void *state);

#endif /* TIDESH_CONTEXTS_TIMEOUT_H */
