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
    CASE = lib.NODE_CASE
    FUNCTION_DEF = lib.NODE_FUNCTION_DEF
    FOR = lib.NODE_FOR
    WHILE = lib.NODE_WHILE
    UNTIL = lib.NODE_UNTIL
    WITH = lib.NODE_WITH


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
            create_ast_node(self._branch.condition)
            if self._branch.condition != ffi.NULL
            else None
        )

    @property
    def body(self) -> ASTNode | None:
        """Get the body AST node."""
        return (
            create_ast_node(self._branch.body)
            if self._branch.body != ffi.NULL
            else None
        )

    def to_dict(self) -> dict[str, typing.Any]:
        """Convert branch to dictionary."""
        return {
            "condition": self.condition.to_dict() if self.condition else None,
            "body": self.body.to_dict() if self.body else None,
        }

    def __repr__(self) -> str:
        """String representation of branch."""
        return f"<ConditionalBranch has_condition={self.condition is not None}>"


class CaseItem:
    """
    Represents a single clause in a case statement.

    Attributes
    ----------
    patterns : list[str]
        List of pattern strings to match against.
    terminator : str
        How the clause is terminated (;;, ;&, or ;;&).
    body : ASTNode | None
        The commands executed when a pattern matches.
    """

    def __init__(self, c_item: typing.Any) -> None:
        """Initialize case item from C CaseItem struct."""
        self._item = c_item

    @property
    def patterns(self) -> list[str]:
        """Get patterns to match against."""
        if self._item.patterns == ffi.NULL:
            return []
        res: list[str] = []
        for i in range(self._item.pattern_count):
            if self._item.patterns[i] != ffi.NULL:
                res.append(ffi.string(self._item.patterns[i]).decode())
        return res

    @property
    def terminator(self) -> str:
        """Get the terminator string (;;, ;&, or ;;&)."""
        t = self._item.terminator
        if t == lib.CASE_TERM_FALLTHROUGH:
            return ";&"
        if t == lib.CASE_TERM_CONTINUE:
            return ";;&"
        return ";;"

    @property
    def body(self) -> ASTNode | None:
        """Get the clause body AST node."""
        return create_ast_node(self._item.body) if self._item.body != ffi.NULL else None

    def to_dict(self) -> dict[str, typing.Any]:
        """Convert case item to dictionary."""
        return {
            "patterns": self.patterns,
            "terminator": self.terminator,
            "body": self.body.to_dict() if self.body else None,
        }

    def __repr__(self) -> str:
        """String representation of case item."""
        return f"<CaseItem patterns={self.patterns!r} term={self.terminator!r}>"


