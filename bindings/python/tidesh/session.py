"""Session management for tidesh."""

# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false
from __future__ import annotations

import contextlib
import dataclasses
import os
import typing
from collections.abc import Sequence

import typing_extensions

from ._tidesh import ffi, lib
from .ast import ASTNode, ASTResult
from .exceptions import CommandNotFoundError, ParseError, SessionError
from .lexer import Lexer, LexResult
from .state import (
    Aliases,
    DirectoryStack,
    Environ,
    Features,
    Functions,
    History,
    Jobs,
    Options,
    Terminal,
)

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
class FunctionCommandInfo(CommandInfo):
    """Information about a shell function."""

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


def _build_argv(*args: str) -> tuple[list[typing.Any], typing.Any]:
    """
    Build a C-compatible argv array from Python strings.

    Returns the list of allocated C string pointers and the argv array pointer.
    Both must be kept alive for the duration of the C call.
    """
    c_args = [ffi.new("char[]", a.encode()) for a in args]
    c_argv = ffi.new("char*[]", [*c_args, ffi.NULL])
    return c_args, c_argv


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
        self.functions = Functions(self._session.functions)
        """The shell functions mapping for this session."""
        self.dirstack = DirectoryStack(self._session.dirstack)
        """The directory stack for pushd/popd operations in this session."""
        self.terminal = Terminal(self._session.terminal)
        """The terminal properties and capabilities for this session."""
        self.jobs = Jobs(self._session.jobs)
        """The background job control manager for this session."""
        self.features = Features(self._session.features)
        """The runtime feature flags for this session."""
        self.options = Options(self._session.options)
        """The runtime shell options (errexit, xtrace, etc.) for this session."""

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
        lexer = Lexer(command, self)
        ast_ptr = lib.parse(lexer._lexer, self._session)
        if ast_ptr == ffi.NULL:
            msg = "Failed to parse command"
            raise ParseError(msg, command=command)
        return ASTNode(ast_ptr)

    def lex(self, command: str) -> LexResult:
        """
        Tokenize a command string in this session.

        Parameters
        ----------
        command : str
            The shell command to tokenize.

        Returns
        -------
        LexResult
            A sequence of tokens with .json and .pretty properties.
        """
        lexer = Lexer(command, self)
        return LexResult(list(lexer))

    def ast(self, command: str) -> ASTResult:
        """
        Parse a command string into an Abstract Syntax Tree (AST).

        Parameters
        ----------
        command : str
            The shell command to parse.

        Returns
        -------
        ASTResult
            A wrapper around the root ASTNode with .json and .pretty properties.
        """
        lexer = Lexer(command, self)
        ast_ptr = lib.parse(lexer._lexer, self._session)
        root = ASTNode(ast_ptr) if ast_ptr != ffi.NULL else None
        return ASTResult(root)

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

    run = execute  # Alias for execute method
    __call__ = execute  # Make session callable: session("command")

    def cd(self, path: str | os.PathLike[str] | None = None, *args: str) -> int:
        """
        Change the current working directory using the cd builtin.

        Parameters
        ----------
        path : str | os.PathLike[str] | None, optional
            The directory path to change to. If None, changes to HOME (~).
        *args : str
            Additional arguments or flags for the cd command.

        Returns
        -------
        int
            The exit status.
        """
        cmd_args: list[str] = []
        if path is not None:
            cmd_args.append(os.fspath(path))
        if args:
            cmd_args.extend(args)
        c_args, c_argv = _build_argv("cd", *cmd_args)
        return int(lib.builtin_cd(len(c_args), c_argv, self._session))

    def set(
        self,
        *args: str,
        errexit: bool | None = None,
        xtrace: bool | None = None,
        nounset: bool | None = None,
        noglob: bool | None = None,
        verbose: bool | None = None,
        noexec: bool | None = None,
        allexport: bool | None = None,
        noclobber: bool | None = None,
        monitor: bool | None = None,
        pipefail: bool | None = None,
        positional: Sequence[str] | None = None,
        **options: bool,
    ) -> int:
        """
        Set or unset shell options and positional parameters, or show variables.

        Parameters
        ----------
        *args : str
            Raw arguments or option flags (e.g. '-e', '-x', '+f').
        errexit : bool | None, optional
            Exit immediately if a command exits with a non-zero status (-e / -o errexit).
        xtrace : bool | None, optional
            Print commands and their arguments as they are executed (-x / -o xtrace).
        nounset : bool | None, optional
            Treat unset variables as an error when substituting (-u / -o nounset).
        noglob : bool | None, optional
            Disable pathname expansion (-f / -o noglob).
        verbose : bool | None, optional
            Print shell input lines as they are read (-v / -o verbose).
        noexec : bool | None, optional
            Read commands but do not execute them (-n / -o noexec).
        allexport : bool | None, optional
            Automatically mark variables which are modified or created for export (-a / -o allexport).
        noclobber : bool | None, optional
            Prevent output redirection from overwriting existing files (-C / -o noclobber).
        monitor : bool | None, optional
            Enable job control (-m / -o monitor).
        pipefail : bool | None, optional
            Pipeline return status is the last non-zero exit code (-o pipefail).
        positional : Sequence[str] | None, optional
            Replace the positional parameters ($1, $2, ...) with the provided sequence.
        **options : bool
            Additional named options passed via -o / +o (e.g. `history=False`).

        Returns
        -------
        int
            The exit status.
        """
        cmd_args: list[str] = list(args)

        known_opts: dict[str, bool | None] = {
            "errexit": errexit,
            "xtrace": xtrace,
            "nounset": nounset,
            "noglob": noglob,
            "verbose": verbose,
            "noexec": noexec,
            "allexport": allexport,
            "noclobber": noclobber,
            "monitor": monitor,
            "pipefail": pipefail,
        }
        for name, val in known_opts.items():
            if val is not None:
                cmd_args.append("-o" if val else "+o")
                cmd_args.append(name)

        for name, val in options.items():
            cmd_args.append("-o" if val else "+o")
            cmd_args.append(name)

        if positional is not None:
            cmd_args.append("--")
            cmd_args.extend(positional)

        c_args, c_argv = _build_argv("set", *cmd_args)
        return int(lib.builtin_set(len(c_args), c_argv, self._session))

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
            c_keys = [ffi.new("char[]", k.encode()) for k in env_vars]
            c_vals = [ffi.new("char[]", v.encode()) for v in env_vars.values()]
            vars_array = ffi.new(
                "HookEnvVar[]",
                [{"key": k, "value": v} for k, v in zip(c_keys, c_vals)],
            )
            lib.run_cwd_hook_with_vars(
                self._session,
                hook_name.encode(),
                vars_array,
                len(env_vars),
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
            elif info.type == lib.COMMAND_FUNCTION:
                results.append(FunctionCommandInfo(command=cmd))
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

    def pushd(self, path: str | os.PathLike[str]) -> bool:
        """
        Push a directory onto the directory stack and change to it.

        Parameters
        ----------
        path : str | os.PathLike[str]
            The directory path to push.

        Returns
        -------
        bool
            True if successful, False otherwise.
        """
        return self.dirstack.push(path)

    def popd(self) -> str | None:
        """
        Pop the top directory from the directory stack and change to it.

        Returns
        -------
        str | None
            The directory popped from the stack, or None if the stack is empty.
        """
        return self.dirstack.pop()

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
    def cwd(self, path: str | os.PathLike[str]) -> None:
        """Set the current working directory."""
        self.cd(path)

    @property
    def previous_cwd(self) -> str:
        """The previous working directory."""
        return ffi.string(self._session.previous_working_dir).decode()

    @contextlib.contextmanager
    def env(self, **kwargs: str) -> typing.Iterator[dict[str, str]]:
        """
        Temporarily set environment variables within a context.

        Parameters
        ----------
        **kwargs : str
            Key-value pairs to set in the environment.

        Yields
        ------
        dict[str, str]
            Dictionary of the temporarily set environment variables.
        """
        saved: dict[str, str | None] = {}
        for k, v in kwargs.items():
            saved[k] = self.environ.get(k)
            self.environ[k] = str(v)
        try:
            yield kwargs
        finally:
            for k, old_val in saved.items():
                if old_val is None:
                    self.environ.pop(k, None)
                else:
                    self.environ[k] = old_val

    @contextlib.contextmanager
    def temp_cwd(self, path: str | os.PathLike[str]) -> typing.Iterator[str]:
        """
        Temporarily change the working directory within a context.

        Parameters
        ----------
        path : str | os.PathLike[str]
            Target directory to change to.

        Yields
        ------
        str
            The new working directory path.
        """
        old_dir = self.cwd
        self.cd(path)
        try:
            yield self.cwd
        finally:
            self.cd(old_dir)

    @contextlib.contextmanager
    def scope(
        self,
        *,
        cwd: str | os.PathLike[str] | None = None,
        env: dict[str, str] | None = None,
        **kwargs: str,
    ) -> typing.Iterator[Session]:
        """
        Temporarily change working directory and/or environment variables within a scope.

        Parameters
        ----------
        cwd : str | os.PathLike[str] | None
            Optional directory to switch to.
        env : dict[str, str] | None
            Optional environment variables dictionary.
        **kwargs : str
            Additional environment variables.

        Yields
        ------
        Session
            The current session within the scope.
        """
        env_vars = {**(env or {}), **kwargs}
        old_dir = self.cwd if cwd is not None else None
        saved_env: dict[str, str | None] = {}
        for k, v in env_vars.items():
            saved_env[k] = self.environ.get(k)
            self.environ[k] = str(v)
        if cwd is not None:
            self.cd(cwd)
        try:
            yield self
        finally:
            if old_dir is not None:
                self.cd(old_dir)
            for k, old_val in saved_env.items():
                if old_val is None:
                    self.environ.pop(k, None)
                else:
                    self.environ[k] = old_val

    @contextlib.contextmanager
    def session(self) -> typing.Iterator[Session]:
        """
        Snapshot the current session state and revert all changes upon exiting.

        Any modifications made within this context (working directory, environment
        variables, aliases, functions, shell options, umask, etc.) are reverted
        when the block exits.

        Yields
        ------
        Session
            The current session within the scoped snapshot.
        """
        snapshot = lib.session_snapshot_create(self._session)
        if snapshot == ffi.NULL:
            raise SessionError("Failed to create session snapshot")
        try:
            yield self
        finally:
            lib.session_snapshot_restore(self._session, snapshot)
            lib.session_snapshot_free(snapshot)

    @contextlib.contextmanager
    def sandbox(
        self,
        *,
        readonly: bool = False,
        ro: list[str | os.PathLike[str]] | str | os.PathLike[str] | None = None,
        rw: list[str | os.PathLike[str]] | str | os.PathLike[str] | None = None,
        deny: list[str | os.PathLike[str]] | str | os.PathLike[str] | None = None,
        chroot: str | os.PathLike[str] | None = None,
        default: str = "allow",
        airgap: bool = False,
        offline: bool = False,
        domains: list[str] | str | None = None,
        allow_ports: list[int] | int | None = None,
        deny_ports: list[int] | int | None = None,
        loopback: bool | None = None,
    ) -> typing.Iterator[Session]:
        """
        Unified filesystem & network sandboxing context manager.

        Restricts filesystem access and/or network communication within
        the context scope.
        """
        sb = lib.fs_sandbox_create()
        if sb == ffi.NULL:
            raise SessionError("Failed to create filesystem sandbox")
        if readonly or default in ("deny", "ro", "readonly"):
            sb.readonly_all = True
        if chroot:
            lib.fs_sandbox_set_chroot(sb, str(chroot).encode())
        if ro:
            paths = ro if isinstance(ro, (list, tuple)) else [ro]
            for p in paths:
                lib.fs_sandbox_add_paths(sb, str(p).encode(), 0)
        if rw:
            paths = rw if isinstance(rw, (list, tuple)) else [rw]
            for p in paths:
                lib.fs_sandbox_add_paths(sb, str(p).encode(), 1)
        if deny:
            paths = deny if isinstance(deny, (list, tuple)) else [deny]
            for p in paths:
                lib.fs_sandbox_add_paths(sb, str(p).encode(), 2)
        lib.fs_sandbox_push(self._session, sb)

        net_sb = None
        has_net = (
            airgap
            or offline
            or domains
            or allow_ports
            or deny_ports
            or (loopback is not None)
        )
        if has_net:
            net_sb = lib.net_sandbox_create()
            if net_sb == ffi.NULL:
                popped = lib.fs_sandbox_pop(self._session)
                if popped != ffi.NULL:
                    lib.fs_sandbox_free(popped)
                raise SessionError("Failed to create network sandbox")
            net_sb.offline = bool(airgap or offline)
            if loopback is not None:
                net_sb.allow_loopback = loopback
            elif airgap:
                net_sb.allow_loopback = False
            else:
                net_sb.allow_loopback = True

            if domains:
                d_list = domains if isinstance(domains, (list, tuple)) else [domains]
                for d in d_list:
                    lib.net_sandbox_add_domain(net_sb, str(d).encode())
            if allow_ports:
                p_list = (
                    allow_ports
                    if isinstance(allow_ports, (list, tuple))
                    else [allow_ports]
                )
                for p in p_list:
                    lib.net_sandbox_add_port(net_sb, int(p), True)
            if deny_ports:
                p_list = (
                    deny_ports
                    if isinstance(deny_ports, (list, tuple))
                    else [deny_ports]
                )
                for p in p_list:
                    lib.net_sandbox_add_port(net_sb, int(p), False)
            lib.net_sandbox_push(self._session, net_sb)

        try:
            yield self
        finally:
            if net_sb is not None:
                popped_net = lib.net_sandbox_pop(self._session)
                if popped_net != ffi.NULL:
                    lib.net_sandbox_free(popped_net)
            popped = lib.fs_sandbox_pop(self._session)
            if popped != ffi.NULL:
                lib.fs_sandbox_free(popped)

    @contextlib.contextmanager
    def airgap(
        self,
        *,
        loopback: bool = False,
    ) -> typing.Iterator[Session]:
        """
        Full air-gapping network sandboxing context manager.

        Disables all external network connectivity within the block.

        Parameters
        ----------
        loopback : bool
            Whether to permit loopback (localhost / 127.0.0.1) access. Default: False.
        """
        sb = lib.net_sandbox_create()
        if sb == ffi.NULL:
            raise SessionError("Failed to create network sandbox")
        sb.offline = True
        sb.allow_loopback = loopback
        lib.net_sandbox_push(self._session, sb)
        try:
            yield self
        finally:
            popped = lib.net_sandbox_pop(self._session)
            if popped != ffi.NULL:
                lib.net_sandbox_free(popped)

    @contextlib.contextmanager
    def network(
        self,
        *,
        offline: bool = False,
        loopback: bool = True,
        domains: list[str] | str | None = None,
        allow_ports: list[int] | int | None = None,
        deny_ports: list[int] | int | None = None,
        ips: list[str] | str | None = None,
    ) -> typing.Iterator[Session]:
        """
        Fine-grained network sandboxing context manager.

        Restricts network communication to specified domains, allowed ports,
        denied ports, or airgap mode.

        Parameters
        ----------
        offline : bool
            Whether to disable external network access.
        loopback : bool
            Whether loopback connections are permitted. Default: True.
        domains : list[str] | str | None
            Allowed domain names (e.g. ['github.com', 'registry.npmjs.org']).
        allow_ports : list[int] | int | None
            Allowed destination ports (e.g. [443, 8080]).
        deny_ports : list[int] | int | None
            Blocked ports (e.g. [22, 25]).
        ips : list[str] | str | None
            Explicitly allowed IP addresses.
        """
        sb = lib.net_sandbox_create()
        if sb == ffi.NULL:
            raise SessionError("Failed to create network sandbox")
        sb.offline = offline
        sb.allow_loopback = loopback
        if domains:
            d_list = domains if isinstance(domains, (list, tuple)) else [domains]
            for d in d_list:
                lib.net_sandbox_add_domain(sb, str(d).encode())
        if allow_ports:
            p_list = (
                allow_ports if isinstance(allow_ports, (list, tuple)) else [allow_ports]
            )
            for p in p_list:
                lib.net_sandbox_add_port(sb, int(p), True)
        if deny_ports:
            p_list = (
                deny_ports if isinstance(deny_ports, (list, tuple)) else [deny_ports]
            )
            for p in p_list:
                lib.net_sandbox_add_port(sb, int(p), False)
        if ips:
            ip_list = ips if isinstance(ips, (list, tuple)) else [ips]
            for ip in ip_list:
                lib.net_sandbox_add_ip(sb, str(ip).encode())
        lib.net_sandbox_push(self._session, sb)
        try:
            yield self
        finally:
            popped = lib.net_sandbox_pop(self._session)
            if popped != ffi.NULL:
                lib.net_sandbox_free(popped)

    @contextlib.contextmanager
    def tempdir(
        self,
        *,
        var: str | None = None,
        cd: bool = True,
        prefix: str = "tidesh_sandbox_",
        cleanup: bool = True,
    ) -> typing.Iterator[str]:
        """
        Ephemeral isolated temporary directory context manager.

        Creates an isolated temporary directory, exports it to environment variables,
        optionally enters it, and cleans it up upon exiting.

        Parameters
        ----------
        var : str | None
            Optional environment variable name to export the path to.
        cd : bool
            Whether to change working directory to the temporary directory (default True).
        prefix : str
            Prefix for the temporary directory name.
        cleanup : bool
            Whether to delete the directory and its contents on exit (default True).

        Yields
        ------
        str
            Path to the temporary directory.
        """
        import tempfile

        base_dir = self.environ.get("TMPDIR")
        tmp_path = tempfile.mkdtemp(prefix=prefix, dir=base_dir)
        old_cwd = self.cwd
        saved_env = {
            "TMPDIR": self.environ.get("TMPDIR"),
            "TEMP": self.environ.get("TEMP"),
            "TMP": self.environ.get("TMP"),
            "SANDBOX_DIR": self.environ.get("SANDBOX_DIR"),
        }
        if var:
            saved_env[var] = self.environ.get(var)

        self.environ["TMPDIR"] = tmp_path
        self.environ["TEMP"] = tmp_path
        self.environ["TMP"] = tmp_path
        self.environ["SANDBOX_DIR"] = tmp_path
        if var:
            self.environ[var] = tmp_path

        sb = lib.fs_sandbox_create()
        if sb != ffi.NULL:
            lib.fs_sandbox_add_rw_path(sb, tmp_path.encode())
            lib.fs_sandbox_push(self._session, sb)

        if cd:
            self.cd(tmp_path)
        try:
            yield tmp_path
        finally:
            if cd:
                self.cd(old_cwd)
            if sb != ffi.NULL:
                popped = lib.fs_sandbox_pop(self._session)
                if popped != ffi.NULL:
                    lib.fs_sandbox_free(popped)
            for k, old_val in saved_env.items():
                if old_val is None:
                    self.environ.pop(k, None)
                else:
                    self.environ[k] = old_val
            if cleanup:
                lib.fs_sandbox_rmtree(tmp_path.encode())

    @contextlib.contextmanager
    def chroot(
        self,
        path: str | os.PathLike[str],
        *,
        readonly: bool = False,
        rw: list[str | os.PathLike[str]] | str | os.PathLike[str] | None = None,
        ro: list[str | os.PathLike[str]] | str | os.PathLike[str] | None = None,
        deny: list[str | os.PathLike[str]] | str | os.PathLike[str] | None = None,
    ) -> typing.Iterator[Session]:
        """
        Chroot directory jail context manager.

        Jails filesystem access within the given root directory. Allows read and write
        access inside the jail by default (unless readonly=True is specified).

        Parameters
        ----------
        path : str | os.PathLike[str]
            Path to the chroot directory.
        readonly : bool
            Whether to make the chroot jail read-only.
        rw : list[str] | str | None
            Specific writable subpaths inside the jail.
        ro : list[str] | str | None
            Specific read-only subpaths inside the jail.
        deny : list[str] | str | None
            Forbidden subpaths inside the jail.

        Yields
        ------
        Session
            The current session within the jailed scope.
        """
        path_str = str(path)
        resolved = os.path.abspath(path_str)
        old_cwd = self.cwd
        self.cd(resolved)
        sb = lib.fs_sandbox_create()
        if sb != ffi.NULL:
            lib.fs_sandbox_set_chroot(sb, resolved.encode())
            if readonly:
                sb.readonly_all = True
            if rw:
                paths = rw if isinstance(rw, (list, tuple)) else [rw]
                for p in paths:
                    lib.fs_sandbox_add_paths(sb, str(p).encode(), 1)
            elif not readonly:
                lib.fs_sandbox_add_rw_path(sb, resolved.encode())
            if ro:
                paths = ro if isinstance(ro, (list, tuple)) else [ro]
                for p in paths:
                    lib.fs_sandbox_add_paths(sb, str(p).encode(), 0)
            if deny:
                paths = deny if isinstance(deny, (list, tuple)) else [deny]
                for p in paths:
                    lib.fs_sandbox_add_paths(sb, str(p).encode(), 2)
            lib.fs_sandbox_push(self._session, sb)
        try:
            yield self
        finally:
            if sb != ffi.NULL:
                popped = lib.fs_sandbox_pop(self._session)
                if popped != ffi.NULL:
                    lib.fs_sandbox_free(popped)
            self.cd(old_cwd)

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
