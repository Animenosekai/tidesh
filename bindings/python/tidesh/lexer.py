"""Lexer for tidesh."""

# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false

from __future__ import annotations

import json
import typing
from collections.abc import Iterator, Sequence

from ._tidesh import ffi, lib
from .tokens import EOFToken, Token, create_token

if typing.TYPE_CHECKING:
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

    def to_json(self, indent: int | None = 2) -> str:
        """Serialize all tokens in this command to a JSON string."""
        import json

        return json.dumps([t.to_dict() for t in self.tokenize()], indent=indent)

    def pretty(self) -> str:
        """Format all tokens in this command as a pretty-printed string."""
        lines = ["Token Stream:"]
        for idx, token in enumerate(self.tokenize()):
            val_part = f' value="{token.value}"' if token.value is not None else ""
            extra_part = f' extra="{token.extra}"' if token.extra is not None else ""
            lines.append(f"  [{idx:03d}] {token.type.name:<24}{val_part}{extra_part}")
        return "\n".join(lines)


class LexResult(Sequence[Token]):
    """
    Result of lexing a shell command string.

    Implements the Sequence protocol over the token stream, and provides
    .json and .pretty properties for serialization.
    """

    def __init__(self, tokens: list[Token]) -> None:
        super().__init__()
        self._tokens: list[Token] = tokens

    @property
    def tokens(self) -> list[Token]:
        """Get the list of tokens."""
        return self._tokens

    @property
    def json(self) -> str:
        """Get the JSON string representation of the token stream."""
        return json.dumps([t.to_dict() for t in self._tokens], indent=2)

    @property
    def pretty(self) -> str:
        """Get the pretty-printed representation of the token stream."""
        lines = ["Token Stream:"]
        for idx, token in enumerate(self._tokens):
            val_part = f' value="{token.value}"' if token.value is not None else ""
            extra_part = f' extra="{token.extra}"' if token.extra is not None else ""
            lines.append(f"  [{idx:03d}] {token.type.name:<24}{val_part}{extra_part}")
        return "\n".join(lines)

    def __len__(self) -> int:
        return len(self._tokens)

    @typing.overload
    def __getitem__(self, index: int) -> Token: ...

    @typing.overload
    def __getitem__(self, index: slice) -> Sequence[Token]: ...

    def __getitem__(self, index: int | slice) -> Token | Sequence[Token]:
        return self._tokens[index]

    def __iter__(self) -> Iterator[Token]:
        return iter(self._tokens)

    def __repr__(self) -> str:
        return f"<LexResult tokens={len(self._tokens)}>"
