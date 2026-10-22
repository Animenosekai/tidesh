"""Models for tidesh."""

# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false

from __future__ import annotations

import typing
from enum import IntEnum

import typing_extensions as typing_ex

from ._tidesh import ffi, lib


class TokenType(IntEnum):
    """Represents the type of a token in the shell command."""

    WORD = lib.TOKEN_WORD
    COMMENT = lib.TOKEN_COMMENT
    IO_NUMBER = lib.TOKEN_IO_NUMBER
    ASSIGNMENT = lib.TOKEN_ASSIGNMENT
    PIPE = lib.TOKEN_PIPE
    REDIRECT_IN = lib.TOKEN_REDIRECT_IN
    FD_DUPLICATION = lib.TOKEN_FD_DUPLICATION
    HEREDOC = lib.TOKEN_HEREDOC
    HERESTRING = lib.TOKEN_HERESTRING
    REDIRECT_OUT = lib.TOKEN_REDIRECT_OUT
    REDIRECT_APPEND = lib.TOKEN_REDIRECT_APPEND
    REDIRECT_OUT_ERR = lib.TOKEN_REDIRECT_OUT_ERR
    REDIRECT_ALL = lib.TOKEN_REDIRECT_ALL
    REDIRECT_APPEND_ERR = lib.TOKEN_REDIRECT_APPEND_ERR
    PROCESS_SUBSTITUTION_IN = lib.TOKEN_PROCESS_SUBSTITUTION_IN
    PROCESS_SUBSTITUTION_OUT = lib.TOKEN_PROCESS_SUBSTITUTION_OUT
    BACKGROUND = lib.TOKEN_BACKGROUND
    SEQUENCE = lib.TOKEN_SEQUENCE
    OR = lib.TOKEN_OR
    SEMICOLON = lib.TOKEN_SEMICOLON
    LPAREN = lib.TOKEN_LPAREN
    RPAREN = lib.TOKEN_RPAREN
    LBRACE = lib.TOKEN_LBRACE
    RBRACE = lib.TOKEN_RBRACE
    IF = lib.TOKEN_IF
    THEN = lib.TOKEN_THEN
    ELSE = lib.TOKEN_ELSE
    ELIF = lib.TOKEN_ELIF
    FI = lib.TOKEN_FI
    EOL = lib.TOKEN_EOL
    EOF = lib.TOKEN_EOF


