# 🌊 tidesh

<img src="./assets/logo-scaled.png" alt="Logo" align="right" height="220px">

***A lightweight but fully-featured shell 🐚***

[![GitHub - License](https://img.shields.io/github/license/Animenosekai/tidesh)](https://github.com/Animenosekai/tidesh/blob/main/LICENSE)
[![GitHub Top Language](https://img.shields.io/github/languages/top/Animenosekai/tidesh)](https://github.com/Animenosekai/tidesh)
[![Tests](https://github.com/Animenosekai/tidesh/actions/workflows/test.yml/badge.svg)](https://github.com/Animenosekai/tidesh/actions/workflows/test.yml)
[![Builds](https://github.com/Animenosekai/tidesh/actions/workflows/build.yml/badge.svg)](https://github.com/Animenosekai/tidesh/actions/workflows/build.yml)
![Code Size](https://img.shields.io/github/languages/code-size/Animenosekai/tidesh)
![Repo Size](https://img.shields.io/github/repo-size/Animenosekai/tidesh)
![Issues](https://img.shields.io/github/issues/Animenosekai/tidesh)

## Index

- [Index](#index)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
  - [Basic Usage](#basic-usage)
  - [Command-line Options](#command-line-options)
  - [Configuration](#configuration)
  - [Development](#development)
  - [Builtin Commands](#builtin-commands)
  - [Terminal Handling](#terminal-handling)
- [Testing](#testing)
  - [Prerequisites](#prerequisites-1)
  - [Running Tests](#running-tests)
  - [Specialized Test Targets](#specialized-test-targets)
- [Python Bindings](#python-bindings)
  - [Installation](#installation-1)
  - [Usage](#usage)
- [Features](#features)
  - [Core Shell Features](#core-shell-features)
    - [Environment Variables](#environment-variables)
    - [Command History](#command-history)
    - [Aliases](#aliases)
    - [Directory Stack](#directory-stack)
    - [Hooks](#hooks)
      - [Example](#example)
      - [Supported Hook Types](#supported-hook-types)
      - [Using Hooks](#using-hooks)
      - [Hook Context Variables](#hook-context-variables)
        - [Example: Timing Command Execution](#example-timing-command-execution)
      - [The `hooks` Builtin](#the-hooks-builtin)
    - [Feature Flags](#feature-flags)
      - [Runtime Feature Flags](#runtime-feature-flags)
      - [The `features` Builtin](#the-features-builtin)
      - [Compile-Time Feature Flags](#compile-time-feature-flags)
  - [Expansion Features](#expansion-features)
    - [Variable Expansion](#variable-expansion)
    - [Tilde Expansion](#tilde-expansion)
    - [Brace Expansion](#brace-expansion)
    - [Filename Expansion (Globbing)](#filename-expansion-globbing)
    - [Command Substitution](#command-substitution)
  - [Control Flow](#control-flow)
    - [Conditional Statements](#conditional-statements)
      - [Basic Syntax](#basic-syntax)
      - [If-Else](#if-else)
      - [If-Elif-Else](#if-elif-else)
      - [Using `test` or `[` for Conditions](#using-test-or--for-conditions)
      - [Combining with Logical Operators](#combining-with-logical-operators)
      - [Multi-line Formatting](#multi-line-formatting)
      - [Nested Conditionals](#nested-conditionals)
      - [Notes](#notes)
  - [Lexical Analysis](#lexical-analysis)
    - [Token Types](#token-types)
    - [String Processing](#string-processing)
- [Deployment](#deployment)
- [Contributing](#contributing)
- [Authors](#authors)
- [License](#license)

## Getting Started

### Prerequisites

You will need a UNIX-like operating system to run this shell.

### Installation

```bash
# Clone the repository
git clone https://github.com/Animenosekai/tidesh
cd tidesh

# Build the project
make build

# (Optional) Install the shell to your system
sudo make install
```

### Basic Usage

You can run the shell directly using:

```sh
make run
```

Once inside the shell, you can use the `help` command to see available builtins or `info` to see build and runtime information.

The shell supports colored output, which can be toggled using:

```sh
terminal color enable
terminal color disable
```

or by using arguments when running tidesh.

### Command-line Options

`tidesh` supports several command-line options to customize its behavior:

| Option | Description |
| :--- | :--- |
| `--help` | Show the help message and exit |
| `--eval, -c <cmd>` | Execute the given command and exit |
| `--keep-alive` | Stay interactive after executing a script or eval command |
| `--cd <dir>` | Change to the specified directory on startup |
| `--rc <file>` | Use a custom RC file (default: `~/.tideshrc`) |
| `--history <file>` | Use a custom history file (default: `~/.tidesh-history`) |
| `--enable-colors` | Force enable terminal colors |
| `--disable-colors` | Disable terminal colors |
| `--disable-history` | Disable command history |

### Configuration

You can configure `tidesh` by creating a `.tideshrc` file in your home directory. This file is executed every time the shell starts.

You can use it to set environment variables, create aliases, or run any other shell commands.

Example `.tideshrc`:

```sh
# Set my favorite editor
export EDITOR=vim

# Useful aliases
alias ll='ls -alF'
alias la='ls -A'
alias l='ls -CF'

# Welcome message
info
```

### Development

The `Makefile` provides several utility commands:

- `make build` - Build the project (default `BUILD_TYPE=debug`).
- `make build BUILD_TYPE=release` - Build the optimized version.
- `make test` - Run all automated tests.
- `make format` - Format the code using `clang-format`.
- `make lint` - Run `clang-tidy` for linting.
- `make docs` - Generate documentation using Doxygen.
- `make clean` - Remove object files and binary.

### Builtin Commands

`tidesh` includes a set of powerful builtin commands:

| Command | Description |
| :--- | :--- |
| `alias` | List or set command aliases |
| `cd` | Change the current directory |
| `clear` | Clear the terminal screen |
| `eval` | Execute arguments as a command |
| `exit` | Exit the shell |
| `export` | Set environment variables |
| `features` | Show or manage feature flags |
| `help` | Show the help message |
| `history` | Show or manage command history |
| `hooks` | Show or manage hooks |
| `info` | Show shell and build information |
| `jobs` | List background jobs |
| `fg` | Bring a job to the foreground |
| `bg` | Continue a stopped job in background |
| `popd` | Pop directory from stack and change to it |
| `printenv` | Print environment variables |
| `pushd` | Push directory onto stack or swap stack entries |
| `pwd` | Print the current working directory |
| `source` | Execute commands from a file |
| `type` | Show the type of a command |
| `test` | Evaluate conditional expressions |
| `terminal` | Show or manage terminal settings |
| `unalias` | Remove command aliases |
| `which` | Locate a command in PATH |

### Terminal Handling

The shell includes terminal handling features such as:

- Line editing with support for arrow keys, home/end keys, and delete/backspace
- Command history navigation

It manages the terminal state directly by keeping a virtual representation of the terminal screen and cursor position. Allowing for total control over the terminal display and user input.

## Testing

`tidesh` uses the [Snow](https://github.com/mortie/snow) testing framework.

### Prerequisites

You need to initialize and update the submodules to include the Snow testing framework:

```bash
git submodule update --init --recursive
```

### Running Tests

You can run all the tests using the following command:

```bash
make test
```

### Specialized Test Targets

You can also run tests for specific modules to speed up development:

- `make test/data`: Data structures (Array, Dynamic String, Trie, UTF-8)
- `make test/parsing`: Lexer and AST
- `make test/execution`: Command execution logic
- `make test/builtins`: Shell builtins
- `make test/core`: Core shell components (Environment, History, etc.)
- `make test/integration`: Full integration tests

Individual suite targets like `make test/lexer`, `make test/ast`, or `make test/utf8` are also available.

## Python Bindings

`tidesh` provides Python bindings to interact with the shell programmatically.

### Installation

```bash
make clean/python build/python
```

This will create a Python wheel file in the `dist/` directory. You can install it using pip:

```bash
pip install bindings/python/dist/*.whl
```

### Usage

```python
from tidesh import Session, ExternalCommandInfo

with Session() as session:
    # Execute a command
    session.execute("echo Hello from Python!")

    # Set environment variables
    session.environ["GREETING"] = "Bonjour"
    session.execute("echo $GREETING")

    # Capture output
    output = session.capture("pwd")
    print(f"Current directory: {output}")
    session.execute("cd /tmp")
    output = session.capture("pwd")
    print(f"Changed directory: {output}")

    # Check a command's path
    ls, echo, *_ = session.which("ls", "echo")

    if isinstance(ls, ExternalCommandInfo):
        print(f"Path to ls: {ls.path}")

    if isinstance(echo, ExternalCommandInfo):
        print(f"Path to echo: {echo.path}")
```

## Features

### Core Shell Features

#### Environment Variables

It manages environment variables similar to traditional UNIX shells with automatic handling of shell-specific variables :

- `SHELL` - Path to the shell executable
- `SHLVL` - Shell level indicating depth of nested shells
- `HOME` - User's home directory
- `PWD` - Current working directory
- `OLDPWD` - Previous working directory

#### Command History

It provides a robust command history feature with the persistent storage of commands on disk and various navigation and expansion capabilities.

The history file can be opened using your favorite CSV editor and each entry is stored with the following format:

```csv
timestamp,command
```

Where `timestamp` is the time the command was executed (in seconds since the epoch) and `command` is the actual command string. The newlines in commands are escaped for proper storage.

#### Aliases

An efficient alias management system allowing users to create, retrieve, and delete command shortcuts for command names.

#### Directory Stack

Keeps a stack of directories to facilitate easy navigation between multiple locations in the filesystem:

- `pushd <path>` - Push directory onto stack and change to it
- `popd` - Pop directory from stack and change to it
- `pushd +N` - Swap current directory with Nth directory in stack

#### Hooks

`tidesh` provides a comprehensive hook system that allows you to run custom scripts at specific points in the shell's lifecycle.

Hooks are stored in a `.tidesh-hooks/` directory in the **current working directory** and are automatically executed when certain events occur.

> [!NOTE]  
> Hooks are somtimes called "tides" in the codebase, but they are the same concept.

##### Example

For example, this could be used to configure a project-specific environment when you `cd` into a directory:

```bash
# .tidesh-hooks/enter.sh
# This hook runs when you enter a directory
if [ -f .env ]; then
    echo "Loading environment variables from .env"
    export $(grep -v '^#' .env | xargs)
fi
```

Kind of like an RC file, but for specific directories instead of the whole shell session. You can have different hooks for different events, such as before executing a command, after changing directories, or when a command fails.

> [!TIP]  
> The `enter` hooks of the filepath parents are executed when you `cd` into a directory. For example, if you have `.tidesh-hooks/enter.sh` in `/home/user/project` and you `cd /home/user/project/subdir`, the `enter` hook in `/home/user/project` will be executed.  
> This allows you to have project-specific hooks that run whenever you enter the project directory or any of its subdirectories.
>
> ```fs
> /
> ├── home
> │   ├── user
> │   │   ├── project
> │   │   │   ├── .tidesh-hooks
> │   │   │   │   ├── enter.sh <━━━━━━━━━┓
> │   │   │   ├── subdir                 ┃ Both will be executed
> |   |   |   |   ├── .tidesh-hooks      ┃ when you `cd` into `subdir`
> |   |   |   |   |   ├── enter.sh <━━━━━┛
> ```

##### Supported Hook Types

The shell supports 29 specific hook types plus 1 wildcard hook:

**Session Lifecycle**:

- `start` - Executed once per session after RC file handling
- `end` - Executed once per session right before exit
- `before_rc` - Executed right before reading the RC file

**Directory Navigation**:

- `cd` - Fired when the working directory changes (any change)
- `enter` - Fired when entering a directory from its parent or ancestor
- `exit` - Fired when moving up to a parent or ancestor directory
- `enter_child` - Fired when moving down into a child directory
- `exit_child` - Fired when moving up from a child into its parent

**Command Execution**:

- `before_cmd` - Fired before executing a command string (one per input line)
- `after_cmd` - Fired after executing a command string (one per input line)
- `before_exec` - Fired right before executing an external command (not for builtins)
- `after_exec` - Fired right after an external command finishes (not for builtins)
- `cmd_not_found` - Fired when a command is not found in PATH

**Subshells**:

- `enter_subshell` - Fired when entering a subshell
- `exit_subshell` - Fired after leaving a subshell

**Prompt Lifecycle**:

- `before_prompt` - Fired right before displaying the prompt
- `after_prompt` - Fired right after the prompt is displayed

**Environment Variables**:

- `add_environ` - Fired when an environment variable is added
- `remove_environ` - Fired when an environment variable is removed
- `change_environ` - Fired when an environment variable changes value

**Aliases**:

- `add_alias` - Fired after adding an alias
- `remove_alias` - Fired after removing an alias
- `change_alias` - Fired after updating an alias

**Background Jobs**:

- `before_job` - Fired right before starting a background job
- `after_job` - Fired after a background job finishes or is killed

**Error Handling**:

- `error` - Fired after a command completes with a non-zero exit status
- `syntax_error` - Fired when the command line fails to parse due to syntax error
- `signal` - Fired when a foreground command is terminated by a signal

**Special**:

- `*` - Wildcard hook called before any specific hook fires

##### Using Hooks

To create a hook, create an executable file in `.tidesh-hooks/` (in your current directory) with the hook name:

```bash
# .tidesh-hooks/cd.sh
# Example: Create a hook to show directory contents after cd
echo "Changed to: $PWD"
ls -lh
```

Hooks can be written in **any language** as long as they:

1. Have a proper shebang (`#!/path/to/interpreter`) at the first line

Examples:

```bash
# Bash hook
#!/bin/bash
echo "Hook fired: $TIDE_HOOK at $TIDE_TIMESTAMP"

# Python hook
#!/usr/bin/env python3
import os
hook_name = os.getenv('TIDE_HOOK')
timestamp = os.getenv('TIDE_TIMESTAMP')
print(f"Hook {hook_name} at {timestamp}")

# Ruby hook
#!/usr/bin/env ruby
puts "Hook: #{ENV['TIDE_HOOK']} at #{ENV['TIDE_TIMESTAMP']}"
```

If a hook does **not** have a shebang, it will be sourced as a shell script instead.

##### Hook Context Variables

All hooks receive these global environment variables:

- `TIDE_HOOK` - The specific hook name being executed (e.g., "cd", "before_cmd")
- `TIDE_TIMESTAMP` - Unix timestamp when the hook fires (epoch seconds)
- `TIDE_TIMESTAMP_NANO` - Nanosecond-precision timestamp when the hook fires (nanoseconds since epoch)

Additional context variables are provided for specific hooks:

| Context Variable | Hooks | Details |
| :--- | :--- | :--- |
| `TIDE_HOOK` | All hooks | The specific hook name being executed (e.g., "cd", "enter", "add_alias") |
| `TIDE_TIMESTAMP` | All hooks | Unix timestamp when the hook fires (seconds since epoch) |
| `TIDE_TIMESTAMP_NANO` | All hooks | Nanosecond-precision timestamp (nanoseconds since epoch) |
| `TIDE_CMDLINE` | `before_cmd`, `after_cmd`, `syntax_error`, `error` | Full command line string that was entered |
| `TIDE_CMD` | `before_cmd`, `after_cmd`, `cmd_not_found`, `syntax_error`, `error` | First word of the command (command name) |
| `TIDE_EXEC` | `before_exec`, `after_exec` | Resolved absolute path to the external command being executed |
| `TIDE_ARGV0` | `before_exec`, `after_exec` | The first argument (argv[0]) passed to the command |
| `TIDE_FROM` | `cd`, `enter`, `exit` | Previous directory path before the change |
| `TIDE_TO` | `cd`, `enter`, `exit` | New directory path after the change |
| `TIDE_DIR` | `cd` | Current directory path (same as `TIDE_TO`) |
| `TIDE_PARENT` | `cd` | Parent directory of the target directory |
| `TIDE_CHILD` | `enter_child`, `exit_child` | Child directory path being entered or exited |
| `TIDE_ENV_KEY` | `add_environ`, `remove_environ`, `change_environ` | Environment variable name |
| `TIDE_ENV_VALUE` | `add_environ`, `remove_environ`, `change_environ` | Current/new value (empty for `remove_environ`) |
| `TIDE_ENV_OLD_VALUE` | `add_environ`, `remove_environ`, `change_environ` | Previous value (empty for new variables in `add_environ`) |
| `TIDE_ALIAS_NAME` | `add_alias`, `remove_alias`, `change_alias` | Alias name |
| `TIDE_ALIAS_VALUE` | `add_alias`, `remove_alias`, `change_alias` | Alias value (new value for add/change, previous value for remove) |
| `TIDE_JOB_ID` | `before_job`, `after_job` | Background job ID number |
| `TIDE_JOB_PID` | `before_job`, `after_job` | Process ID of the background job |
| `TIDE_JOB_STATE` | `before_job`, `after_job` | Job state: `running`, `stopped`, `done`, or `killed` |
| `TIDE_SIGNAL` | `signal` | Signal number that terminated the foreground command |
| `TIDE_ERROR` | `syntax_error`, `error` | Error type: `"SYNTAX_ERROR"` or `"CMD_FAIL"` |
| `CMD_FAIL` | `error` | Set to `"1"` when a command fails (for legacy compatibility) |

###### Example: Timing Command Execution

You can use the `before_cmd` and `after_cmd` hooks to measure how long a command takes to execute:

```sh
# .tidesh-hooks/before_cmd.sh
export TIDE_START_TIME=$TIDE_TIMESTAMP_NANO
```

```python
# .tidesh-hooks/after_cmd.py
#!/usr/bin/env python3

import os

elapsed = (
    int(os.environ["TIDE_TIMESTAMP_NANO"]) - int(os.environ.get("TIDE_START_TIME", 0))
) / 1e9

print(f"Took {elapsed} seconds to run {os.environ['TIDE_CMDLINE']}")
```

Now, in tidesh, every command you run will show how long it took to execute:

```sh
❱ sleep 1
Took 1.014119 seconds to run sleep 1
❱ echo hello
hello
Took 0.009505 seconds to run echo hello
```

You can even think about creative ways of customizing your shell behavior with hooks, such as:

```sh
# .tidesh-hooks/start.sh
export WEATHER=$(curl -s wttr.in?format="%c%t")
export PS1="${WEATHER} ❱ "
```

Which would show the current weather in your prompt whenever you start a new shell session:

```sh
🌦  +9°C ❱ echo hello
hello
```

##### The `hooks` Builtin

The `hooks` builtin provides comprehensive hook management:

| Command | Description |
| :--- | :--- |
| `hooks` or `hooks list` | List all hook files in current directory's `.tidesh-hooks/` |
| `hooks enable` | Enable hook execution for this session |
| `hooks disable` | Disable hook execution for this session |
| `hooks status` | Show whether hooks are enabled or disabled |
| `hooks run <hook_name>` | Manually execute a specific hook |
| `hooks path` | Display the hooks directory path (`.tidesh-hooks/` in current dir) |
| `hooks types` | List all available hook types |

Example usage:

```sh
# List available hooks in current directory
hooks list

# Show hook status
hooks status

# Disable hooks temporarily
hooks disable

# Run a hook manually
hooks run cd

# Re-enable hooks
hooks enable

# Show all supported hook types
hooks types
```

#### Feature Flags

`tidesh` provides a flexible feature flag system that allows you to enable or disable shell features at **runtime** or **compile-time** for improved performance when certain features are not needed.

##### Runtime Feature Flags

Runtime features can be toggled on/off per session using the `features` builtin. All features are **enabled by default**.

**Expansion Features**:

- `variable_expansion` - Variable expansion (`$VAR`, `${VAR}`)
- `tilde_expansion` - Tilde expansion (`~`, `~user`)
- `brace_expansion` - Brace expansion (`{a,b,c}`, `{1..10}`)
- `filename_expansion` - Globbing (`*`, `?`, `[...]`)
- `alias_expansion` - Alias substitution

**Shell Features**:

- `job_control` - Background jobs, `fg`, `bg`, `jobs` commands
- `history` - Command history
- `directory_stack` - `pushd`, `popd`, `dirs` commands

**Control Flow & Redirection**:

- `pipes` - Pipe operator `|`
- `redirections` - Input/output redirection (`>`, `<`, `>>`, etc.)
- `sequences` - Command sequences (`;`, `&&`, `||`)
- `subshells` - Subshells `( ... )`
- `command_substitution` - Command substitution (`$(...)`, `<(...)`)
- `assignments` - Variable assignments (`VAR=value`)

**Future Features** (not yet implemented):

- `prompt_expansion` - Prompt customization
- `completion` - Tab completion

##### The `features` Builtin

Manage runtime feature flags with the `features` builtin:

| Command | Description |
| :--- | :--- |
| `features` or `features list` | List all features and their status |
| `features status [name]` | Show status for a specific feature or all |
| `features enable <name\|all>` | Enable a feature or all features |
| `features disable <name\|all>` | Disable a feature or all features |
| `features enable expansions` | Enable all expansion features |
| `features disable expansions` | Disable all expansion features |

Example usage:

```sh
# List all features and their status
features

# Disable brace expansion for this session
features disable brace_expansion

# Enable all expansion features
features enable expansions

# Disable all features for minimal mode
features disable all

# Re-enable everything
features enable all

# Check status of a specific feature
features status pipes
```

##### Compile-Time Feature Flags

For maximum performance and minimal binary size, you can disable features at **compile time** using preprocessor flags. Once disabled at compile time, these features cannot be enabled at runtime.

Available compile-time flags:

| Flag | Description |
| :--- | :--- |
| `TIDESH_DISABLE_JOB_CONTROL` | Disable background jobs, `fg`, `bg`, `jobs` |
| `TIDESH_DISABLE_HISTORY` | Disable command history |
| `TIDESH_DISABLE_ALIASES` | Disable `alias`/`unalias` |
| `TIDESH_DISABLE_DIRSTACK` | Disable `pushd`, `popd`, `dirs` |
| `TIDESH_DISABLE_EXPANSIONS` | Disable all expansions |
| `TIDESH_DISABLE_PIPES` | Disable pipe operator `\|` |
| `TIDESH_DISABLE_REDIRECTIONS` | Disable redirections `>`, `<`, `>>` |
| `TIDESH_DISABLE_SEQUENCES` | Disable `;`, `&&`, `\|\|` operators |
| `TIDESH_DISABLE_SUBSHELLS` | Disable subshells `( ... )` |
| `TIDESH_DISABLE_COMMAND_SUBSTITUTION` | Disable `$(...)` |
| `TIDESH_DISABLE_ASSIGNMENTS` | Disable `VAR=value` assignments |
| `TIDESH_DISABLE_CONDITIONALS` | Disable `if/then/elif/else/fi` statements |

Build with compile-time flags:

```sh
# Build without job control and history
make EXTRA_CFLAGS="-DTIDESH_DISABLE_JOB_CONTROL -DTIDESH_DISABLE_HISTORY"

# Build with no history
make build/no-history

# Build minimal shell (no expansions, no job control)
make build/minimal
```

When features are disabled at compile-time, the `features` builtin will show them as "disabled (compile-time)" and they cannot be enabled at runtime.

### Expansion Features

#### Variable Expansion

It supports variable expansions with various modifiers and word splitting:

- `$VAR` or `${VAR}` - Expand variable value
- `${VAR:-default}` - Use default if unset or empty
- `${VAR:=default}` - Set to default if unset or empty
- `${VAR:+alternative}` - Use alternative if set
- `${VAR:?error}` - Error if unset or empty
- `${#VAR}` - Length of variable
- **Word splitting**: `$=VAR` and `${=VAR}` split on whitespace

#### Tilde Expansion

It supports tilde expansion for user home directories:

- `~` - Expands to current user's home directory
- `~user` - Expands to specified user's home directory
- `~+` - Expands to current working directory (`PWD`)
- `~-` - Expands to previous working directory (`OLDPWD`)
- `~N` - Expands to Nth directory in directory stack

#### Brace Expansion

It supports complex brace expansions for generating multiple strings from patterns:

- **Comma-separated lists**: `a{b,c,d}e` → `abe ace ade`
- **Nested braces**: `a{{,c}d,e}` → `ad acd ae`
- **Numeric ranges**: `file_{1..3}.txt` → `file_1.txt file_2.txt file_3.txt`
- **Padded ranges**: `file_{01..03}.txt` → `file_01.txt file_02.txt file_03.txt`
- **Character ranges**: `{a..z}` → `a b c ... z`
- **Reverse ranges**: `{3..1}` → `3 2 1`

#### Filename Expansion (Globbing)

`glob`-style filename expansion is supported using the standard `glob` algorithm :

- `*` - Matches any string of characters
- `?` - Matches any single character
- `[...]` - Matches any one character from the set
- `[!...]` - Matches any one character not in the set

> [!NOTE]  
> This features relies on the POSIX `glob` function, which doesn't support some advanced patterns like `**` for recursive matching.

#### Command Substitution

It supports various command substitution mechanisms:

- **Process substitution**: `$(command)` executes command and substitutes output
- **Input substitution**: `<(command)` for reading command output as a file
- **Output substitution**: `>(command)` for writing to command input

### Control Flow

#### Conditional Statements

`tidesh` supports full if/then/elif/else/fi conditional statements, similar to bash and other POSIX-compatible shells.

##### Basic Syntax

```sh
if command1
then
    command2
fi
```

The `if` statement executes `command1` and checks its exit status. If the exit status is 0 (success), the commands in the `then` block are executed.

##### If-Else

```sh
if test -f myfile.txt
then
    echo "File exists"
else
    echo "File does not exist"
fi
```

##### If-Elif-Else

```sh
if test "$VAR" = "foo"
then
    echo "VAR is foo"
elif test "$VAR" = "bar"
then
    echo "VAR is bar"
else
    echo "VAR is something else"
fi
```

##### Using `test` or `[` for Conditions

The `test` builtin (also available as `[`) evaluates conditional expressions:

```sh
# String comparisons
if [ "$USER" = "root" ];
then
    echo "Running as root"
fi

# Numeric comparisons
if test "$COUNT" -gt 10
then
    echo "Count is greater than 10"
fi

# File tests
if test -d /tmp
then
    echo "/tmp is a directory"
fi

# Using [ ] syntax (same as test)
if [ -f "$FILE" ]
then
    echo "File exists"
fi
```

##### Combining with Logical Operators

Conditionals work seamlessly with `&&` and `||` operators:

```sh
if test -f file1.txt && test -f file2.txt
then
    echo "Both files exist"
fi

if command1 || command2
then
    echo "At least one command succeeded"
fi
```

##### Multi-line Formatting

Conditionals support flexible formatting with newlines or semicolons:

```sh
# Compact format
if test -f file.txt; then echo "exists"; fi

# Multi-line format
if test -f file.txt
then
    echo "File exists"
    cat file.txt
fi

# Multiple commands in blocks
if pwd | grep -q "/tmp"
then
    echo "In temp directory"
    ls -la
    echo "Done listing"
fi
```

##### Nested Conditionals

```sh
if test -d /etc
then
    if test -f /etc/passwd
    then
        echo "Found passwd file"
    fi
fi
```

##### Notes

- Conditions are evaluated based on the **exit status** of commands (0 = success/true, non-zero = failure/false)
- The `then` keyword is required and must be on a new line or after a semicolon
- The `fi` keyword closes the conditional block
- Conditionals can be disabled at compile-time with `TIDESH_DISABLE_CONDITIONALS`
- Any command can be used as a condition, not just `test`
- Multiple commands can be included in each block (then/elif/else)

### Lexical Analysis

#### Token Types

- **Words and assignments**: Regular words and variable assignments (`VAR=value`)
- **Operators**:
  - Pipes: `|`
  - Redirections: `<`, `>`, `>>`, `>&`
  - Logical operators: `&&`, `||`
  - Background: `&`
  - Sequential: `;`
- **Conditional keywords**: `if`, `then`, `elif`, `else`, `fi`
- **Advanced I/O**:
  - Here-documents: `<<`
  - Here-strings: `<<<`
  - I/O redirection with file descriptors: `n<`, `n>`, `n>>`, `n>&`
  - Process substitution: `<(...)`, `>(...)`
- **Grouping**: Parentheses `(` and `)`
- **Comments**: `#` to end of line

#### String Processing

- **Quoting**:
  - Single quotes: Preserve literal strings
  - Double quotes: Allow variable expansion
  - Escape sequences: Backslash escaping
- **Dynamic strings**: Efficient string building with automatic memory management
- **String arrays**: Dynamic arrays for managing multiple strings

## Deployment

This module is currently in development and might contain bugs.

Feel free to use it in production if you feel like it is suitable for your production even if you may encounter issues.

## Contributing

Pull requests are welcome. For major changes, please open a discussion first to discuss what you would like to change.

Please make sure to update the tests as appropriate.

## Authors

- **anise** — *Initial work, Lexer, Prompt, Expansions* — [Animenosekai](https://github.com/Animenosekai)
- **dindedouce** — *Initial work, Parser* — [DindeDouce](https://github.com/DindeDouce)
- **malogr** — *Initial work, Commands* — [malogr](https://github.com/malogr)
- **BESSIERE Lena** — *Initial work, Commands* — [lena.bessiere](https://gibson.telecomnancy.univ-lorraine.fr/lena.bessiere)

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details
