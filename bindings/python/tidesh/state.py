"""Shell state and components for tidesh."""

# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false, reportPrivateUsage=false
from __future__ import annotations

import typing
from collections.abc import Iterator, MutableMapping, Sequence

import typing_extensions

from ._tidesh import ffi, lib
from .exceptions import AliasError


class History:
    """
    Manages command history for a shell session.

    This class provides access to command history, allowing you to append
    commands, navigate through history, and persist history to disk.
    """

    def __init__(self, h_ptr: typing.Any) -> None:
        """
        Initialize the history manager.

        Parameters
        ----------
        h_ptr : typing.Any
            Pointer to the C history structure.
        """
        super().__init__()
        self._h = h_ptr
        """(internal) The underlying C history structure."""

    def append(self, command: str) -> None:
        """
        Append a command to the history.

        Parameters
        ----------
        command : str
            The command to add to history.
        """
        lib.history_append(self._h, command.encode())

    def save(self) -> None:
        """
        Save the history to the history file.

        The history file path is specified when creating the session.
        """
        lib.history_save(self._h)

    def clear(self) -> None:
        """Clear all history entries."""
        lib.history_clear(self._h)

    def last_command(self) -> str | None:
        """
        Get the last command from history.

        Returns
        -------
        str | None
            The last command, or None if history is empty.
        """
        res = lib.history_last_command(self._h)
        return ffi.string(res).decode() if res != ffi.NULL else None

    def previous(self) -> str | None:
        """
        Navigate to the previous command in history.

        Returns
        -------
        str | None
            The previous command, or None if at the beginning.
        """
        res = lib.history_get_previous(self._h)
        return ffi.string(res).decode() if res != ffi.NULL else None

    def next(self) -> str | None:
        """
        Navigate to the next command in history.

        Returns
        -------
        str | None
            The next command, or None if at the end.
        """
        res = lib.history_get_next(self._h)
        return ffi.string(res).decode() if res != ffi.NULL else None

    def nth(self, n: int) -> str | None:
        """
        Get the nth command from history.

        Parameters
        ----------
        n : int
            The index of the command to retrieve (0-based).

        Returns
        -------
        str | None
            The nth command, or None if the index is out of bounds.
        """
        res = lib.history_nth_command(self._h, n)
        return ffi.string(res).decode() if res != ffi.NULL else None


