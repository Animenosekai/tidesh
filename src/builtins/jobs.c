#include <stdio.h>  /* printf, fprintf */
#include <stdlib.h> /* NULL */

#include "builtins/jobs.h"
#include "jobs.h"
#include "session.h"

int builtin_jobs(int argc, char **argv, Session *session) {
    (void)argc;
    (void)argv;

    if (!session || !session->jobs) {
        return 0;
    }

    // Update job states before listing
    jobs_update(session->jobs);

    // List all jobs
    for (int i = 0; i < session->jobs->count; i++) {
        Job *job = &session->jobs->jobs[i];

        const char *state_str = NULL;
        const char *marker    = " ";

        // Determine marker (+ for current, - for previous)
        Job *current  = jobs_get_current(session->jobs);
        Job *previous = jobs_get_previous(session->jobs);
        if (current && job->id == current->id) {
            marker = "+";
        } else if (previous && job->id == previous->id) {
            marker = "-";
        }

        // Determine state string
        switch (job->state) {
            case JOB_RUNNING:
                state_str = "Running";
                break;
            case JOB_STOPPED:
                state_str = "Stopped";
                break;
            case JOB_DONE:
                state_str = "Done";
                break;
            case JOB_KILLED:
                state_str = "Killed";
                break;
        }

        printf("[%d]%s  %s\t\t%s\n", job->id, marker, state_str,
               job->command ? job->command : "");
    }

    return 0;
}
