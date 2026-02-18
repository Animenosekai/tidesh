"""
tidesh - Python bindings for the tidesh shell

This module provides a high-level interface to the tidesh shell,
allowing you to create shell sessions, execute commands,
manage environment variables and aliases and perform
shell expansions directly from Python.

It uses CFFI to interface with the underlying C library of tidesh,
exposing its functionality in a Pythonic way.

Examples
--------
>>> from tidesh import Session
>>> with Session() as s:
...     s.environ['MY_VAR'] = 'Hello'
...     s.execute('echo $MY_VAR')
Hello

Classes
-------
- Session: Represents a shell session with state and capabilities.
- History: Manages command history for a session.
- Environ: A dict-like interface for environment variables.
- Aliases: A dict-like interface for shell aliases.
- DirectoryStack: A sequence interface for the directory stack.
- Terminal: Provides information about the terminal capabilities.
- Jobs: A dict-like interface for background job control.
- JobState: Job state constants (RUNNING, STOPPED, DONE, KILLED).
- Hook: Hook name constants for .tidesh-hooks scripts.
- Features: Runtime feature flags for conditional shell features.
- Lexer: Lexical analyzer for tokenizing shell commands.
- Token: Base class for all tokens in the shell command.
- ValuedToken: Token with a value attribute.
- ExtraValuedToken: Token with both value and extra attributes.
- WordToken, IONumberToken, CommentToken, etc.: Specific token types.
- ASTNode: Represents a node in the abstract syntax tree of a command.

Functions
---------
- run(command: str) -> int: Run a command in a new session and return its exit code.
- capture(command: str) -> str | None: Run a command and capture its standard output.
- tokenize(command: str) -> Iterable[Token]: Tokenize a command string.
- parse(command: str) -> ASTNode: Parse a command string into an abstract syntax tree.
"""
# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false

from __future__ import annotations

import typing

from .__info__ import (
    __author__,
    __brief__,
    __build_date__,
    __build_type__,
    __compiler__,
    __copyright__,
    __email__,
    __git_version__,
    __license__,
    __module__,
    __platform__,
    __repository__,
    __status__,
    __version__,
    __year__,
)
from ._tidesh import ffi, lib
from .ast import ASTNode, NodeType
from .constants import Hook
from .exceptions import (
    AliasError,
    CommandNotFoundError,
    DirectoryStackError,
    EnvironmentVariableError,
    ExecutionError,
    LexerError,
    ParseError,
    SessionError,
    TideshError,
)
from .lexer import Lexer
from .session import (
    AliasCommandInfo,
    BuiltinCommandInfo,
    CommandInfo,
    ExternalCommandInfo,
    Session,
)
from .state import (
    Aliases,
    DirectoryStack,
    Environ,
    Features,
    History,
    Jobs,
    JobState,
    Terminal,
)
from .tokens import (
    AssignmentToken,
    BackgroundToken,
    CommentToken,
    EOFToken,
    EOLToken,
    ExtraValuedToken,
    FDDuplicationToken,
    HeredocToken,
    HereStringToken,
    IONumberToken,
    LParenToken,
    OrToken,
    PipeToken,
    ProcessSubstitutionInToken,
    ProcessSubstitutionOutToken,
    RedirectAppendToken,
    RedirectInToken,
    RedirectOutErrToken,
    RedirectOutToken,
    RParenToken,
    SemicolonToken,
    SequenceToken,
    Token,
    TokenType,
    ValuedToken,
    WordToken,
)

__all__ = [
    "Aliases",
    "AliasError",
    "ASTNode",
    "AssignmentToken",
    "BackgroundToken",
    "Token",
    "CommentToken",
    "CommandNotFoundError",
    "CommandInfo",
    "ExternalCommandInfo",
    "BuiltinCommandInfo",
    "AliasCommandInfo",
    "DirectoryStack",
    "DirectoryStackError",
    "EOFToken",
    "EOLToken",
    "Environ",
    "EnvironmentVariableError",
    "ExecutionError",
    "ExtraValuedToken",
    "FDDuplicationToken",
    "Features",
    "HeredocToken",
    "HereStringToken",
    "History",
    "Hook",
    "IONumberToken",
    "JobState",
    "Jobs",
    "LexerError",
    "LParenToken",
    "Lexer",
    "NodeType",
    "OrToken",
    "ParseError",
    "PipeToken",
    "ProcessSubstitutionInToken",
    "ProcessSubstitutionOutToken",
    "RParenToken",
    "RedirectAppendToken",
    "RedirectInToken",
    "RedirectOutErrToken",
    "RedirectOutToken",
    "SemicolonToken",
    "SequenceToken",
    "Session",
    "SessionError",
    "Terminal",
    "TideshError",
    "TokenType",
    "ValuedToken",
    "WordToken",
    "capture",
    "parse",
    "run",
    "tokenize",
    "__author__",
    "__brief__",
    "__build_date__",
    "__build_type__",
    "__copyright__",
    "__email__",
    "__git_version__",
    "__license__",
    "__module__",
    "__platform__",
    "__repository__",
    "__status__",
    "__version__",
    "__year__",
    "__compiler__",
]


def run(command: str) -> int:
    """
    Run a command in a new session and return its exit code.

    Parameters
    ----------
    command : str
        The command to run.

    Returns
    -------
    int
        The exit code of the command.
    """
    with Session() as s:
        return s.execute(command)


def capture(command: str) -> str | None:
    """
    Run a command in a new session and capture its standard output.

    Parameters
    ----------
    command : str
        The command to run.

    Returns
    -------
    str | None
        The captured output of the command, or None if the command failed.
    """
    with Session() as s:
        return s.capture(command)


def tokenize(command: str) -> typing.Iterable[Token]:
    """
    Tokenize a command string in a new session.

    Parameters
    ----------
    command : str
        The command to tokenize.

    Returns
    -------
    Iterable[Token]
        An iterable of tokens representing the tokenized command.
    """
    with Session() as s:
        lexer = Lexer(command, s)
        yield from lexer.tokenize()


def parse(command: str) -> ASTNode:
    """
    Parse a command string into an abstract syntax tree (AST) in a new session.

    Parameters
    ----------
    command : str
        The command to parse.

    Returns
    -------
    ASTNode
        The root node of the parsed abstract syntax tree representing the command.
    """
    with Session() as s:
        lexer = Lexer(command, s)
        c_node = lib.parse(lexer._lexer, s._session)  # noqa: SLF001
        if c_node == ffi.NULL:
            msg = "Failed to parse command"
            raise ParseError(msg, command=command)
        return ASTNode(c_node)
