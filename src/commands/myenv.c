#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#include "commands/myenv.h"

int builtin_myenv(int argc, char **argv, Session *session) {
    (void)session;

    // Vérification des arguments : on attend "-p <PID>"
    if (argc < 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: myenv -p <PID>\n");
        return 1;
    }

#if defined(__APPLE__)
    int    pid      = atoi(argv[2]);
    int    mib[3]   = {CTL_KERN, KERN_PROCARGS2, pid};
    char  *procargs = NULL;
    size_t len      = 0;

    // Get size required
    if (sysctl(mib, 3, NULL, &len, NULL, 0) == -1) {
        perror("myenv: sysctl size");
        return 1;
    }

    procargs = malloc(len);
    if (!procargs) {
        perror("malloc");
        return 1;
    }

    // Get the arguments/environment
    if (sysctl(mib, 3, procargs, &len, NULL, 0) == -1) {
        perror("myenv: sysctl");
        free(procargs);
        return 1;
    }

    // Parse the returned buffer
    // Layout: [argc][exec_path][zero
    // padding][arg0][arg1]...[NULL][env0][env1]... All strings are
    // null-terminated.

    int arg_count;
    memcpy(&arg_count, procargs, sizeof(arg_count));

    char *cp = procargs + sizeof(int);

    // Skip exec_path
    for (; cp < procargs + len && *cp; cp++) {
    }

    // Skip trailing null parsing/alignment
    for (; cp < procargs + len && *cp == '\0'; cp++) {
    }

    if (cp >= procargs + len) {
        free(procargs);
        return 0;
    }

    // Skip arguments
    for (int i = 0; i < arg_count; i++) {
        while (cp < procargs + len && *cp) {
            cp++;
        }
        cp++; // Skip the null terminator
    }

    // Now we are at environment variables
    while (cp < procargs + len && *cp) {
        printf("%s\n", cp);
        while (cp < procargs + len && *cp) {
            cp++;
        }
        cp++; // Skip null
    }

    free(procargs);
    return 0;

#else
    char path[256];
    // Construction du chemin : /proc/PID/environ
    snprintf(path, sizeof(path), "/proc/%s/environ", argv[2]);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("myenv: impossible d'ouvrir le processus");
        return 1;
    }

    char    buffer[1024];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            // Si on rencontre un caractère nul, on le remplace par un retour à
            // la ligne
            if (buffer[i] == '\0') {
                putchar('\n');
            } else {
                putchar(buffer[i]);
            }
        }
    }

    close(fd);
    return 0;
#endif
}