class ASTNode:
    """
    Base class representing a node in the abstract syntax tree (AST) of a shell command.

    Attributes
    ----------
    type : NodeType
        The type of the AST node.
    background : bool
        Whether the command should run in the background.
    """

    def __new__(cls, c_node: typing.Any) -> ASTNode:
        """Instantiate the appropriate ASTNode subclass based on node type."""
        if c_node == ffi.NULL or c_node is None:
            msg = "Cannot create ASTNode from NULL pointer"
            raise ValueError(msg)
        if cls is not ASTNode:
            return super().__new__(cls)
        node_type = NodeType(c_node.type)
        subclass = _NODE_CLASS_MAP.get(node_type, ASTNode)
        return super().__new__(subclass)

    def __init__(self, c_node: typing.Any) -> None:
        """Initialize the AST node from a C node struct."""
        super().__init__()
        self._node = c_node
        self.type: NodeType = NodeType(c_node.type)
        self.background: bool = bool(c_node.background)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize the AST node recursively to a Python dictionary."""
        return {
            "type": self.type.name,
            "background": self.background,
        }

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

            if isinstance(node, CommandNode):
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

            if isinstance(node, FunctionDefNode):
                prefix = "".join(
                    space if cur_stack[i] else vertical for i in range(depth)
                )
                lines.append(f"{prefix}{branch}name: {node.name!r}")
                new_stack = cur_stack + [False]
                _format(node.body, "body", new_stack, depth + 1, True)
                return

            if isinstance(node, (BlockNode, SubshellNode)):
                new_stack = cur_stack + [True]
                _format(node.body, "body", new_stack, depth + 1, True)
                return

            if isinstance(node, ForNode):
                prefix = "".join(
                    space if cur_stack[i] else vertical for i in range(depth)
                )
                lines.append(f"{prefix}{branch}variable: {node.var_name!r}")
                if node.has_in:
                    lines.append(f"{prefix}{branch}words: {node.words!r}")
                new_stack = cur_stack + [False]
                _format(node.body, "body", new_stack, depth + 1, True)
                return

            if isinstance(node, WhileNode):
                new_stack = cur_stack + [False]
                _format(node.condition, "condition", new_stack, depth + 1, False)
                new_stack = cur_stack + [True]
                _format(node.body, "body", new_stack, depth + 1, True)
                return

            if isinstance(node, CaseNode):
                prefix = "".join(
                    space if cur_stack[i] else vertical for i in range(depth)
                )
                lines.append(f'{prefix}{branch}word: "{node.word}"')
                items = node.items
                i_total = len(items)
                for i_idx, item in enumerate(items):
                    i_last = i_idx == i_total - 1
                    conn = last_branch if i_last else branch
                    p_str = ", ".join(f'"{p}"' for p in item.patterns)
                    lines.append(
                        f'{prefix}{conn}clause: patterns=[{p_str}] terminator="{item.terminator}"',
                    )
                    if item.body:
                        new_stack = cur_stack + [i_last]
                        _format(item.body, "body", new_stack, depth + 1, True)
                return

            if isinstance(node, ConditionalNode):
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

            if isinstance(node, BinaryNode):
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
                return

        _format(self, None, [], 0, True)
        return "\n".join(lines)

    @property
    def json(self) -> str:
        """Serialize the AST node to a JSON string."""
        return self.to_json()

    def __repr__(self) -> str:
        """String representation of AST node."""
        return f"<{self.__class__.__name__} type={self.type.name}>"


class HasRedirectsMixin:
    """Mixin for AST nodes that support I/O redirections."""

    _node: typing.Any

    @property
    def redirects(self) -> list[Redirection]:
        """Get I/O redirections attached to this node."""
        res: list[Redirection] = []
        curr = self._node.redirects
        while curr != ffi.NULL:
            res.append(Redirection(curr))
            curr = curr.next
        return res


class CommandNode(HasRedirectsMixin, ASTNode):
    """Represents a simple command AST node (NODE_COMMAND)."""

    @property
    def argv(self) -> list[str]:
        """Get the command arguments."""
        if self._node.argv == ffi.NULL:
            return []
        return [
            ffi.string(self._node.argv[i]).decode()
            for i in range(self._node.argc)
            if self._node.argv[i] != ffi.NULL
        ]

    @property
    def assignments(self) -> list[str]:
        """Get variable assignments defined on this command node."""
        if self._node.assignments == ffi.NULL:
            return []
        arr = self._node.assignments
        return [ffi.string(arr.items[i]).decode() for i in range(arr.count)]

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize command node to dictionary."""
        data = super().to_dict()
        data["argv"] = self.argv
        data["argc"] = len(self.argv)
        data["assignments"] = self.assignments
        data["redirects"] = [r.to_dict() for r in self.redirects]
        return data

    def __repr__(self) -> str:
        """String representation of command node."""
        return f"<CommandNode argv={self.argv!r}>"


