#include <signal.h>   /* kill, SIGCONT */
#include <stdio.h>    /* printf, fprintf */
#include <stdlib.h>   /* malloc, free, realloc */
#include <string.h>   /* strdup */
#include <sys/wait.h> /* waitpid, WNOHANG, WUNTRACED, WCONTINUED */
#include <unistd.h>   /* getpgrp */

#include "jobs.h"

Jobs *init_jobs(void) {
    Jobs *jobs = malloc(sizeof(Jobs));
    if (!jobs) {
        return NULL;
    }

    jobs->jobs     = NULL;
    jobs->count    = 0;
    jobs->capacity = 0;
    jobs->pgid     = getpgrp(); // Shell's process group

    return jobs;
}

int jobs_add(Jobs *jobs, pid_t pid, const char *command, JobState state) {
    if (!jobs) {
        return -1;
    }

    // Expand capacity if needed
    if (jobs->count >= jobs->capacity) {
        int  new_capacity = jobs->capacity == 0 ? 8 : jobs->capacity * 2;
        Job *new_jobs     = realloc(jobs->jobs, new_capacity * sizeof(Job));
        if (!new_jobs) {
            return -1;
        }
        jobs->jobs     = new_jobs;
        jobs->capacity = new_capacity;
    }

    // Find next available job ID
    int job_id = 1;
    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].id >= job_id) {
            job_id = jobs->jobs[i].id + 1;
        }
    }

    // Add new job
    Job *job         = &jobs->jobs[jobs->count];
    job->id          = job_id;
    job->pid         = pid;
    job->command     = command ? strdup(command) : NULL;
    job->state       = state;
    job->exit_status = 0;
    job->notified    = false;

    jobs->count++;
    return job_id;
}

Job *jobs_get(Jobs *jobs, int job_id) {
    if (!jobs) {
        return NULL;
    }

    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].id == job_id) {
            return &jobs->jobs[i];
        }
    }
    return NULL;
}

Job *jobs_get_by_pid(Jobs *jobs, pid_t pid) {
    if (!jobs) {
        return NULL;
    }

    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].pid == pid) {
            return &jobs->jobs[i];
        }
    }
    return NULL;
}

bool jobs_remove(Jobs *jobs, int job_id) {
    if (!jobs) {
        return false;
    }

    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].id == job_id) {
            // Free command string
            if (jobs->jobs[i].command) {
                free(jobs->jobs[i].command);
            }

            // Shift remaining jobs
            for (int j = i; j < jobs->count - 1; j++) {
                jobs->jobs[j] = jobs->jobs[j + 1];
            }
            jobs->count--;
            return true;
        }
    }
    return false;
}

Job *jobs_get_current(Jobs *jobs) {
    if (!jobs || jobs->count == 0) {
        return NULL;
    }

    // Return the most recently added job
    int  max_id  = -1;
    Job *current = NULL;
    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].id > max_id) {
            max_id  = jobs->jobs[i].id;
            current = &jobs->jobs[i];
        }
    }
    return current;
}

Job *jobs_get_previous(Jobs *jobs) {
    if (!jobs || jobs->count < 2) {
        return NULL;
    }

    // Find the two most recent jobs
    int  max_id        = -1;
    int  second_max_id = -1;
    Job *previous      = NULL;

    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].id > max_id) {
            second_max_id = max_id;
            max_id        = jobs->jobs[i].id;
        } else if (jobs->jobs[i].id > second_max_id) {
            second_max_id = jobs->jobs[i].id;
        }
    }

    // Get the second most recent
    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].id == second_max_id) {
            previous = &jobs->jobs[i];
            break;
        }
    }
    return previous;
}

void jobs_update(Jobs *jobs) {
    if (!jobs) {
        return;
    }

    for (int i = 0; i < jobs->count; i++) {
        Job  *job = &jobs->jobs[i];
        int   status;
        pid_t result =
            waitpid(job->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if (result > 0) {
            JobState old_state = job->state;

            if (WIFEXITED(status)) {
                job->state       = JOB_DONE;
                job->exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                job->state       = JOB_KILLED;
                job->exit_status = 128 + WTERMSIG(status);
            } else if (WIFSTOPPED(status)) {
                job->state = JOB_STOPPED;
            } else if (WIFCONTINUED(status)) {
                job->state = JOB_RUNNING;
            }

            // Mark as not notified if state changed
            if (old_state != job->state) {
                job->notified = false;
            }
        }
    }
}

void jobs_notify(Jobs *jobs) {
    if (!jobs) {
        return;
    }

    for (int i = jobs->count - 1; i >= 0; i--) {
        Job *job = &jobs->jobs[i];

        if (!job->notified) {
            const char *state_str = NULL;
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

            if (state_str) {
                printf("[%d] %s\t\t%s\n", job->id, state_str,
                       job->command ? job->command : "");
                job->notified = true;
            }

            // Remove finished/killed jobs after notification
            if (job->state == JOB_DONE || job->state == JOB_KILLED) {
                jobs_remove(jobs, job->id);
                i++; // Adjust index since we removed an element
            }
        }
    }
}

void free_jobs(Jobs *jobs) {
    if (!jobs) {
        return;
    }

    for (int i = 0; i < jobs->count; i++) {
        if (jobs->jobs[i].command) {
            free(jobs->jobs[i].command);
        }
    }

    if (jobs->jobs) {
        free(jobs->jobs);
    }
}