class Token:
    """
    Base class for all tokens in the shell command.

    Attributes
    ----------
    type : TokenType
        The type of the token.
    """

    def __init__(self, c_token: typing.Any) -> None:
        """
        Initialize the token from a C token struct.

        Parameters
        ----------
        c_token : typing.Any
            The C token structure from the FFI layer.
        """
        super().__init__()
        self.type = TokenType(c_token.type)

    @typing_ex.override
    def __repr__(self) -> str:
        """Return a string representation of the token."""
        return f"<{self.__class__.__name__}>"

    @property
    def value(self) -> str | None:
        """Get the token value string, or None if not applicable."""
        return getattr(self, "_value", None)

    @property
    def extra(self) -> str | None:
        """Get extra data associated with the token, or None if not applicable."""
        return getattr(self, "_extra", None)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize token to a dictionary."""
        return {
            "type": self.type.name,
            "value": self.value,
            "extra": self.extra,
        }

    def to_json(self, indent: int | None = 2) -> str:
        """Serialize token to a JSON string."""
        import json

        return json.dumps(self.to_dict(), indent=indent)


class ValuedToken(Token):
    """
    Represents a token in the shell command with a value.

    Attributes
    ----------
    _value : str
        The raw value of the token from the C lexer.
    """

    def __init__(self, c_token: typing.Any) -> None:
        """
        Initialize the token from a C token struct.

        Parameters
        ----------
        c_token : typing.Any
            The C token structure from the FFI layer.
        """
        super().__init__(c_token)
        self._value: str = ffi.string(c_token.value).decode()  # pyright: ignore[reportUnknownMemberType]

    @typing_ex.override
    def __repr__(self) -> str:
        """Return a string representation of the token with its value."""
        return f"<{self.__class__.__name__}: {self._value!r}>"


class ExtraValuedToken(ValuedToken):
    """
    Represents a token in the shell command with both value and extra data.

    Attributes
    ----------
    _value : str
        The raw value of the token from the C lexer.
    _extra : str
        The raw extra data associated with the token from the C lexer.
    """

    def __init__(self, c_token: typing.Any) -> None:
        """
        Initialize the token from a C token struct.

        Parameters
        ----------
        c_token : typing.Any
            The C token structure from the FFI layer.
        """
        super().__init__(c_token)
        self._extra: str = ffi.string(c_token.extra).decode()  # pyright: ignore[reportUnknownMemberType]

    @typing_ex.override
    def __repr__(self) -> str:
        """Return a string representation of the token with its value and extra data."""
        return f"<{self.__class__.__name__}: {self._value!r}, extra={self._extra!r}>"


# Specific Token Classes
class WordToken(ValuedToken):
    """A word token (WORD)."""


class IONumberToken(ValuedToken):
    """An IO number token (IO_NUMBER)."""

    @property
    def number(self) -> int:
        """Get the IO number as an integer."""
        return int(self._value)


class CommentToken(ValuedToken):
    """A comment token (COMMENT)."""

    @property
    def text(self) -> str:
        """Get the comment text."""
        return self._value


class AssignmentToken(ExtraValuedToken):
    """An assignment token (ASSIGNMENT)."""

    @property
    def var_name(self) -> str:
        """Get the variable name."""
        return self._value

    @property
    def assigned_value(self) -> str:
        """Get the assigned value."""
        return self._extra


class PipeToken(Token):
    """A pipe token (PIPE)."""


class RedirectInToken(Token):
    """A redirect input token (REDIRECT_IN)."""


class FDDuplicationToken(Token):
    """A file descriptor duplication token (FD_DUPLICATION)."""


class ProcessSubstitutionInToken(ValuedToken):
    """A process substitution input token (PROCESS_SUBSTITUTION_IN)."""

    @property
    def command(self) -> str:
        """Get the command for process substitution."""
        return self._value


class HeredocToken(ValuedToken):
    """A heredoc token (HEREDOC)."""

    @property
    def content(self) -> str:
        """Get the heredoc content."""
        return self._value


class HereStringToken(ValuedToken):
    """A herestring token (HERESTRING)."""

    @property
    def word(self) -> str:
        """Get the word for herestring input."""
        return self._value


class RedirectOutToken(Token):
    """A redirect output token (REDIRECT_OUT)."""


class RedirectAppendToken(Token):
    """A redirect append token (REDIRECT_APPEND)."""


class RedirectOutErrToken(Token):
    """A redirect output error token (REDIRECT_OUT_ERR)."""


class ProcessSubstitutionOutToken(ValuedToken):
    """A process substitution output token (PROCESS_SUBSTITUTION_OUT)."""

    @property
    def command(self) -> str:
        """Get the command for process substitution."""
        return self._value


class BackgroundToken(Token):
    """A background token (BACKGROUND)."""


class SequenceToken(Token):
    """A sequence token (SEQUENCE)."""


class OrToken(Token):
    """An OR token (OR)."""


class SemicolonToken(Token):
    """A semicolon token (SEMICOLON)."""


class LParenToken(Token):
    """A left parenthesis token (LPAREN)."""


class RParenToken(Token):
    """A right parenthesis token (RPAREN)."""


class LBraceToken(Token):
    """A left brace token (LBRACE)."""


class RBraceToken(Token):
    """A right brace token (RBRACE)."""


class IfToken(Token):
    """An if token (IF)."""


class ThenToken(Token):
    """A then token (THEN)."""


class ElseToken(Token):
    """An else token (ELSE)."""


class ElifToken(Token):
    """An elif token (ELIF)."""


class FiToken(Token):
    """A fi token (FI)."""


class EOLToken(Token):
    """An end-of-line token (EOL)."""


class EOFToken(Token):
    """An end-of-file token (EOF)."""


# Mapping from TokenType to Token class
_TOKEN_CLASS_MAP = {
    TokenType.WORD: WordToken,
    TokenType.IO_NUMBER: IONumberToken,
    TokenType.COMMENT: CommentToken,
    TokenType.ASSIGNMENT: AssignmentToken,
    TokenType.PIPE: PipeToken,
    TokenType.REDIRECT_IN: RedirectInToken,
    TokenType.FD_DUPLICATION: FDDuplicationToken,
    TokenType.PROCESS_SUBSTITUTION_IN: ProcessSubstitutionInToken,
    TokenType.HEREDOC: HeredocToken,
    TokenType.HERESTRING: HereStringToken,
    TokenType.REDIRECT_OUT: RedirectOutToken,
    TokenType.REDIRECT_APPEND: RedirectAppendToken,
    TokenType.REDIRECT_OUT_ERR: RedirectOutErrToken,
    TokenType.PROCESS_SUBSTITUTION_OUT: ProcessSubstitutionOutToken,
    TokenType.BACKGROUND: BackgroundToken,
    TokenType.SEQUENCE: SequenceToken,
    TokenType.OR: OrToken,
    TokenType.SEMICOLON: SemicolonToken,
    TokenType.LPAREN: LParenToken,
    TokenType.RPAREN: RParenToken,
    TokenType.LBRACE: LBraceToken,
    TokenType.RBRACE: RBraceToken,
    TokenType.IF: IfToken,
    TokenType.THEN: ThenToken,
    TokenType.ELSE: ElseToken,
    TokenType.ELIF: ElifToken,
    TokenType.FI: FiToken,
    TokenType.EOL: EOLToken,
    TokenType.EOF: EOFToken,
}


def create_token(c_token: typing.Any) -> Token:
    """
    Create a token instance of the appropriate class based on the token type.

    Parameters
    ----------
    c_token : typing.Any
        The C token structure from the FFI layer.

    Returns
    -------
    Token
        A token instance of the appropriate type.
    """
    token_type = TokenType(c_token.type)
    token_class = _TOKEN_CLASS_MAP.get(token_type, Token)
    return token_class(c_token)