class BinaryNode(ASTNode):
    """Base class for binary AST nodes with left and right children."""

    @property
    def left(self) -> ASTNode | None:
        """Get the left child node."""
        return create_ast_node(self._node.left)

    @property
    def right(self) -> ASTNode | None:
        """Get the right child node."""
        return create_ast_node(self._node.right)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize binary node to dictionary."""
        data = super().to_dict()
        if self.left:
            data["left"] = self.left.to_dict()
        if self.right:
            data["right"] = self.right.to_dict()
        return data

    def __repr__(self) -> str:
        """String representation of binary node."""
        return f"<{self.__class__.__name__} left={self.left!r} right={self.right!r}>"


class PipeNode(BinaryNode):
    """A pipeline AST node (NODE_PIPE)."""


class AndNode(BinaryNode):
    """A logical AND sequence AST node (NODE_AND)."""


class OrNode(BinaryNode):
    """A logical OR sequence AST node (NODE_OR)."""


class SequenceNode(BinaryNode):
    """A command sequence AST node (NODE_SEQUENCE)."""


class BlockNode(HasRedirectsMixin, ASTNode):
    """A command block AST node ({ ... }, NODE_BLOCK)."""

    @property
    def body(self) -> ASTNode | None:
        """Get the block body AST node."""
        return create_ast_node(self._node.left)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize block node to dictionary."""
        data = super().to_dict()
        data["body"] = self.body.to_dict() if self.body else None
        return data

    def __repr__(self) -> str:
        """String representation of block node."""
        return f"<BlockNode body={self.body!r}>"


class SubshellNode(HasRedirectsMixin, ASTNode):
    """A subshell AST node (( ... ), NODE_SUBSHELL)."""

    @property
    def body(self) -> ASTNode | None:
        """Get the subshell body AST node."""
        return create_ast_node(self._node.left)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize subshell node to dictionary."""
        data = super().to_dict()
        data["body"] = self.body.to_dict() if self.body else None
        return data

    def __repr__(self) -> str:
        """String representation of subshell node."""
        return f"<SubshellNode body={self.body!r}>"


class FunctionDefNode(ASTNode):
    """A function definition AST node (NODE_FUNCTION_DEF)."""

    @property
    def name(self) -> str:
        """Get the function name."""
        if (
            self._node.argc > 0
            and self._node.argv != ffi.NULL
            and self._node.argv[0] != ffi.NULL
        ):
            return ffi.string(self._node.argv[0]).decode()
        return ""

    @property
    def body(self) -> ASTNode | None:
        """Get the function body AST node."""
        return create_ast_node(self._node.left)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize function definition node to dictionary."""
        data = super().to_dict()
        data["name"] = self.name
        data["body"] = self.body.to_dict() if self.body else None
        return data

    def __repr__(self) -> str:
        """String representation of function definition node."""
        return f"<FunctionDefNode name={self.name!r} body={self.body!r}>"


class ConditionalNode(ASTNode):
    """A conditional statement AST node (if/elif/else/fi, NODE_CONDITIONAL)."""

    @property
    def branches(self) -> list[ConditionalBranch]:
        """Get conditional branches for this statement."""
        res: list[ConditionalBranch] = []
        curr = self._node.branches
        while curr != ffi.NULL:
            res.append(ConditionalBranch(curr))
            curr = curr.next
        return res

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize conditional statement node to dictionary."""
        data = super().to_dict()
        data["branches"] = [b.to_dict() for b in self.branches]
        return data

    def __repr__(self) -> str:
        """String representation of conditional node."""
        return f"<ConditionalNode branches={len(self.branches)}>"


class ForNode(HasRedirectsMixin, ASTNode):
    """A for loop AST node (NODE_FOR)."""

    @property
    def var_name(self) -> str:
        """Get the loop variable name."""
        if (
            self._node.argc > 0
            and self._node.argv != ffi.NULL
            and self._node.argv[0] != ffi.NULL
        ):
            return ffi.string(self._node.argv[0]).decode()
        return ""

    @property
    def has_in(self) -> bool:
        """Check if this for loop has an explicit 'in' clause."""
        return bool(self._node.for_has_in)

    @property
    def words(self) -> list[str]:
        """Get words to iterate over."""
        if self._node.argv == ffi.NULL:
            return []
        res: list[str] = []
        for i in range(1, self._node.argc):
            if self._node.argv[i] != ffi.NULL:
                res.append(ffi.string(self._node.argv[i]).decode())
        return res

    @property
    def body(self) -> ASTNode | None:
        """Get the loop body AST node."""
        return create_ast_node(self._node.left)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize for loop node to dictionary."""
        data = super().to_dict()
        data["variable"] = self.var_name
        if self.has_in:
            data["words"] = self.words
        data["body"] = self.body.to_dict() if self.body else None
        return data

    def __repr__(self) -> str:
        """String representation of for loop node."""
        return f"<ForNode var={self.var_name!r} words={self.words!r}>"


