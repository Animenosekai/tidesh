#include <errno.h>  /* errno */
#include <signal.h> /* kill, SIGCONT */
#include <stdio.h>  /* fprintf, stderr, printf */
#include <stdlib.h> /* atoi */
#include <string.h> /* strcmp, strerror */

#include "builtins/bg.h"
#include "jobs.h"
#include "session.h"

#ifndef TIDESH_DISABLE_JOB_CONTROL

int builtin_bg(int argc, char **argv, Session *session) {
    if (!session->features.job_control) {
        fprintf(stderr, "tidesh: job control not enabled\n");
        return 127;
    }
    if (!session || !session->jobs) {
        return 1;
    }

    Job *job = NULL;

    // Determine which job to continue in background
    if (argc == 1) {
        // No argument - use current job
        job = jobs_get_current(session->jobs);
        if (!job) {
            fprintf(stderr, "bg: no current job\n");
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
            fprintf(stderr, "bg: job not found: %s\n", argv[1]);
            return 1;
        }
    }

    // Check if job is stopped
    if (job->state != JOB_STOPPED) {
        fprintf(stderr, "bg: job is already running\n");
        return 1;
    }

    // Send SIGCONT to resume the job
    if (kill(job->pid, SIGCONT) < 0) {
        fprintf(stderr, "bg: failed to continue job: %s\n", strerror(errno));
        return 1;
    }

    // Update job state
    job->state = JOB_RUNNING;

    // Print job being resumed
    printf("[%d]+ %s &\n", job->id, job->command ? job->command : "");

    return 0;
}

#endif
