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
#include "feature-flags.h"   /* Features */
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

#ifndef TIDESH_MAX_SIGNALS
#define TIDESH_MAX_SIGNALS 64
#endif

typedef struct ShellOptions {
    bool allexport; // -a: Automatically export modified/created variables
    bool errexit;   // -e: Exit immediately if a command exits with non-zero
    bool noglob;    // -f: Disable pathname expansion (globbing)
    bool nounset;   // -u: Treat unset variables as an error
    bool verbose;   // -v: Print shell input lines as they are read
    bool xtrace;    // -x: Print commands and arguments before executing
    bool noexec;    // -n: Read commands but do not execute them
    bool noclobber; // -C: Prevent output redirection from overwriting files
    bool monitor;   // -m: Job control
    bool pipefail;  // Return value of pipeline is status of last failed command
    bool notify;    // -b: Report status of background jobs immediately
    bool hashall;   // -h: Remember locations of commands
    bool ignoreeof; // Do not exit on EOF
} ShellOptions;

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
#ifndef TIDESH_DISABLE_FUNCTIONS
    Trie *functions; // Shell functions (name -> ASTNode*)
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
    bool     initial_parent_hooks_run; // Track if initial HOOK_ENTER has been
                                       // executed
    int      loop_depth;               // Current nesting depth of loops
    int      loop_break;               // Pending break levels to propagate
    int      loop_continue;            // Pending continue levels to propagate
    int      function_depth;   // Current nesting depth of functions / sourced
                               // scripts
    bool     return_requested; // Whether return has been requested
    int      return_status;    // Return status code
    bool  permanent_redirections; // Leave redirections active in current shell
    char *trap_exit;              // Action for EXIT trap (condition 0)
    char *trap_signals[TIDESH_MAX_SIGNALS]; // Action for signals 1..63
    void (*original_handlers[TIDESH_MAX_SIGNALS])(
        int);                                  // Original signal handlers
    bool has_orig_handler[TIDESH_MAX_SIGNALS]; // Whether original handler was
                                               // saved
    bool in_trap;         // Reentrancy guard for trap execution
    bool is_subshell;     // Whether session is in a subshell
    bool trap_exit_local; // Whether EXIT trap was set within this subshell
    ShellOptions options; // Shell options (set builtin)
    int ignore_errexit;   // Ignore errexit in conditions and and/or lists
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    struct CustomContextManager
                        *context_managers; // Custom registered context managers
    struct ContextFrame *context_stack;    // Active context unwind stack
#endif
    struct AllowedExitCodes *allowed_exit_codes; // Active allowed exit codes
    struct FSSandbox        *fs_sandbox;    // Active filesystem sandbox stack
    int                      try_depth;     // Nesting depth of try blocks
    bool                     try_caught;    // Flag indicating exception raised
    int                      try_exit_code; // Caught exit code
} Session;

/* Stack of configured allowed exit codes (e.g. via with
 * allowed_exit_codes(...)) */
typedef struct AllowedExitCodes {
    int                     *codes;
    size_t                   count;
    bool                     allow_all;
    struct AllowedExitCodes *prev;
} AllowedExitCodes;

bool session_is_exit_code_allowed(Session *session, int status);
void session_push_allowed_exit_codes(Session *session, const int *codes,
                                     size_t count, bool allow_all);
void session_pop_allowed_exit_codes(Session *session);

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
 * Run initial parent enter hooks after environment setup
 * This is called after .tideshrc is read to ensure environment variables
 * are properly initialized before running HOOK_ENTER hooks
 *
 * @param session Pointer to Session
 */
void run_initial_parent_hooks(Session *session);

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