class WhileNode(HasRedirectsMixin, ASTNode):
    """A while loop AST node (NODE_WHILE)."""

    @property
    def condition(self) -> ASTNode | None:
        """Get the loop condition AST node."""
        return create_ast_node(self._node.left)

    @property
    def body(self) -> ASTNode | None:
        """Get the loop body AST node."""
        return create_ast_node(self._node.right)

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize while loop node to dictionary."""
        data = super().to_dict()
        data["condition"] = self.condition.to_dict() if self.condition else None
        data["body"] = self.body.to_dict() if self.body else None
        return data

    def __repr__(self) -> str:
        """String representation of while loop node."""
        return f"<WhileNode condition={self.condition!r} body={self.body!r}>"


class CaseNode(HasRedirectsMixin, ASTNode):
    """A case statement AST node (NODE_CASE)."""

    @property
    def word(self) -> str:
        """Get the word being tested."""
        if (
            self._node.argc > 0
            and self._node.argv != ffi.NULL
            and self._node.argv[0] != ffi.NULL
        ):
            return ffi.string(self._node.argv[0]).decode()
        return ""

    @property
    def items(self) -> list[CaseItem]:
        """Get case items/clauses."""
        res: list[CaseItem] = []
        curr = self._node.case_items
        while curr != ffi.NULL:
            res.append(CaseItem(curr))
            curr = curr.next
        return res

    def to_dict(self) -> dict[str, typing.Any]:
        """Serialize case statement node to dictionary."""
        data = super().to_dict()
        data["word"] = self.word
        data["items"] = [item.to_dict() for item in self.items]
        return data

    def __repr__(self) -> str:
        """String representation of case node."""
        return f"<CaseNode word={self.word!r} items={len(self.items)}>"


class WithNode(ASTNode):
    """
    Represents a with context manager node in the AST.

    Attributes
    ----------
    body : ASTNode | None
        The commands executed inside the with block.
    """

    def __init__(self, c_node: typing.Any) -> None:
        super().__init__(c_node)
        self.body: ASTNode | None = (
            create_ast_node(c_node.left) if c_node.left != ffi.NULL else None
        )

    def __repr__(self) -> str:
        return f"<WithNode body={self.body!r}>"


_NODE_CLASS_MAP: dict[NodeType, type[ASTNode]] = {
    NodeType.COMMAND: CommandNode,
    NodeType.PIPE: PipeNode,
    NodeType.AND: AndNode,
    NodeType.OR: OrNode,
    NodeType.SEQUENCE: SequenceNode,
    NodeType.SUBSHELL: SubshellNode,
    NodeType.BLOCK: BlockNode,
    NodeType.CONDITIONAL: ConditionalNode,
    NodeType.CASE: CaseNode,
    NodeType.FUNCTION_DEF: FunctionDefNode,
    NodeType.FOR: ForNode,
    NodeType.WHILE: WhileNode,
    NodeType.WITH: WithNode,
}


def create_ast_node(c_node: typing.Any) -> ASTNode | None:
    """Create an ASTNode subclass instance from a C ASTNode struct, or None if NULL."""
    if c_node == ffi.NULL or c_node is None:
        return None
    return ASTNode(c_node)


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
