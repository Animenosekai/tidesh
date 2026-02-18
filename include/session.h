/** session.h
 *
 * Declarations for managing shell sessions.
 * This module provides functions to initialize, update, and free
 * session-related data such as environment variables, command history,
 * aliases, and working directories.
 */

#ifndef SESSION_H
#define SESSION_H

#include <stddef.h> /* size_t */

#include "data/trie.h"       /* Trie */
#include "environ.h"         /* Environ */
#include "features.h"        /* Features */
#include "prompt/terminal.h" /* Terminal */

#ifndef TIDESH_DISABLE_HISTORY
#include "history.h" /* History */
#endif

#ifndef TIDESH_DISABLE_DIRSTACK
#include "dirstack.h" /* DirStack */
#endif

#ifndef TIDESH_DISABLE_JOB_CONTROL
#include "jobs.h" /* Jobs */
#endif

typedef struct Session {
    char    *current_working_dir;  // Current working directory
    char    *previous_working_dir; // Previous working directory
    Environ *environ;              // Environment variables
#ifndef TIDESH_DISABLE_HISTORY
    History *history; // Command history
#endif
#ifndef TIDESH_DISABLE_ALIASES
    Trie *aliases; // Aliases
#endif
    Trie *path_commands; // Commands found in PATH
#ifndef TIDESH_DISABLE_DIRSTACK
    DirStack *dirstack; // Directory stack
#endif
    Terminal *terminal; // Terminal information
#ifndef TIDESH_DISABLE_JOB_CONTROL
    Jobs *jobs; // Background jobs
#endif
    Features features;       // Runtime feature flags
    bool     exit_requested; // Flag to indicate if shell should exit
    bool     hooks_disabled; // Prevent hook recursion during hook execution
} Session;

/**
 * Initialize a session with the given history file path
 *
 * @param session Pointer to Session to initialize, or NULL to allocate a new
 * one
 * @param history_path Path to the history file
 * @return Pointer to initialized Session, or NULL on failure
 */
Session *init_session(Session *session, char *history_path);

/**
 * Update the current and previous working directories in the session
 *
 * @param session Pointer to Session to update
 */
void update_working_dir(Session *session);

/**
 * Update the PATH in the session
 *
 * @param session Pointer to Session to update
 */
void update_path(Session *session);

/**
 * Free all resources used by a Session structure
 *
 * @param session Pointer to Session to free
 */
void free_session(Session *session);

#endif /* SESSION_H */
