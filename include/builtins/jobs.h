/** jobs.h
 *
 * Declarations for the 'jobs' builtin command.
 */

#ifndef BUILTIN_JOBS_H
#define BUILTIN_JOBS_H

#include "session.h"

/**
 * The jobs builtin command.
 *
 * Lists all current jobs and their states.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @param session The current session
 * @return The exit status (0 on success)
 */
int builtin_jobs(int argc, char **argv, Session *session);

#endif /* BUILTIN_JOBS_H */
