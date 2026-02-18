"""Session management for tidesh."""

# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false
from __future__ import annotations

import dataclasses
import typing

import typing_extensions

from ._tidesh import ffi, lib
from .ast import ASTNode
from .exceptions import CommandNotFoundError, ParseError, SessionError
from .lexer import Lexer
from .state import Aliases, DirectoryStack, Environ, Features, History, Jobs, Terminal

if typing.TYPE_CHECKING:
    import types

    from .tokens import Token


@dataclasses.dataclass(frozen=True)
class CommandInfo:
    """Information about a command found in the PATH."""

    command: str
    """The command name."""


@dataclasses.dataclass(frozen=True)
class BuiltinCommandInfo(CommandInfo):
    """Information about a builtin command."""

    special: bool
    """Whether this is a special builtin."""


@dataclasses.dataclass(frozen=True)
class AliasCommandInfo(CommandInfo):
    """Information about an alias command."""

    target: str
    """The target command that the alias points to."""


@dataclasses.dataclass(frozen=True)
class ExternalCommandInfo(CommandInfo):
    """Information about an external command."""

    path: str
    """The full path to the external command."""


class Session:
    """
    Represents a tidesh shell session.

    A session maintains the state of a shell including history, environment variables,
    aliases, directory stack, and terminal properties. It can execute commands,
    capture output, and perform shell expansions.

    Attributes
    ----------
    history : History
        Command history manager.
    environ : Environ
        Environment variables mapping.
    aliases : Aliases
        Command aliases mapping.
    dirstack : DirectoryStack
        Directory stack for pushd/popd operations.
    terminal : Terminal
        Terminal properties and capabilities.
    jobs : Jobs
        Background job control manager.
    features : Features
        Runtime feature flags for conditional features.
    """

    _session: typing.Any
    """(internal) Pointer to the underlying C session structure."""

    def __init__(
        self,
        history_path: str | None = None,
        *,
        run_hooks: bool = True,
    ) -> None:
        """
        Initialize the shell session.

        Parameters
        ----------
        history_path : str | None, optional
            The path to the history file. If None, history will not be saved to disk.
        run_hooks : bool, default=True
            Whether to enable hook execution for this session.
        """
        super().__init__()
        c_path = history_path.encode() if history_path else ffi.NULL
        self._session = lib.init_session(ffi.NULL, c_path)
        if self._session == ffi.NULL:
            msg = "Failed to initialize session"
            raise SessionError(msg)

        # Set hook execution state
        self._session.hooks_disabled = not run_hooks

        self.history = History(self._session.history)
        """The command history manager for this session."""
        self.environ = Environ(self._session.environ)
        """The environment variables mapping for this session."""
        self.aliases = Aliases(self._session.aliases)
        """The command aliases mapping for this session."""
        self.dirstack = DirectoryStack(self._session.dirstack)
        """The directory stack for pushd/popd operations in this session."""
        self.terminal = Terminal(self._session.terminal)
        """The terminal properties and capabilities for this session."""
        self.jobs = Jobs(self._session.jobs)
        """The background job control manager for this session."""
        self.features = Features(self._session.features)
        """The runtime feature flags for this session."""

    def tokenize(self, command: str) -> typing.Iterable[Token]:
        """
        Tokenize a command string in the context of this session.

        Parameters
        ----------
        command : str
            The command string to tokenize.

        Yields
        ------
        Token
            An iterable of tokens representing the tokenized command.
        """
        lexer = Lexer(command, self)
        yield from lexer.tokenize()

    def parse(self, command: str) -> ASTNode:
        """
        Parse a command string into an abstract syntax tree (AST).

        Parameters
        ----------
        command : str
            The command string to parse.

        Returns
        -------
        ASTNode
            The root node of the parsed abstract syntax tree.
        """
        ast_ptr = lib.parse_command(command.encode(), self._session)
        if ast_ptr == ffi.NULL:
            msg = "Failed to parse command"
            raise ParseError(msg, command=command)
        return ASTNode(ast_ptr)

    def execute(self, command: str | ASTNode) -> int:
        """
        Execute a shell command.

        Parameters
        ----------
        command : str | ASTNode
            The command to execute.

        Returns
        -------
        int
            The exit code of the command.
        """
        if isinstance(command, ASTNode):
            return int(lib.execute(command._node, self._session))  # noqa: SLF001

        return int(lib.execute_string(command.encode(), self._session))

    def cd(self, *args: str) -> int:
        """
        Change the current working directory using the cd builtin.

        Parameters
        ----------
        *args : str
            Arguments for the cd command (usually just the path).

        Returns
        -------
        int
            The exit status.
        """
        c_args = [b"cd", *[a.encode() for a in args]]
        c_argv = ffi.new("char*[]", c_args)
        return bool(lib.builtin_cd(len(c_args), c_argv, self._session))

    def bg(self, *args: str) -> int:
        """
        Continue a stopped job in the background using the bg builtin.

        Parameters
        ----------
        *args : str
            Arguments for the bg command (job ID or job spec like %1, %%, %+, %-).

        Returns
        -------
        int
            The exit status.
        """
        c_args = [b"bg", *[a.encode() for a in args]]
        c_argv = ffi.new("char*[]", c_args)
        return int(lib.builtin_bg(len(c_args), c_argv, self._session))

    def fg(self, *args: str) -> int:
        """
        Bring a background job to the foreground using the fg builtin.

        Parameters
        ----------
        *args : str
            Arguments for the fg command (job ID or job spec like %1, %%, %+, %-).

        Returns
        -------
        int
            The exit status.
        """
        c_args = [b"fg", *[a.encode() for a in args]]
        c_argv = ffi.new("char*[]", c_args)
        return int(lib.builtin_fg(len(c_args), c_argv, self._session))

    def hooks(self, *args: str) -> int:
        """
        Manage hooks using the hooks builtin.

        Parameters
        ----------
        *args : str
            Arguments for the hooks command:
            - No args or 'list': List available hook files
            - 'enable': Enable hook execution
            - 'disable': Disable hook execution
            - 'status': Show hook execution status
            - 'run <hook_name>': Manually run a specific hook
            - 'path': Show hooks directory path
            - 'types': List all hook types

        Returns
        -------
        int
            The exit status.
        """
        c_args = [b"hooks", *[a.encode() for a in args]]
        c_argv = ffi.new("char*[]", c_args)
        return int(lib.builtin_hooks(len(c_args), c_argv, self._session))

    def run_hook(self, hook_name: str, env_vars: dict[str, str] | None = None) -> None:
        """
        Run a hook script from the current working directory's .tidesh-hooks folder.

        Parameters
        ----------
        hook_name : str
            Name of the hook to run (e.g., "before_cmd", "cd", "enter").
        env_vars : dict[str, str] | None, optional
            Additional environment variables to set for the hook execution.
        """
        if env_vars:
            # Create HookEnvVar array
            hook_vars = []
            for key, value in env_vars.items():
                hook_var = ffi.new("HookEnvVar *")
                hook_var.key = key.encode()
                hook_var.value = value.encode()
                hook_vars.append(hook_var)

            vars_array = ffi.new("HookEnvVar[]", hook_vars)
            lib.run_cwd_hook_with_vars(
                self._session,
                hook_name.encode(),
                vars_array,
                len(hook_vars),
            )
        else:
            lib.run_cwd_hook(self._session, hook_name.encode())

    def enable_hooks(self) -> None:
        """
        Enable hook execution for this session.

        This allows .tidesh-hooks scripts to run when their trigger events occur.
        """
        self._session.hooks_disabled = False

    def disable_hooks(self) -> None:
        """
        Disable hook execution for this session.

        This prevents .tidesh-hooks scripts from running. Useful for debugging
        or when you want to temporarily bypass hook behavior.
        """
        self._session.hooks_disabled = True

    @property
    def hooks_enabled(self) -> bool:
        """Whether hook execution is enabled for this session."""
        return not self._session.hooks_disabled

    @hooks_enabled.setter
    def hooks_enabled(self, enabled: bool) -> None:
        """Set whether hook execution is enabled for this session."""
        self._session.hooks_disabled = not enabled

    def list_jobs(self, *args: str) -> int:
        """
        List background jobs using the jobs builtin.

        Parameters
        ----------
        *args : str
            Arguments for the jobs command (optional).

        Returns
        -------
        int
            The exit status.
        """
        c_args = [b"jobs", *[a.encode() for a in args]]
        c_argv = ffi.new("char*[]", c_args)
        return int(lib.builtin_jobs(len(c_args), c_argv, self._session))

    def which(self, *args: str) -> list[CommandInfo]:
        """
        Locate commands and return information about their types and paths.

        Parameters
        ----------
        *args : str
            Command names to search for.

        Returns
        -------
        list[CommandInfo]
            A list of CommandInfo objects (or subclasses) containing:
            - CommandInfo: if the command was not found.
            - AliasCommandInfo: if the command is an alias.
            - BuiltinCommandInfo: if the command is a shell builtin.
            - ExternalCommandInfo: if the command is an external executable.
        """
        results: list[CommandInfo] = []
        for cmd in args:
            info = lib.get_command_info(cmd.encode(), self._session)

            if info.type == lib.COMMAND_ALIAS:
                target = ffi.string(info.path).decode() if info.path != ffi.NULL else ""
                results.append(AliasCommandInfo(command=cmd, target=target))
            elif info.type == lib.COMMAND_BUILTIN:
                results.append(BuiltinCommandInfo(command=cmd, special=False))
            elif info.type == lib.COMMAND_SPECIAL_BUILTIN:
                results.append(BuiltinCommandInfo(command=cmd, special=True))
            elif info.type == lib.COMMAND_EXTERNAL:
                path = ffi.string(info.path).decode() if info.path != ffi.NULL else ""
                results.append(ExternalCommandInfo(command=cmd, path=path))
            else:
                raise CommandNotFoundError(cmd)

            if info.path != ffi.NULL:
                lib.free(info.path)

        return results

    def capture(self, command: str) -> str | None:
        """
        Execute a command and capture its standard output.

        Parameters
        ----------
        command : str
            The command to execute.

        Returns
        -------
        str | None
            The captured output, or None if the command failed.
        """
        res_ptr = lib.execute_string_stdout(command.encode(), self._session)

        if res_ptr == ffi.NULL:
            return None
        try:
            return ffi.string(res_ptr).decode()
        finally:
            lib.free(res_ptr)

    def expand(self, text: str) -> list[str]:
        """
        Perform full shell expansion on text.

        This includes variable expansion, command substitution, tilde expansion,
        brace expansion, and glob expansion.

        Parameters
        ----------
        text : str
            The text to expand.

        Returns
        -------
        list[str]
            The list of expanded strings.
        """
        arr_ptr = lib.full_expansion(text.encode(), self._session)
        if arr_ptr == ffi.NULL:
            return []
        try:
            return [ffi.string(arr_ptr.items[i]).decode() for i in range(arr_ptr.count)]
        finally:
            lib.free_array(arr_ptr)
            lib.free(arr_ptr)

    def find_in_path(self, command: str) -> str | None:
        """
        Find a command in the PATH.

        Parameters
        ----------
        command : str
            The command name to search for.

        Returns
        -------
        str | None
            The full path to the command if found, None otherwise.
        """
        res = lib.find_in_path(command.encode(), self._session)
        if res == ffi.NULL:
            return None
        try:
            return ffi.string(res).decode()
        finally:
            lib.free(res)

    def pushd(self, path: str) -> bool:
        """
        Push a directory onto the directory stack and change to it.

        Parameters
        ----------
        path : str
            The directory path to push.

        Returns
        -------
        bool
            True if successful, False otherwise.
        """
        return bool(lib.dirstack_pushd(self._session.dirstack, path.encode()))

    def popd(self) -> bool:
        """
        Pop the top directory from the directory stack and change to it.

        Returns
        -------
        bool
            True if successful, False otherwise.
        """
        return bool(lib.dirstack_popd(self._session.dirstack))

    def update_working_dir(self) -> None:
        """
        Update the current working directory from the actual filesystem.

        This synchronizes the session's view of the current directory with
        the actual process working directory.
        """
        lib.update_working_dir(self._session)

    def update_path(self) -> None:
        """
        Update the cached PATH from the environment.

        This should be called after modifying the PATH environment variable
        to update the internal cache used for command lookup.
        """
        lib.update_path(self._session)

    @property
    def cwd(self) -> str:
        """The current working directory."""
        return ffi.string(self._session.current_working_dir).decode()

    @cwd.setter
    def cwd(self, path: str) -> None:
        """Set the current working directory."""
        self.cd(path)

    @property
    def previous_cwd(self) -> str:
        """The previous working directory."""
        return ffi.string(self._session.previous_working_dir).decode()

    def __del__(self) -> None:
        """Ensure resources are cleaned up when the session is garbage collected."""
        self.close()

    def close(self) -> None:
        """
        Close the session and free associated resources.

        After calling this method, the session cannot be used anymore.
        This is automatically called when the session is garbage collected
        or when used as a context manager.
        """
        if hasattr(self, "_session") and self._session != ffi.NULL:
            lib.free_session(self._session)
            self._session = ffi.NULL

    cleanup = close  # Alias for close method
    free = close  # Alias for close method

    def __enter__(self) -> typing_extensions.Self:
        """Enter the context manager."""
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: types.TracebackType | None,
    ) -> None:
        """Exit the context manager and clean up resources."""
        self.close()