class Environ(MutableMapping[str, str]):
    """
    A dictionary-like interface for environment variables.

    This class implements the MutableMapping protocol, allowing you to
    get, set, and delete environment variables using standard dict operations.
    """

    def __init__(self, env_ptr: typing.Any) -> None:
        """
        Initialize the environment manager.

        Parameters
        ----------
        env_ptr : typing.Any
            Pointer to the C environment structure.
        """
        super().__init__()
        self._env = env_ptr
        """(internal) The underlying C environment structure."""

    @typing_extensions.override
    def __getitem__(self, key: str) -> str:
        """Get the value of an environment variable."""
        val_ptr = lib.environ_get(self._env, key.encode())
        if val_ptr == ffi.NULL:
            raise KeyError(key)
        return ffi.string(val_ptr).decode()

    @typing_extensions.override
    def __setitem__(self, key: str, value: str) -> None:
        """Set an environment variable."""
        lib.environ_set(self._env, key.encode(), value.encode())

    @typing_extensions.override
    def __delitem__(self, key: str) -> None:
        """Delete an environment variable."""
        if not lib.environ_remove(self._env, key.encode()):
            raise KeyError(key)

    @typing_extensions.override
    def __iter__(self) -> Iterator[str]:
        """Iterate over environment variable names."""
        arr_ptr = lib.environ_to_array(self._env)
        if arr_ptr == ffi.NULL:
            return
        try:
            for i in range(arr_ptr.count):
                item = ffi.string(arr_ptr.items[i]).decode()
                if "=" in item:
                    yield item.split("=", 1)[0]
        finally:
            lib.free_array(arr_ptr)
            lib.free(arr_ptr)

    @typing_extensions.override
    def __len__(self) -> int:
        """Get the number of environment variables."""
        arr_ptr = lib.environ_to_array(self._env)
        if arr_ptr == ffi.NULL:
            return 0
        try:
            return arr_ptr.count
        finally:
            lib.free_array(arr_ptr)
            lib.free(arr_ptr)

    @typing_extensions.override
    def get(self, key: str, default: str | None = None) -> str | None:  # pyright: ignore[reportIncompatibleMethodOverride]
        """Get the value of an environment variable, or return default if not found."""
        val_ptr = lib.environ_get(self._env, key.encode())
        if val_ptr == ffi.NULL:
            return default
        return ffi.string(val_ptr).decode()

    @typing_extensions.override
    def __contains__(self, key: object) -> bool:
        """Check if an environment variable exists."""
        if not isinstance(key, str):
            return False
        return bool(lib.environ_contains(self._env, key.encode()))

    def contains(self, key: str) -> bool:
        """
        Check if an environment variable exists.

        Parameters
        ----------
        key : str
            The variable name to check.

        Returns
        -------
        bool
            True if the variable exists, False otherwise.
        """
        return key in self

    def to_array(self) -> list[str]:
        """
        Convert the environment to an array of strings.

        Returns
        -------
        list[str]
            A list of "KEY=VALUE" strings representing all environment variables.
        """
        arr_ptr = lib.environ_to_array(self._env)
        if arr_ptr == ffi.NULL:
            return []
        try:
            return [ffi.string(arr_ptr.items[i]).decode() for i in range(arr_ptr.count)]
        finally:
            lib.free_array(arr_ptr)
            lib.free(arr_ptr)


class Aliases(MutableMapping[str, str]):
    """
    A dictionary-like interface for shell aliases.

    This class implements the MutableMapping protocol, allowing you to
    get, set, and delete aliases using standard dict operations.
    """

    def __init__(self, trie_ptr: typing.Any) -> None:
        """
        Initialize the aliases manager.

        Parameters
        ----------
        trie_ptr : typing.Any
            Pointer to the C trie structure.
        """
        super().__init__()
        self._trie = trie_ptr
        """(internal) The underlying C trie structure for aliases."""

    @typing_extensions.override
    def __getitem__(self, key: str) -> str:
        """Get the value of an alias."""
        res = lib.trie_get(self._trie, key.encode())
        if res == ffi.NULL:
            raise KeyError(key)
        return ffi.string(res).decode()

    @typing_extensions.override
    def __setitem__(self, key: str, value: str) -> None:
        """Set an alias."""
        if not lib.trie_set(self._trie, key.encode(), value.encode()):
            msg = "Failed to set alias"
            raise AliasError(msg, alias_name=key)

    @typing_extensions.override
    def __delitem__(self, key: str) -> None:
        """Delete an alias."""
        if not lib.trie_delete_key(self._trie, key.encode()):
            raise KeyError(key)

    @typing_extensions.override
    def __contains__(self, key: object) -> bool:
        """Check if an alias exists."""
        if not isinstance(key, str):
            return False
        return bool(lib.trie_contains(self._trie, key.encode()))

    @typing_extensions.override
    def __iter__(self) -> Iterator[str]:
        """Iterate over alias names."""
        arr_ptr = lib.trie_starting_with(self._trie, b"")
        if arr_ptr == ffi.NULL:
            return
        try:
            for i in range(arr_ptr.count):
                yield ffi.string(arr_ptr.items[i]).decode()
        finally:
            lib.free_array(arr_ptr)
            lib.free(arr_ptr)

    @typing_extensions.override
    def __len__(self) -> int:
        """Get the number of aliases."""
        arr_ptr = lib.trie_starting_with(self._trie, b"")
        if arr_ptr == ffi.NULL:
            return 0
        try:
            return arr_ptr.count
        finally:
            lib.free_array(arr_ptr)
            lib.free(arr_ptr)

    @typing_extensions.override
    def get(self, key: str, default: str | None = None) -> str | None:  # pyright: ignore[reportIncompatibleMethodOverride]
        """Get the value of an alias, or return default if not found."""
        res = lib.trie_get(self._trie, key.encode())
        if res == ffi.NULL:
            return default
        return ffi.string(res).decode()

    def set(self, key: str, value: str) -> bool:
        """
        Set an alias.

        Parameters
        ----------
        key : str
            The alias name.
        value : str
            The alias value.

        Returns
        -------
        bool
            True if successful, False otherwise.
        """
        return bool(lib.trie_set(self._trie, key.encode(), value.encode()))

    def remove(self, key: str) -> bool:
        """
        Remove an alias.

        Parameters
        ----------
        key : str
            The alias name to remove.

        Returns
        -------
        bool
            True if the alias was found and removed, False otherwise.
        """
        return bool(lib.trie_delete_key(self._trie, key.encode()))


