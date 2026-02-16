#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands/mydelexe.h"
#include "execute.h"

#if !defined(__APPLE__)
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

// Check if a string is purely numeric
static int is_numeric(const char *str) {
    while (*str) {
        if (*str < '0' || *str > '9')
            return 0;
        str++;
    }
    return 1;
}
#endif

int builtin_mydelexe(int argc, char **argv, Session *session) {
    (void)argc;
    (void)argv;
    (void)session;

#if defined(__APPLE__)
    // macOS implementation utilizing lsof -F to parse machine-readable output
    // -n: No host name lookup
    // -P: No port name lookup
    // -F pkcn: Output PID, Link count, Command, Name
    // +L1: Select files with link count < 1
    // -d txt: Select text segments (code)
    char *lsof_cmd = "lsof -n -P -F pkcn +L1 -d txt";
    char *output   = execute_string_stdout(lsof_cmd, session);

    if (!output) {
        // lsof returns 1 if no files are found with +L1, which is fine.
        printf("No fileless processes detected.\n");
        return 0;
    }

    printf("%-8s %-20s %s\n", "PID", "NAME", "PATH (DELETED)");
    printf("------------------------------------------------------------\n");

    char *current_pid  = NULL;
    char *current_cmd  = NULL;
    char *current_name = NULL;
    int   current_link = -1;

    // Tokenize by newline.
    // Note: execute_string_stdout returns a malloc'd string that we can modify.
    char *line = strtok(output, "\n");
    while (line) {
        char  type  = line[0];
        char *value = line + 1;

        if (type == 'p') {
            // New process - Check if we have a pending file from previous
            // process
            if (current_link == 0 && current_name && current_pid &&
                current_cmd) {
                printf("%-8s %-20s %s\n", current_pid, current_cmd,
                       current_name);
            }
            // Reset per-process
            current_pid = value;
            current_cmd = NULL;
            // Reset per-file
            current_name = NULL;
            current_link = -1;
        } else if (type == 'c') {
            current_cmd = value;
        } else if (type == 'f') {
            // New file - Check pending previous file
            if (current_link == 0 && current_name && current_pid &&
                current_cmd) {
                printf("%-8s %-20s %s\n", current_pid, current_cmd,
                       current_name);
            }
            // Reset per-file
            current_name = NULL;
            current_link = -1;
        } else if (type == 'k') {
            current_link = atoi(value);
        } else if (type == 'n') {
            current_name = value;
        }

        line = strtok(NULL, "\n");
    }

    // Check last file
    if (current_link == 0 && current_name && current_pid && current_cmd) {
        printf("%-8s %-20s %s\n", current_pid, current_cmd, current_name);
    }

    free(output);
    return 0;

#else
    // Linux implementation scraping /proc
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("mydelexe: opendir /proc");
        return 1;
    }

    printf("%-8s %-20s %s\n", "PID", "NAME", "PATH (DELETED)");
    printf("------------------------------------------------------------\n");

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        // We only care about PIDs (numeric directories)
        if (!is_numeric(entry->d_name))
            continue;

        char exe_path[PATH_MAX];
        snprintf(exe_path, sizeof(exe_path), "/proc/%s/exe", entry->d_name);

        char    target_path[PATH_MAX];
        ssize_t len = readlink(exe_path, target_path, sizeof(target_path) - 1);

        if (len != -1) {
            target_path[len] = '\0';
            // Check if it ends with " (deleted)"
            if (strstr(target_path, " (deleted)")) {
                // Get process name from /proc/PID/comm for nicer display
                char comm_path[PATH_MAX];
                char comm_name[256] = "unknown";
                snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm",
                         entry->d_name);
                FILE *f = fopen(comm_path, "r");
                if (f) {
                    if (fgets(comm_name, sizeof(comm_name), f)) {
                        // Remove newline
                        size_t c_len = strlen(comm_name);
                        if (c_len > 0 && comm_name[c_len - 1] == '\n')
                            comm_name[c_len - 1] = '\0';
                    }
                    fclose(f);
                }

                printf("%-8s %-20s %s\n", entry->d_name, comm_name,
                       target_path);
            }
        }
    }

    closedir(proc_dir);
    return 0;
#endif
}
