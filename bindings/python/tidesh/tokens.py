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
    IO_NUMBER = lib.TOKEN_IO_NUMBER
    COMMENT = lib.TOKEN_COMMENT
    ASSIGNMENT = lib.TOKEN_ASSIGNMENT
    PIPE = lib.TOKEN_PIPE
    REDIRECT_IN = lib.TOKEN_REDIRECT_IN
    FD_DUPLICATION = lib.TOKEN_FD_DUPLICATION
    PROCESS_SUBSTITUTION_IN = lib.TOKEN_PROCESS_SUBSTITUTION_IN
    HEREDOC = lib.TOKEN_HEREDOC
    HERESTRING = lib.TOKEN_HERESTRING
    REDIRECT_OUT = lib.TOKEN_REDIRECT_OUT
    REDIRECT_APPEND = lib.TOKEN_REDIRECT_APPEND
    REDIRECT_OUT_ERR = lib.TOKEN_REDIRECT_OUT_ERR
    PROCESS_SUBSTITUTION_OUT = lib.TOKEN_PROCESS_SUBSTITUTION_OUT
    BACKGROUND = lib.TOKEN_BACKGROUND
    SEQUENCE = lib.TOKEN_SEQUENCE
    OR = lib.TOKEN_OR
    SEMICOLON = lib.TOKEN_SEMICOLON
    LPAREN = lib.TOKEN_LPAREN
    RPAREN = lib.TOKEN_RPAREN
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