class DirectoryStack(Sequence[str]):
    """
    A sequence interface for the directory stack (used by pushd/popd).

    This class implements the Sequence protocol, allowing you to access
    directories in the stack using indexing and iteration.
    """

    def __init__(self, ds_ptr: typing.Any) -> None:
        """
        Initialize the directory stack manager.

        Parameters
        ----------
        ds_ptr : typing.Any
            Pointer to the C directory stack structure.
        """
        super().__init__()
        self._ds = ds_ptr
        """(internal) The underlying C directory stack structure."""

    @typing.overload
    def __getitem__(self, index: int) -> str: ...

    @typing.overload
    def __getitem__(self, index: slice) -> list[str]: ...

    @typing_extensions.override
    def __getitem__(self, index: int | slice) -> str | list[str]:
        """Get a directory from the stack by index or a slice of directories."""
        if isinstance(index, slice):
            start, stop, step = index.indices(len(self))
            return [self[i] for i in range(start, stop, step)]

        if index < 0:
            index = len(self) + index
        if index < 0 or index >= len(self):
            raise IndexError(index)
        p = lib.dirstack_peek(self._ds, index)
        if p == ffi.NULL:
            raise IndexError(index)
        try:
            return ffi.string(p).decode()
        finally:
            lib.free(p)

    @typing_extensions.override
    def __len__(self) -> int:
        return self._ds.stack.count

    def list(self) -> list[str]:
        """
        Get all directories in the stack as a list.

        Returns
        -------
        list[str]
            A list of all directories in the stack.
        """
        return list(self)


class Terminal:
    """
    Provides information about the terminal.

    Attributes
    ----------
    rows : int
        Number of rows in the terminal.
    cols : int
        Number of columns in the terminal.
    supports_colors : bool
        Whether the terminal supports colors.
    """

    def __init__(self, t_ptr: typing.Any) -> None:
        """
        Initialize the terminal manager.

        Parameters
        ----------
        t_ptr : typing.Any
            Pointer to the C terminal structure.
        """
        super().__init__()
        self._t = t_ptr
        """(internal) The underlying C terminal structure."""

    @property
    def rows(self) -> int:
        """
        Get the number of rows in the terminal.

        Returns
        -------
        int
            The number of rows.
        """
        return self._t.rows

    @property
    def cols(self) -> int:
        """
        Get the number of columns in the terminal.

        Returns
        -------
        int
            The number of columns.
        """
        return self._t.cols

    @property
    def supports_colors(self) -> bool:
        """
        Check if the terminal supports colors.

        Returns
        -------
        bool
            True if colors are supported, False otherwise.
        """
        return bool(self._t.supports_colors)

    @supports_colors.setter
    def supports_colors(self, value: bool) -> None:
        """
        Set whether the terminal supports colors.

        Parameters
        ----------
        value : bool
            True to indicate color support, False otherwise.
        """
        self._t.supports_colors = int(value)
