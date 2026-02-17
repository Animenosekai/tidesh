"""Lexer for tidesh."""

# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false

from __future__ import annotations

import typing

from ._tidesh import ffi, lib
from .tokens import EOFToken, Token, create_token

if typing.TYPE_CHECKING:
    from collections.abc import Iterator

    from .session import Session


class Lexer:
    """
    Lexical analyzer for shell commands.

    The lexer tokenizes shell command strings into individual tokens
    that can be further processed or parsed.
    """

    def __init__(self, command: str, session: Session) -> None:
        """
        Initialize the lexer with a command string and session context.

        Parameters
        ----------
        command : str
            The command string to tokenize.
        session : Session
            The session context for lexical analysis.
        """
        super().__init__()
        self._command = command.encode()
        self._session = session
        # We pass NULL for the execute function for now as it's complex to bridge
        self._lexer = lib.init_lexer_input(
            ffi.NULL,
            self._command,
            ffi.NULL,
            session._session,  # noqa: SLF001
        )

    def next_token(self) -> Token:
        """
        Get the next token from the input.

        Returns
        -------
        Token
            The next token in the command string.
        """
        c_token = lib.lexer_next_token(self._lexer)
        token = create_token(c_token)
        lib.free_lexer_token(ffi.addressof(c_token))
        return token

    def tokenize(self) -> typing.Iterable[Token]:
        """
        Tokenize the entire command string.

        Yields
        ------
        Token
            All tokens in the command, including the EOF token.
        """
        while True:
            token = self.next_token()
            yield token
            if isinstance(token, EOFToken):
                break

    def __iter__(self) -> Iterator[Token]:
        """Allow iteration over tokens using the tokenize method."""
        return iter(self.tokenize())
