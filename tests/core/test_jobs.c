#include <signal.h> /* SIGKILL */
#include <stdio.h>  /* fprintf, stderr */
#include <stdlib.h> /* NULL */
#include <unistd.h> /* sleep */

#include "builtin.h"
#include "jobs.h"
#include "session.h"
#include "snow/snow.h"

describe(jobs_control) {
    it("should initialize jobs list") {
        Jobs *jobs = init_jobs();
        assertneq(jobs, NULL);
        asserteq(jobs->count, 0);
        free_jobs(jobs);
        free(jobs);
    }

    it("should add a job") {
        Jobs *jobs   = init_jobs();
        int   job_id = jobs_add(jobs, 12345, "sleep 100", JOB_RUNNING);
        asserteq(job_id, 1);
        asserteq(jobs->count, 1);
        free_jobs(jobs);
        free(jobs);
    }

    it("should get a job by ID") {
        Jobs *jobs   = init_jobs();
        int   job_id = jobs_add(jobs, 12345, "sleep 100", JOB_RUNNING);
        Job  *job    = jobs_get(jobs, job_id);
        assertneq(job, NULL);
        asserteq(job->id, 1);
        asserteq(job->pid, 12345);
        asserteq(job->state, JOB_RUNNING);
        free_jobs(jobs);
        free(jobs);
    }

    it("should get a job by PID") {
        Jobs *jobs   = init_jobs();
        int   job_id = jobs_add(jobs, 54321, "sleep 100", JOB_RUNNING);
        Job  *job    = jobs_get_by_pid(jobs, 54321);
        assertneq(job, NULL);
        asserteq(job->id, job_id);
        asserteq(job->pid, 54321);
        free_jobs(jobs);
        free(jobs);
    }

    it("should remove a job") {
        Jobs *jobs = init_jobs();
        int   id1  = jobs_add(jobs, 100, "cmd1", JOB_RUNNING);
        int   id2  = jobs_add(jobs, 200, "cmd2", JOB_RUNNING);
        asserteq(jobs->count, 2);

        bool removed = jobs_remove(jobs, id1);
        assert(removed);
        asserteq(jobs->count, 1);

        Job *job = jobs_get(jobs, id1);
        asserteq(job, NULL);

        job = jobs_get(jobs, id2);
        assertneq(job, NULL);

        free_jobs(jobs);
        free(jobs);
    }

    it("should get current job") {
        Jobs *jobs = init_jobs();
        int   id1  = jobs_add(jobs, 100, "cmd1", JOB_RUNNING);
        int   id2  = jobs_add(jobs, 200, "cmd2", JOB_RUNNING);

        Job *current = jobs_get_current(jobs);
        assertneq(current, NULL);
        asserteq(current->id, id2); // Most recent

        free_jobs(jobs);
        free(jobs);
    }

    it("should get previous job") {
        Jobs *jobs = init_jobs();
        int   id1  = jobs_add(jobs, 100, "cmd1", JOB_RUNNING);
        int   id2  = jobs_add(jobs, 200, "cmd2", JOB_RUNNING);

        Job *previous = jobs_get_previous(jobs);
        assertneq(previous, NULL);
        asserteq(previous->id, id1); // Second most recent

        free_jobs(jobs);
        free(jobs);
    }

    it("should handle no previous job") {
        Jobs *jobs = init_jobs();
        jobs_add(jobs, 100, "cmd1", JOB_RUNNING);

        Job *previous = jobs_get_previous(jobs);
        asserteq(previous, NULL); // Only one job

        free_jobs(jobs);
        free(jobs);
    }

    it("should execute jobs builtin with empty list") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *argv[] = {"jobs"};
        int   result = builtin_jobs(1, argv, session);
        asserteq(result, 0);

        free_session(session);
        free(session);
    }

    it("should execute jobs builtin with job list") {
        Session *session = init_session(NULL, "/tmp/test_history");

        jobs_add(session->jobs, 12345, "sleep 100", JOB_RUNNING);
        jobs_add(session->jobs, 12346, "cat", JOB_STOPPED);

        char *argv[] = {"jobs"};
        int   result = builtin_jobs(1, argv, session);
        asserteq(result, 0);

        free_session(session);
        free(session);
    }

    it("should handle bg builtin with no jobs") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *argv[] = {"bg"};
        int   result = builtin_bg(1, argv, session);
        asserteq(result, 1); // Error: no jobs

        free_session(session);
        free(session);
    }

    it("should handle fg builtin with no jobs") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *argv[] = {"fg"};
        int   result = builtin_fg(1, argv, session);
        asserteq(result, 1); // Error: no jobs

        free_session(session);
        free(session);
    }

    it("should verify jobs builtins are registered") {
        assert(is_builtin("jobs"));
        assert(is_builtin("fg"));
        assert(is_builtin("bg"));

        int (*jobs_func)(int, char **, Session *) = get_builtin("jobs");
        assertneq(jobs_func, NULL);

        int (*fg_func)(int, char **, Session *) = get_builtin("fg");
        assertneq(fg_func, NULL);

        int (*bg_func)(int, char **, Session *) = get_builtin("bg");
        assertneq(bg_func, NULL);
    }
}
