"""Abstract Syntax Tree (AST) representation for tidesh."""
# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false

from __future__ import annotations

import json
import typing
from enum import IntEnum

from ._tidesh import ffi, lib
from .tokens import TokenType


class NodeType(IntEnum):
    """Represents the type of an AST node in the shell command."""

    COMMAND = lib.NODE_COMMAND
    PIPE = lib.NODE_PIPE
    AND = lib.NODE_AND
    OR = lib.NODE_OR
    SEQUENCE = lib.NODE_SEQUENCE
    SUBSHELL = lib.NODE_SUBSHELL
    BLOCK = lib.NODE_BLOCK
    CONDITIONAL = lib.NODE_CONDITIONAL
    FUNCTION_DEF = lib.NODE_FUNCTION_DEF
    FOR = lib.NODE_FOR
    WHILE = lib.NODE_WHILE


class Redirection:
    """
    Represents an I/O redirection in an AST node.

    Attributes
    ----------
    fd : int
        The file descriptor being redirected.
    type : TokenType
        The redirection operator type.
    target : str
        The redirection target file or descriptor.
    is_process_substitution : bool
        Whether this redirection uses process substitution.
    """

    def __init__(self, c_redirect: typing.Any) -> None:
        """Initialize redirection from C Redirection struct."""
        self.fd: int = int(c_redirect.fd)
        self.type: TokenType = TokenType(c_redirect.type)
        self.target: str = (
            ffi.string(c_redirect.target).decode()
            if c_redirect.target != ffi.NULL
            else ""
        )
        self.is_process_substitution: bool = bool(c_redirect.is_process_substitution)

    def to_dict(self) -> dict[str, typing.Any]:
        """Convert redirection to dictionary."""
        return {
            "fd": self.fd,
            "type": self.type.name,
            "target": self.target,
            "is_process_substitution": self.is_process_substitution,
        }

    def __repr__(self) -> str:
        """String representation of redirection."""
        return (
            f"<Redirection fd={self.fd} type={self.type.name} target={self.target!r}>"
        )


