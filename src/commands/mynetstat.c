#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands/mynetstat.h"
#include "execute.h"
#include "session.h"

int builtin_mynetstat(int argc, char **argv, Session *session) {
    (void)session;

    // Check if invoked as "mynetstat2" (custom variant with lsof)
    if (strcmp(argv[0], "mynetstat2") == 0) {
        // Using lsof instead of netstat for a more consistent output across
        // platforms, especially for the IP:port format.
        const char *prefix_args[] = {"-i", "-n", "-P", NULL};
        return exec_wrapper("lsof", argc, argv, prefix_args);
    }

    // netstat -tunap
#if defined(__APPLE__)
    const char *prefix_args[] = {"-i", "-n", "-P", NULL};
    return exec_wrapper("lsof", argc, argv, prefix_args);
#else
    const char *prefix_args[] = {"-t", "-u", "-n", "-a", "-p", NULL};
    return exec_wrapper("netstat", argc, argv, prefix_args);
#endif
}
