#include <errno.h>    /* errno */
#include <signal.h>   /* kill, SIGCONT */
#include <stdio.h>    /* fprintf, stderr, printf */
#include <stdlib.h>   /* atoi */
#include <string.h>   /* strcmp, strerror */
#include <sys/wait.h> /* waitpid, WIFEXITED, WIFSIGNALED, WIFSTOPPED */
#include <unistd.h>   /* tcsetpgrp, STDIN_FILENO */

#include "builtins/fg.h"
#include "environ.h"
#include "jobs.h"
#include "session.h"

#ifndef TIDESH_DISABLE_JOB_CONTROL

int builtin_fg(int argc, char **argv, Session *session) {
    if (!session->features.job_control) {
        fprintf(stderr, "tidesh: job control not enabled\n");
        return 127;
    }
    if (!session || !session->jobs) {
        return 1;
    }

    Job *job = NULL;

    // Determine which job to bring to foreground
    if (argc == 1) {
        // No argument - use current job
        job = jobs_get_current(session->jobs);
        if (!job) {
            fprintf(stderr, "fg: no current job\n");
            return 1;
        }
    } else {
        // Parse job ID from argument (handle %1, %%, %+ formats)
        const char *arg = argv[1];
        int         job_id;

        if (arg[0] == '%') {
            arg++; // Skip %
            if (*arg == '\0' || strcmp(arg, "+") == 0) {
                // %% or %+ - current job
                job = jobs_get_current(session->jobs);
            } else if (strcmp(arg, "-") == 0) {
                // %- - previous job
                job = jobs_get_previous(session->jobs);
            } else {
                // %N - job number
                job_id = atoi(arg);
                job    = jobs_get(session->jobs, job_id);
            }
        } else {
            // Plain number
            job_id = atoi(argv[1]);
            job    = jobs_get(session->jobs, job_id);
        }

        if (!job) {
            fprintf(stderr, "fg: job not found: %s\n", argv[1]);
            return 1;
        }
    }

    // Print job being resumed
    printf("%s\n", job->command ? job->command : "");

    // If stopped, send SIGCONT to resume
    if (job->state == JOB_STOPPED) {
        if (kill(job->pid, SIGCONT) < 0) {
            fprintf(stderr, "fg: failed to continue job: %s\n",
                    strerror(errno));
            return 1;
        }
        job->state = JOB_RUNNING;
    }

    // Give terminal control to the job
    tcsetpgrp(STDIN_FILENO, job->pid);

    // Wait for the job to complete or stop
    int status;
    waitpid(job->pid, &status, WUNTRACED);

    // Take back terminal control
    tcsetpgrp(STDIN_FILENO, session->jobs->pgid);

    // Update job state
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        environ_set_exit_status(session->environ, exit_code);
        jobs_remove(session->jobs, job->id);
        return exit_code;
    } else if (WIFSIGNALED(status)) {
        int sig_num = WTERMSIG(status);
        environ_set_exit_status(session->environ, 128 + sig_num);
        jobs_remove(session->jobs, job->id);
        return 128 + sig_num;
    } else if (WIFSTOPPED(status)) {
        job->state = JOB_STOPPED;
        printf("\n[%d]+  Stopped\t\t%s\n", job->id,
               job->command ? job->command : "");
        environ_set_exit_status(session->environ, 148); // SIGTSTP
        return 148;
    }

    return 0;
}

#endif
