/** jobs.h
 *
 * Declarations for job control management.
 * This module provides functionality to track and manage background jobs.
 */

#ifndef JOBS_H
#define JOBS_H

#ifndef TIDESH_DISABLE_JOB_CONTROL

#include <stdbool.h>
#include <sys/types.h>

/* Job states */
typedef enum JobState {
    JOB_RUNNING, // Job is currently running
    JOB_STOPPED, // Job is stopped (suspended)
    JOB_DONE,    // Job has completed
    JOB_KILLED   // Job was terminated
} JobState;

/* Job structure */
typedef struct Job {
    int      id;          // Job ID (1-based)
    pid_t    pid;         // Process ID
    char    *command;     // Command string
    JobState state;       // Current state
    int      exit_status; // Exit status (if done)
    bool     notified;    // Whether state change has been reported
} Job;

/* Jobs list structure */
typedef struct Jobs {
    Job  *jobs;     // Array of jobs
    int   count;    // Number of jobs
    int   capacity; // Capacity of jobs array
    pid_t pgid;     // Process group ID for the shell
} Jobs;

/**
 * Initialize a jobs list
 *
 * @return Pointer to initialized Jobs, or NULL on failure
 */
Jobs *init_jobs(void);

/**
 * Add a job to the jobs list
 *
 * @param jobs Pointer to Jobs
 * @param pid Process ID
 * @param command Command string
 * @param state Initial state
 * @return Job ID (1-based), or -1 on failure
 */
int jobs_add(Jobs *jobs, pid_t pid, const char *command, JobState state);

/**
 * Get a job by ID
 *
 * @param jobs Pointer to Jobs
 * @param job_id Job ID (1-based)
 * @return Pointer to Job, or NULL if not found
 */
Job *jobs_get(Jobs *jobs, int job_id);

/**
 * Get a job by PID
 *
 * @param jobs Pointer to Jobs
 * @param pid Process ID
 * @return Pointer to Job, or NULL if not found
 */
Job *jobs_get_by_pid(Jobs *jobs, pid_t pid);

/**
 * Remove a job from the jobs list
 *
 * @param jobs Pointer to Jobs
 * @param job_id Job ID (1-based)
 * @return true if removed, false if not found
 */
bool jobs_remove(Jobs *jobs, int job_id);

/**
 * Get the current (most recent) job
 *
 * @param jobs Pointer to Jobs
 * @return Pointer to Job, or NULL if no jobs
 */
Job *jobs_get_current(Jobs *jobs);

/**
 * Get the previous (second most recent) job
 *
 * @param jobs Pointer to Jobs
 * @return Pointer to Job, or NULL if less than 2 jobs
 */
Job *jobs_get_previous(Jobs *jobs);

/**
 * Update job states by checking their status
 *
 * @param jobs Pointer to Jobs
 */
void jobs_update(Jobs *jobs);

/**
 * Print status updates for jobs that have changed state
 *
 * @param jobs Pointer to Jobs
 */
void jobs_notify(Jobs *jobs);

/**
 * Free all resources used by Jobs structure
 *
 * @param jobs Pointer to Jobs to free
 */
void free_jobs(Jobs *jobs);

#endif /* TIDESH_DISABLE_JOB_CONTROL */

#endif /* JOBS_H */
