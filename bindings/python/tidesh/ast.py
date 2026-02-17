"""Abstract Syntax Tree (AST) representation for tidesh."""
# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false

import typing
from enum import IntEnum

from ._tidesh import ffi, lib


class NodeType(IntEnum):
    """Represents the type of an AST node in the shell command."""

    COMMAND = lib.NODE_COMMAND
    PIPE = lib.NODE_PIPE
    AND = lib.NODE_AND
    OR = lib.NODE_OR
    SEQUENCE = lib.NODE_SEQUENCE
    SUBSHELL = lib.NODE_SUBSHELL


class ASTNode:
    """
    Represents a node in the abstract syntax tree (AST) of a shell command.

    Attributes
    ----------
    type : NodeType
        The type of the AST node.
    background : bool
        Whether the command should run in the background.
    """

    def __init__(self, c_node: typing.Any) -> None:
        """
        Initialize the AST node from a C node struct.

        Parameters
        ----------
        c_node : typing.Any
            The C AST node structure from the FFI layer.
        """
        super().__init__()
        self._node = c_node
        self.type = NodeType(c_node.type)
        self.background = bool(c_node.background)

    @property
    def argv(self) -> list[str]:
        """
        Get the command arguments.

        Returns
        -------
        list[str]
            The command arguments if this is a COMMAND node, empty list otherwise.
        """
        if self.type != NodeType.COMMAND or self._node.argv == ffi.NULL:
            return []
        return [ffi.string(self._node.argv[i]).decode() for i in range(self._node.argc)]

    @property
    def left(self) -> "ASTNode | None":
        """
        Get the left child node.

        Returns
        -------
        ASTNode | None
            The left child node, or None if not present.
        """
        return ASTNode(self._node.left) if self._node.left != ffi.NULL else None

    @property
    def right(self) -> "ASTNode | None":
        """
        Get the right child node.

        Returns
        -------
        ASTNode | None
            The right child node, or None if not present.
        """
        return ASTNode(self._node.right) if self._node.right != ffi.NULL else None