class ConditionalBranch:
    """
    Represents a branch in an if-elif-else conditional statement.

    Attributes
    ----------
    condition : ASTNode | None
        The condition AST node (None for 'else' branches).
    body : ASTNode | None
        The body AST node executed when this branch succeeds.
    """

    def __init__(self, c_branch: typing.Any) -> None:
        """Initialize conditional branch from C ConditionalBranch struct."""
        self._branch = c_branch

    @property
    def condition(self) -> ASTNode | None:
        """Get the condition AST node, or None for else branches."""
        return (
            ASTNode(self._branch.condition)
            if self._branch.condition != ffi.NULL
            else None
        )

    @property
    def body(self) -> ASTNode | None:
        """Get the body AST node."""
        return ASTNode(self._branch.body) if self._branch.body != ffi.NULL else None

    def to_dict(self) -> dict[str, typing.Any]:
        """Convert branch to dictionary."""
        return {
            "condition": self.condition.to_dict() if self.condition else None,
            "body": self.body.to_dict() if self.body else None,
        }

    def __repr__(self) -> str:
        """String representation of branch."""
        return f"<ConditionalBranch has_condition={self.condition is not None}>"


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
            The command arguments if this is a COMMAND or FUNCTION_DEF node, empty list otherwise.
        """
        if self._node.argv == ffi.NULL:
            return []
        return [ffi.string(self._node.argv[i]).decode() for i in range(self._node.argc)]

    @property
    def name(self) -> str | None:
        """Get the function name for FUNCTION_DEF nodes."""
        if (
            self.type == NodeType.FUNCTION_DEF
            and self._node.argc > 0
            and self._node.argv != ffi.NULL
        ):
            return ffi.string(self._node.argv[0]).decode()
        return None

    @property
    def left(self) -> ASTNode | None:
        """
        Get the left child node.

        Returns
        -------
        ASTNode | None
            The left child node, or None if not present.
        """
        return ASTNode(self._node.left) if self._node.left != ffi.NULL else None

    @property
    def right(self) -> ASTNode | None:
        """
        Get the right child node.

        Returns
        -------
        ASTNode | None
            The right child node, or None if not present.
        """
        return ASTNode(self._node.right) if self._node.right != ffi.NULL else None

    @property
    def condition(self) -> ASTNode | None:
        """Get the condition AST node for WHILE nodes."""
        if self.type == NodeType.WHILE:
            return self.left
        return None

    @property
    def body(self) -> ASTNode | None:
        """Get the body AST node for blocks, subshells, loops, and function definitions."""
        if self.type in (
            NodeType.BLOCK,
            NodeType.SUBSHELL,
            NodeType.FUNCTION_DEF,
            NodeType.FOR,
        ):
            return self.left
        if self.type == NodeType.WHILE:
            return self.right
        return None

    @property
    def assignments(self) -> list[str]:
        """Get variable assignments defined on this node."""
        if self._node.assignments == ffi.NULL:
            return []
        arr = self._node.assignments
        return [ffi.string(arr.items[i]).decode() for i in range(arr.count)]

    @property
    def redirects(self) -> list[Redirection]:
        """Get I/O redirections attached to this node."""
        res: list[Redirection] = []
        curr = self._node.redirects
        while curr != ffi.NULL:
            res.append(Redirection(curr))
            curr = curr.next
        return res

    @property
    def branches(self) -> list[ConditionalBranch]:
        """Get conditional branches for CONDITIONAL nodes."""
        if self.type != NodeType.CONDITIONAL or self._node.branches == ffi.NULL:
            return []
        res: list[ConditionalBranch] = []
        curr = self._node.branches
        while curr != ffi.NULL:
            res.append(ConditionalBranch(curr))
            curr = curr.next
        return res

    @property
    def has_in(self) -> bool:
        """Check if this for loop has an explicit 'in' clause."""
        if self.type != NodeType.FOR:
            return False
        return bool(self._node.for_has_in)

    @property
    def var_name(self) -> str | None:
        """Get loop variable name for FOR nodes."""
        if (
            self.type != NodeType.FOR
            or self._node.argc == 0
            or self._node.argv == ffi.NULL
        ):
            return None
        return (
            ffi.string(self._node.argv[0]).decode()
            if self._node.argv[0] != ffi.NULL
            else None
        )

    @property
    def words(self) -> list[str]:
        """Get words to iterate over for FOR nodes."""
        if self.type != NodeType.FOR or self._node.argv == ffi.NULL:
            return []
        res: list[str] = []
        for i in range(1, self._node.argc):
            if self._node.argv[i] != ffi.NULL:
                res.append(ffi.string(self._node.argv[i]).decode())
        return res

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize the AST node recursively to a Python dictionary."""
        data: dict[str, typing.Any] = {
            "type": self.type.name,
            "background": self.background,
        }

        if self.type == NodeType.COMMAND:
            data["argv"] = self.argv
            data["argc"] = len(self.argv)
            data["assignments"] = self.assignments
            data["redirects"] = [r.to_dict() for r in self.redirects]
        elif self.type == NodeType.FUNCTION_DEF:
            data["name"] = self.name
            data["body"] = self.body.to_dict() if self.body else None
        elif self.type in (NodeType.BLOCK, NodeType.SUBSHELL):
            data["body"] = self.body.to_dict() if self.body else None
        elif self.type == NodeType.CONDITIONAL:
            data["branches"] = [b.to_dict() for b in self.branches]
        elif self.type == NodeType.FOR:
            data["variable"] = self.var_name
            if self.has_in:
                data["words"] = self.words
            data["body"] = self.body.to_dict() if self.body else None
        elif self.type == NodeType.WHILE:
            data["condition"] = self.condition.to_dict() if self.condition else None
            data["body"] = self.body.to_dict() if self.body else None
        else:
            if self.left:
                data["left"] = self.left.to_dict()
            if self.right:
                data["right"] = self.right.to_dict()

        return data

    def to_json(self, indent: int | None = 2) -> str:
        """Serialize the AST node to a JSON string."""
        return json.dumps(self.to_dict(), indent=indent)

    def pretty(self, *, ascii: bool = False) -> str:
        """Return a pretty-printed tree representation using rounded Unicode or ASCII box characters."""
        branch = "|-- " if ascii else "├── "
        last_branch = "\\-- " if ascii else "╰── "
        vertical = "|   " if ascii else "│   "
        space = "    "

        lines: list[str] = []

        def _format(
            node: ASTNode | None,
            label: str | None,
            is_last_stack: list[bool],
            depth: int,
            is_last: bool,
        ) -> None:
            if node is None:
                if depth == 0:
                    lines.append("(null)")
                else:
                    prefix = "".join(
                        space if is_last_stack[i] else vertical
                        for i in range(depth - 1)
                    )
                    conn = last_branch if is_last else branch
                    tag = f"{label}: " if label else ""
                    lines.append(f"{prefix}{conn}{tag}(null)")
                return

            tag = f"{label}: " if label else ""
            node_hdr = f"{tag}{node.type.name}"
            if node.background:
                node_hdr += " [background]"

            if depth == 0:
                lines.append(node_hdr)
            else:
                prefix = "".join(
                    space if is_last_stack[i] else vertical for i in range(depth - 1)
                )
                conn = last_branch if is_last else branch
                lines.append(f"{prefix}{conn}{node_hdr}")

            cur_stack = is_last_stack[: depth - 1] + [is_last] if depth > 0 else []

            if node.type == NodeType.COMMAND:
                children: list[str] = []
                if node.assignments:
                    children.append(f"assignments: {node.assignments!r}")
                if node.argv:
                    children.append(f"argv: {node.argv!r}")

                total = len(children) + (1 if node.redirects else 0)
                curr = 0

                for text in children:
                    curr += 1
                    child_last = curr == total
                    prefix = "".join(
                        space if cur_stack[i] else vertical for i in range(depth)
                    )
                    conn = last_branch if child_last else branch
                    lines.append(f"{prefix}{conn}{text}")

                if node.redirects:
                    curr += 1
                    child_last = curr == total
                    prefix = "".join(
                        space if cur_stack[i] else vertical for i in range(depth)
                    )
                    conn = last_branch if child_last else branch
                    lines.append(f"{prefix}{conn}redirects:")

                    new_stack = cur_stack + [child_last]
                    r_total = len(node.redirects)
                    for r_idx, r in enumerate(node.redirects):
                        r_last = r_idx == r_total - 1
                        r_prefix = "".join(
                            space if new_stack[i] else vertical
                            for i in range(depth + 1)
                        )
                        r_conn = last_branch if r_last else branch
                        lines.append(
                            f"{r_prefix}{r_conn}fd {r.fd} ({r.type.name}) -> {r.target!r}",
                        )
                return

            if node.type == NodeType.FUNCTION_DEF:
                prefix = "".join(
                    space if cur_stack[i] else vertical for i in range(depth)
                )
                lines.append(f"{prefix}{branch}name: {node.name!r}")
                new_stack = cur_stack + [False]
                _format(node.left, "body", new_stack, depth + 1, True)
                return

            if node.type in (NodeType.BLOCK, NodeType.SUBSHELL):
                new_stack = cur_stack + [True]
                _format(node.left, "body", new_stack, depth + 1, True)
                return

            if node.type == NodeType.FOR:
                prefix = "".join(
                    space if cur_stack[i] else vertical for i in range(depth)
                )
                lines.append(f"{prefix}{branch}variable: {node.var_name!r}")
                if node.has_in:
                    lines.append(f"{prefix}{branch}words: {node.words!r}")
                new_stack = cur_stack + [False]
                _format(node.left, "body", new_stack, depth + 1, True)
                return

            if node.type == NodeType.WHILE:
                new_stack = cur_stack + [False]
                _format(node.left, "condition", new_stack, depth + 1, False)
                new_stack = cur_stack + [True]
                _format(node.right, "body", new_stack, depth + 1, True)
                return

            if node.type == NodeType.CONDITIONAL:
                branches = node.branches
                b_total = len(branches)
                for b_idx, b in enumerate(branches):
                    b_last = b_idx == b_total - 1
                    prefix = "".join(
                        space if cur_stack[i] else vertical for i in range(depth)
                    )
                    conn = last_branch if b_last else branch
                    hdr = (
                        f"branch {b_idx} (if/elif)"
                        if b.condition
                        else f"branch {b_idx} (else)"
                    )
                    lines.append(f"{prefix}{conn}{hdr}")

                    new_stack = cur_stack + [b_last]
                    if b.condition:
                        _format(b.condition, "condition", new_stack, depth + 1, False)
                        _format(b.body, "body", new_stack, depth + 1, True)
                    else:
                        _format(b.body, "body", new_stack, depth + 1, True)
                return

            if node.left and node.right:
                new_stack = cur_stack + [False]
                _format(node.left, "left", new_stack, depth + 1, False)
                _format(node.right, "right", new_stack, depth + 1, True)
            elif node.left:
                new_stack = cur_stack + [True]
                _format(node.left, "left", new_stack, depth + 1, True)
            elif node.right:
                new_stack = cur_stack + [True]
                _format(node.right, "right", new_stack, depth + 1, True)

        _format(self, None, [], 0, True)
        return "\n".join(lines)

    @property
    def json(self) -> str:
        """Serialize the AST node to a JSON string."""
        return self.to_json()


