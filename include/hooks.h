/** hooks.h
 *
 * Hook name definitions for .tide scripts.
 */

#ifndef HOOKS_H
#define HOOKS_H

// Fired when entering a directory from its parent or ancestor (not when
// moving up from a child; see exit_child).
#define HOOK_ENTER "enter"

// Fired when moving up to a parent or ancestor directory (not when moving
// down into a child; see enter_child).
#define HOOK_EXIT "exit"

// Fired when moving down into a child directory; complements enter.
#define HOOK_ENTER_CHILD "enter_child"

// Fired when moving up from a child into its parent; complements exit.
#define HOOK_EXIT_CHILD "exit_child"

// Fired before executing a command string (one per input line).
#define HOOK_BEFORE_CMD "before_cmd"

// Fired after executing a command string (one per input line).
#define HOOK_AFTER_CMD "after_cmd"

// Fired right before displaying the prompt (prompt lifecycle, not execution).
#define HOOK_BEFORE_PROMPT "before_prompt"

// Fired right after the prompt is displayed (prompt lifecycle, not execution).
#define HOOK_AFTER_PROMPT "after_prompt"

// Fired after a command completes with a non-zero exit status (post-cmd only).
#define HOOK_ERROR "error"

// Fired right before executing an external command (not for builtins).
#define HOOK_BEFORE_EXEC "before_exec"

// Fired right after an external command finishes (not for builtins).
#define HOOK_AFTER_EXEC "after_exec"

// Fired when entering a subshell (subshell node execution).
#define HOOK_ENTER_SUBSHELL "enter_subshell"

// Fired after leaving a subshell (subshell node execution).
#define HOOK_EXIT_SUBSHELL "exit_subshell"

// Fired when an environment variable changes (set/remove).
#define HOOK_ENV_CHANGE "env_change"

// Fired when the working directory changes (any change).
#define HOOK_CD "cd"

// Fired when a command is not found in PATH.
#define HOOK_CMD_NOT_FOUND "cmd_not_found"

// Fired after adding or updating an alias.
#define HOOK_ALIAS_ADD "alias_add"

// Fired after removing an alias.
#define HOOK_ALIAS_REMOVE "alias_remove"

// Fired when a foreground command is terminated by a signal.
#define HOOK_SIGNAL "signal"

// Fired right before starting a background job.
#define HOOK_BEFORE_JOB "before_job"

// Fired after a background job finishes or is killed.
#define HOOK_AFTER_JOB "after_job"

// Fired when the command line fails to parse due to syntax error.
#define HOOK_SYNTAX_ERROR "syntax_error"

// Fired once per session after rc handling.
#define HOOK_SESSION_START "start"

// Fired once per session right before exit.
#define HOOK_SESSION_END "end"

// Fired right before reading the rc file.
#define HOOK_BEFORE_RC "before_rc"

#endif /* HOOKS_H */
