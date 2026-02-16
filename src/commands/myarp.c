#include <stdio.h>
#include <stdlib.h>

#include "commands/myarp.h"
#include "execute.h"
#include "session.h"

int builtin_myarp(int argc, char **argv, Session *session) {
    (void)session;
#if defined(__APPLE__)
    // On macOS, `arp -n` is not a full command. Use `arp -a -n`.
    const char *prefix_args[] = {"-a", "-n", NULL};
#else
    const char *prefix_args[] = {"-n", NULL};
#endif
    return exec_wrapper("arp", argc, argv, prefix_args);
}