class PrettyString(str):
    """
    A string subclass that represents pretty-printed output.

    Acts as a string by default (with rounded Unicode characters), but can be called
    with ascii=True to obtain ASCII characters.
    """

    def __new__(cls, value: str, ascii_value: str) -> PrettyString:
        s = super().__new__(cls, value)
        s._ascii = ascii_value
        return s

    def __call__(self, *, ascii: bool = False) -> str:
        return self._ascii if ascii else self


class ASTResult:
    """
    Result of parsing a shell command into an Abstract Syntax Tree (AST).

    Wraps the root ASTNode (if any) and provides .json and .pretty properties,
    as well as forwarding attribute access to the root AST node.
    """

    def __init__(self, root: ASTNode | None) -> None:
        super().__init__()
        self.root: ASTNode | None = root

    @property
    def node(self) -> ASTNode | None:
        """Get the root ASTNode, or None if the AST is empty."""
        return self.root

    @property
    def json(self) -> str:
        """Get the JSON string representation of the AST."""
        if self.root is None:
            return "null"
        return self.root.to_json()

    @property
    def pretty(self) -> PrettyString:
        """Get the pretty-printed tree representation (Unicode by default, or callable with ascii=True)."""
        if self.root is None:
            return PrettyString("(empty AST)", "(empty AST)")
        return PrettyString(self.root.pretty(ascii=False), self.root.pretty(ascii=True))

    @property
    def ascii(self) -> str:
        """Get the ASCII box-drawing tree representation."""
        if self.root is None:
            return "(empty AST)"
        return self.root.pretty(ascii=True)

    def __bool__(self) -> bool:
        """Return True if the AST is not empty."""
        return self.root is not None

    def __getattr__(self, name: str) -> typing.Any:
        """Forward attribute access to the root ASTNode."""
        if self.root is not None and hasattr(self.root, name):
            return getattr(self.root, name)
        msg = f"'{self.__class__.__name__}' object has no attribute '{name}'"
        raise AttributeError(msg)

    def __repr__(self) -> str:
        if self.root is None:
            return "<ASTResult (empty)>"
        return f"<ASTResult type={self.root.type.name}>"
