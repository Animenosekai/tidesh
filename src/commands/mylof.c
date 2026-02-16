#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands/mylof.h"
#include "session.h"

int builtin_mylof(int argc, char **argv, Session *session) {
    (void)session;

#if defined(__APPLE__)
    // On macOS, procfs is not available standardly.
    // Use lsof as a fallback/alternative.
    argv[0] = "lsof";
    execvp("lsof", argv);
    fprintf(stderr, "tidesh: command not found: %s\n", "lsof");
    return 127;
#else
    // Linux: Manual procfs parsing for "mylof -p <PID>"
    if (argc < 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: mylof -p <PID>\n");
        return 1;
    }

    char *pid = argv[2];
    char  path[256];
    snprintf(path, sizeof(path), "/proc/%s/fd", pid);

    DIR *dir = opendir(path);
    if (!dir) {
        perror("mylof: opendir");
        return 1;
    }

    printf("%-10s %-10s %s\n", "FD", "TYPE", "OBJECT");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (entry->d_name[0] == '.')
            continue;

        char fd_path[512];
        snprintf(fd_path, sizeof(fd_path), "%s/%s", path, entry->d_name);

        char    target[PATH_MAX];
        ssize_t len = readlink(fd_path, target, sizeof(target) - 1);

        if (len != -1) {
            target[len] = '\0';

            // Simple type heuristic
            char *type = "REG";
            if (strstr(target, "socket:"))
                type = "SOCK";
            else if (strstr(target, "pipe:"))
                type = "PIPE";
            else if (target[0] == '/')
                type = "FILE";
            else if (strstr(target, "/dev/"))
                type = "CHR";

            printf("%-10s %-10s %s\n", entry->d_name, type, target);
        }
    }

    closedir(dir);
    return 0;
#endif
}
