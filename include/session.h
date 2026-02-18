/** session.h
 *
 * Declarations for managing shell sessions.
 * This module provides functions to initialize, update, and free
 * session-related data such as environment variables, command history,
 * aliases, and working directories.
 */

#ifndef SESSION_H
#define SESSION_H

#include "data/trie.h"       /* Trie */
#include "dirstack.h"        /* DirStack */
#include "environ.h"         /* Environ */
#include "history.h"         /* History */
#include "jobs.h"            /* Jobs */
#include "prompt/terminal.h" /* Terminal */

typedef struct Session {
    char     *current_working_dir;  // Current working directory
    char     *previous_working_dir; // Previous working directory
    Environ  *environ;              // Environment variables
    History  *history;              // Command history
    Trie     *aliases;              // Aliases
    Trie     *path_commands;        // Commands found in PATH
    DirStack *dirstack;             // Directory stack
    Terminal *terminal;             // Terminal information
    Jobs     *jobs;                 // Background jobs
    bool      exit_requested;       // Flag to indicate if shell should exit
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
