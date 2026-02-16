#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands/myps.h"
#include "execute.h"
#include "session.h"

int builtin_myps(int argc, char **argv, Session *session) {
    (void)session;

    // We need to capture the output of 'ps' to calculate the sum of VSZ and
    // RSS. Pipe creation
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return 1;
    }

    if (pid == 0) {
        // Child: exec 'ps' outputting to pipe
        close(pipe_fds[0]); // Close read end
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[1]);

        // ps -o pid,ppid,uid,vsz,rss,command
        // Note: VSZ is usually in KB, RSS in KB (on Linux/macOS standard ps
        // options)
        const char *ps_argv[] = {"ps", "-o", "pid,ppid,uid,vsz,rss,command",
                                 NULL};

        execvp("ps", (char *const *)ps_argv);
        perror("myps: execvp");
        exit(1);
    }

    // Parent: Read pipe, print lines, parse VSZ/RSS, sum them up
    close(pipe_fds[1]); // Close write end

    FILE *fp = fdopen(pipe_fds[0], "r");
    if (!fp) {
        perror("fdopen");
        waitpid(pid, NULL, 0);
        return 1;
    }

    char line[4096];
    long total_vsz = 0;
    long total_rss = 0;
    int  is_header = 1;

    // Read line by line
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line); // Print the line exactly as is

        if (is_header) {
            is_header = 0;
            continue;
        }

        // Parse VSZ (col 4) and RSS (col 5)

        // Tokenize just to extract the numbers
        char line_copy[4096];
        strncpy(line_copy, line, sizeof(line_copy) - 1);

        // Skip PID
        char *token = strtok(line_copy, " \t\n");
        if (!token)
            continue;

        // Skip PPID
        token = strtok(NULL, " \t\n");
        if (!token)
            continue;

        // Skip UID
        token = strtok(NULL, " \t\n");
        if (!token)
            continue;

        // VSZ
        token = strtok(NULL, " \t\n");
        if (token) {
            total_vsz += strtol(token, NULL, 10);
        }

        // RSS
        token = strtok(NULL, " \t\n");
        if (token) {
            total_rss += strtol(token, NULL, 10);
        }
    }

    fclose(fp);
    int status;
    waitpid(pid, &status, 0);

    // Print the sum line
    printf("Total VSZ: %ld KB | Total RSS: %ld KB\n", total_vsz, total_rss);

    return WEXITSTATUS(status);
}
