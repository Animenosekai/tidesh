#ifndef TIDESH_CONTEXTS_PIDS_H
#define TIDESH_CONTEXTS_PIDS_H

#include "context.h"
#include "session.h"
#include <stddef.h>

int context_enter_pids(Session *session, ContextArg *args, size_t argc,
                       void **state);
int context_exit_pids(Session *session, int status, void *state);

#endif /* TIDESH_CONTEXTS_PIDS_H */
