"""
Exceptions for tidesh.

This module provides exception classes for various tidesh errors,
organized by category for better exception handling and debugging.
"""

from __future__ import annotations


class TideshError(Exception):
    """
    Base exception for all tidesh errors.

    All other tidesh exceptions inherit from this class, allowing
    you to catch all tidesh-specific errors with a single except clause.
    """

    def __init__(self, message: str, *, cause: BaseException | None = None) -> None:
        """
        Initialize the exception with an error message and optional cause.

        Parameters
        ----------
        message : str
            A descriptive error message.
        cause : BaseException | None, optional
            The underlying cause of this error (for exception chaining).
        """
        super().__init__(message)
        self.__cause__ = cause

    def __repr__(self) -> str:
        """Return a detailed representation of the exception."""
        return f"{self.__class__.__name__}({self.args[0]!r})"


class SessionError(TideshError):
    """
    Raised when a session management operation fails.

    This includes errors during session initialization, configuration,
    or resource management.
    """


class ParseError(TideshError):
    """
    Raised when command parsing fails.

    This occurs when the input cannot be parsed into a valid abstract
    syntax tree (AST).
    """

    def __init__(
        self,
        message: str,
        command: str | None = None,
        *,
        cause: BaseException | None = None,
    ) -> None:
        """
        Initialize the exception with parsing error details.

        Parameters
        ----------
        message : str
            A descriptive error message.
        command : str | None, optional
            The command that failed to parse.
        cause : BaseException | None, optional
            The underlying cause of this error.
        """
        super().__init__(message, cause=cause)
        self.command = command
        """The command that failed to parse, if available."""

    def __str__(self) -> str:
        """Return a formatted error message."""
        msg = self.args[0] if self.args else "Parse error"
        if self.command:
            msg += f"\nCommand: {self.command}"
        return msg


class LexerError(TideshError):
    """
    Raised when lexical analysis (tokenization) fails.

    This occurs when the input cannot be properly tokenized.
    """

    def __init__(
        self,
        message: str,
        command: str | None = None,
        position: int | None = None,
        *,
        cause: BaseException | None = None,
    ) -> None:
        """
        Initialize the exception with lexer error details.

        Parameters
        ----------
        message : str
            A descriptive error message.
        command : str | None, optional
            The command being analyzed.
        position : int | None, optional
            The position in the command where the error occurred.
        cause : BaseException | None, optional
            The underlying cause of this error.
        """
        super().__init__(message, cause=cause)
        self.command = command
        """The command being analyzed, if available."""
        self.position = position
        """The position in the command where the error occurred, if known."""

    def __str__(self) -> str:
        """Return a formatted error message."""
        msg = self.args[0] if self.args else "Lexer error"
        if self.command:
            msg += f"\nCommand: {self.command}"
            if self.position is not None and 0 <= self.position < len(self.command):
                msg += f"\nPosition: {self.position}"
                msg += f"\n  {self.command}"
                msg += f"\n  {' ' * self.position}^"
        return msg


class CommandNotFoundError(TideshError):
    """
    Raised when a command is not found.

    This exception indicates that the requested command could not be
    located in the system's executable search path.
    """

    def __init__(self, command: str) -> None:
        """
        Initialize the exception with the command that was not found.

        Parameters
        ----------
        command : str
            The name of the command that was not found.
        """
        super().__init__(f"Command not found: {command!r}")
        self.command = command
        """The command that was not found."""

    def __repr__(self) -> str:
        """Return a detailed representation of the exception."""
        return f"CommandNotFoundError({self.command!r})"


class ExecutionError(TideshError):
    """
    Raised when a command execution fails.

    This exception indicates that a command was executed but returned
    a non-zero exit code (i.e., it failed).
    """

    def __init__(
        self,
        message: str,
        exit_code: int,
        command: str | None = None,
        *,
        cause: BaseException | None = None,
    ) -> None:
        """
        Initialize the exception with execution error details.

        Parameters
        ----------
        message : str
            A descriptive error message.
        exit_code : int
            The exit code returned by the failed command.
        command : str | None, optional
            The command that failed.
        cause : BaseException | None, optional
            The underlying cause of this error.
        """
        super().__init__(message, cause=cause)
        self.exit_code = exit_code
        """The exit code returned by the failed command."""
        self.command = command
        """The command that failed, if available."""

    def __str__(self) -> str:
        """Return a formatted error message."""
        msg = self.args[0] if self.args else "Execution error"
        msg += f"\nExit code: {self.exit_code}"
        if self.command:
            msg += f"\nCommand: {self.command}"
        return msg

    def __repr__(self) -> str:
        """Return a detailed representation of the exception."""
        return f"ExecutionError({self.args[0]!r}, exit_code={self.exit_code})"


class EnvironmentVariableError(TideshError):
    """
    Raised when an environment variable operation fails.

    This includes errors when getting, setting, or deleting environment variables.
    """

    def __init__(
        self,
        message: str,
        var_name: str | None = None,
        *,
        cause: BaseException | None = None,
    ) -> None:
        """
        Initialize the exception with environment variable error details.

        Parameters
        ----------
        message : str
            A descriptive error message.
        var_name : str | None, optional
            The name of the environment variable involved.
        cause : BaseException | None, optional
            The underlying cause of this error.
        """
        super().__init__(message, cause=cause)
        self.var_name = var_name
        """The name of the environment variable involved, if available."""


class AliasError(TideshError):
    """
    Raised when an alias operation fails.

    This includes errors when getting, setting, or deleting aliases.
    """

    def __init__(
        self,
        message: str,
        alias_name: str | None = None,
        *,
        cause: BaseException | None = None,
    ) -> None:
        """
        Initialize the exception with alias error details.

        Parameters
        ----------
        message : str
            A descriptive error message.
        alias_name : str | None, optional
            The name of the alias involved.
        cause : BaseException | None, optional
            The underlying cause of this error.
        """
        super().__init__(message, cause=cause)
        self.alias_name = alias_name
        """The name of the alias involved, if available."""


class DirectoryStackError(TideshError):
    """
    Raised when a directory stack operation fails.

    This includes errors with pushd, popd, or directory stack management.
    """

    def __init__(
        self,
        message: str,
        directory: str | None = None,
        *,
        cause: BaseException | None = None,
    ) -> None:
        """
        Initialize the exception with directory stack error details.

        Parameters
        ----------
        message : str
            A descriptive error message.
        directory : str | None, optional
            The directory involved in the operation.
        cause : BaseException | None, optional
            The underlying cause of this error.
        """
        super().__init__(message, cause=cause)
        self.directory = directory
        """The directory involved in the operation, if available."""
