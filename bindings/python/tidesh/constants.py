"""Constants and enums for tidesh."""

# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false
from __future__ import annotations

import enum


class Hook(str, enum.Enum):
    """
    Hook names for .tidesh-hooks scripts.

    Each enum value represents a hook that can be triggered during shell execution.
    The value is the actual hook filename (without extension) that should exist in
    the .tidesh-hooks directory.
    """

    # Wildcard hook: called before any specific hook fires
    ALL = "*"

    # Directory navigation hooks
    ENTER = "enter"
    """Fired when entering a directory from its parent or ancestor."""

    EXIT = "exit"
    """Fired when moving up to a parent or ancestor directory."""

    ENTER_CHILD = "enter_child"
    """Fired when moving down into a child directory."""

    EXIT_CHILD = "exit_child"
    """Fired when moving up from a child into its parent."""

    CD = "cd"
    """Fired when the working directory changes (any change)."""

    # Command execution hooks
    BEFORE_CMD = "before_cmd"
    """Fired before executing a command string (one per input line)."""

    AFTER_CMD = "after_cmd"
    """Fired after executing a command string (one per input line)."""

    BEFORE_EXEC = "before_exec"
    """Fired right before executing an external command (not for builtins)."""

    AFTER_EXEC = "after_exec"
    """Fired right after an external command finishes (not for builtins)."""

    ERROR = "error"
    """Fired after a command completes with a non-zero exit status."""

    CMD_NOT_FOUND = "cmd_not_found"
    """Fired when a command is not found in PATH."""

    # Prompt hooks
    BEFORE_PROMPT = "before_prompt"
    """Fired right before displaying the prompt."""

    AFTER_PROMPT = "after_prompt"
    """Fired right after the prompt is displayed."""

    # Subshell hooks
    ENTER_SUBSHELL = "enter_subshell"
    """Fired when entering a subshell."""

    EXIT_SUBSHELL = "exit_subshell"
    """Fired after leaving a subshell."""

    # Environment variable hooks
    ADD_ENVIRON = "add_environ"
    """Fired when an environment variable is added."""

    REMOVE_ENVIRON = "remove_environ"
    """Fired when an environment variable is removed."""

    CHANGE_ENVIRON = "change_environ"
    """Fired when an environment variable changes value."""

    # Alias hooks
    ADD_ALIAS = "add_alias"
    """Fired after adding an alias."""

    REMOVE_ALIAS = "remove_alias"
    """Fired after removing an alias."""

    CHANGE_ALIAS = "change_alias"
    """Fired after updating an alias."""

    # Job control hooks
    BEFORE_JOB = "before_job"
    """Fired right before starting a background job."""

    AFTER_JOB = "after_job"
    """Fired after a background job finishes or is killed."""

    SIGNAL = "signal"
    """Fired when a foreground command is terminated by a signal."""

    # Session hooks
    START = "start"
    """Fired once per session after rc handling (session_start)."""

    END = "end"
    """Fired once per session right before exit (session_end)."""

    BEFORE_RC = "before_rc"
    """Fired right before reading the rc file."""

    SYNTAX_ERROR = "syntax_error"
    """Fired when the command line fails to parse due to syntax error."""
