/** hooks.h
 *
 * Hook name definitions for .tidesh-hooks scripts.
 *
 * GLOBAL HOOK CONTEXT (available to all hooks):
 * - TIDE_HOOK: The specific hook name being executed (e.g., "cd", "before_cmd")
 * - TIDE_TIMESTAMP: Unix timestamp when the hook fires (epoch seconds)
 * - TIDE_TIMESTAMP_NANO: Unix timestamp when the hook fires (nanoseconds)
 *
 * Additional context variables are documented with each hook below.
 */

#ifndef HOOKS_H
#define HOOKS_H

#include <stddef.h> /* size_t */

typedef struct Session Session;

typedef struct HookEnvVar {
    const char *key;
    const char *value;
} HookEnvVar;

// Wildcard hook: called before any specific hook fires
// Additional context: Same variables as the specific hook that will follow
#define HOOK_ALL "*"

// Fired when entering a directory from its parent or ancestor (not when
// moving up from a child; see exit_child).
// Additional context: TIDE_FROM (previous directory), TIDE_TO (new directory)
#define HOOK_ENTER "enter"

// Fired when moving up to a parent or ancestor directory (not when moving
// down into a child; see enter_child).
// Additional context: TIDE_FROM (previous directory), TIDE_TO (new directory)
#define HOOK_EXIT "exit"

// Fired when moving down into a child directory; complements enter.
// Additional context: TIDE_CHILD (child directory path)
#define HOOK_ENTER_CHILD "enter_child"

// Fired when moving up from a child into its parent; complements exit.
// Additional context: TIDE_CHILD (child directory path being exited)
#define HOOK_EXIT_CHILD "exit_child"

// Fired before executing a command string (one per input line).
// Additional context: TIDE_CMDLINE (full command line), TIDE_CMD (first word)
#define HOOK_BEFORE_CMD "before_cmd"

// Fired after executing a command string (one per input line).
// Additional context: TIDE_CMDLINE (full command line), TIDE_CMD (first word)
#define HOOK_AFTER_CMD "after_cmd"

// Fired right before displaying the prompt (prompt lifecycle, not execution).
// Additional context: None
#define HOOK_BEFORE_PROMPT "before_prompt"

// Fired right after the prompt is displayed (prompt lifecycle, not execution).
// Additional context: None
#define HOOK_AFTER_PROMPT "after_prompt"

// Fired after a command completes with a non-zero exit status (post-cmd only).
// Additional context: TIDE_CMDLINE, TIDE_CMD, TIDE_ERROR ("CMD_FAIL"),
// CMD_FAIL ("1")
#define HOOK_ERROR "error"

// Fired right before executing an external command (not for builtins).
// Additional context: TIDE_EXEC (resolved command path), TIDE_ARGV0 (argv[0])
#define HOOK_BEFORE_EXEC "before_exec"

// Fired right after an external command finishes (not for builtins).
// Additional context: TIDE_EXEC (resolved command path), TIDE_ARGV0 (argv[0])
#define HOOK_AFTER_EXEC "after_exec"

// Fired when entering a subshell (subshell node execution).
// Additional context: None
#define HOOK_ENTER_SUBSHELL "enter_subshell"

// Fired after leaving a subshell (subshell node execution).
// Additional context: None
#define HOOK_EXIT_SUBSHELL "exit_subshell"

// Fired when an environment variable is added.
// Additional context: TIDE_ENV_KEY, TIDE_ENV_VALUE, TIDE_ENV_OLD_VALUE (empty
// for new vars)
#define HOOK_ADD_ENVIRON "add_environ"

// Fired when an environment variable is removed.
// Additional context: TIDE_ENV_KEY, TIDE_ENV_VALUE (empty), TIDE_ENV_OLD_VALUE
// (previous value)
#define HOOK_REMOVE_ENVIRON "remove_environ"

// Fired when an environment variable changes value.
// Additional context: TIDE_ENV_KEY, TIDE_ENV_VALUE (new value),
// TIDE_ENV_OLD_VALUE (previous value)
#define HOOK_CHANGE_ENVIRON "change_environ"

// Fired when the working directory changes (any change).
// Additional context: TIDE_DIR (current directory), TIDE_FROM (previous
// directory), TIDE_TO (new directory), TIDE_PARENT (parent directory of
// target)
#define HOOK_CD "cd"

// Fired when a command is not found in PATH.
// Additional context: TIDE_CMD (command name)
#define HOOK_CMD_NOT_FOUND "cmd_not_found"

// Fired after adding an alias.
// Additional context: TIDE_ALIAS_NAME, TIDE_ALIAS_VALUE
#define HOOK_ADD_ALIAS "add_alias"

// Fired after removing an alias.
// Additional context: TIDE_ALIAS_NAME, TIDE_ALIAS_VALUE (value before removal)
#define HOOK_REMOVE_ALIAS "remove_alias"

// Fired after updating an alias.
// Additional context: TIDE_ALIAS_NAME, TIDE_ALIAS_VALUE (new value)
#define HOOK_CHANGE_ALIAS "change_alias"

// Fired when a foreground command is terminated by a signal.
// Additional context: TIDE_SIGNAL (signal number)
#define HOOK_SIGNAL "signal"

// Fired right before starting a background job.
// Additional context: TIDE_JOB_ID (job ID), TIDE_JOB_PID (process ID),
// TIDE_JOB_STATE ("running")
#define HOOK_BEFORE_JOB "before_job"
// Additional context: TIDE_JOB_ID (job ID), TIDE_JOB_PID (process ID),
// TIDE_JOB_STATE (job state: running, stopped, done, killed)

// Fired after a background job finishes or is killed.
#define HOOK_AFTER_JOB "after_job"

// Fired when the command line fails to parse due to syntax error.
// Additional context: TIDE_CMDLINE, TIDE_CMD, TIDE_ERROR ("SYNTAX_ERROR")
#define HOOK_SYNTAX_ERROR "syntax_error"

// Fired once per session after rc handling.
// Additional context: None
#define HOOK_SESSION_START "start"

// Fired once per session right before exit.
// Additional context: None
#define HOOK_SESSION_END "end"

// Fired right before reading the rc file.
// Additional context: None
#define HOOK_BEFORE_RC "before_rc"

/**
 * Run a hook script from the current working directory's .tidesh-hooks folder.
 *
 * @param session Pointer to Session
 * @param hook_name Hook script name (e.g., "cmd_pre")
 */
void run_cwd_hook(Session *session, const char *hook_name);

/**
 * Run a hook script from the current working directory's .tidesh-hooks folder
 * with temporary environment variables.
 *
 * @param session Pointer to Session
 * @param hook_name Hook script name (e.g., "before_cmd")
 * @param vars Extra environment variables to set for the hook
 * @param var_count Number of variables in vars
 */
void run_cwd_hook_with_vars(Session *session, const char *hook_name,
                            const HookEnvVar *vars, size_t var_count);

/**
 * Run a hook script from the specified directory's .tidesh-hooks folder with
 * temporary environment variables.
 *
 * @param session Pointer to Session
 * @param dir Directory to search for .tidesh-hooks hooks
 * @param hook_name Hook script name (e.g., "cd")
 * @param vars Extra environment variables to set for the hook
 * @param var_count Number of variables in vars
 */
void run_dir_hook_with_vars(Session *session, const char *dir,
                            const char *hook_name, const HookEnvVar *vars,
                            size_t var_count);

/**
 * Register session-level hook callbacks (env changes, job state changes).
 *
 * @param session Pointer to Session
 */
void hooks_register_session(Session *session);

#endif /* HOOKS_H */
