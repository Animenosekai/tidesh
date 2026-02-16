# 🌊 tidesh

<img src="./assets/logo.png" alt="Logo" align="right" height="220px">

***A lightweight but fully-featured shell 🐚***

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
- [Features](#features)
  - [Core Shell Features](#core-shell-features)
    - [Environment Variables](#environment-variables)
    - [Command History](#command-history)
    - [Aliases](#aliases)
    - [Directory Stack](#directory-stack)
  - [Expansion Features](#expansion-features)
    - [Variable Expansion](#variable-expansion)
    - [Tilde Expansion](#tilde-expansion)
    - [Brace Expansion](#brace-expansion)
    - [Filename Expansion (Globbing)](#filename-expansion-globbing)
    - [Command Substitution](#command-substitution)
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
| `help` | Show the help message |
| `history` | Show or manage command history |
| `info` | Show shell and build information |
| `popd` | Pop directory from stack and change to it |
| `printenv` | Print environment variables |
| `pushd` | Push directory onto stack or swap stack entries |
| `pwd` | Print the current working directory |
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

### Lexical Analysis

#### Token Types

- **Words and assignments**: Regular words and variable assignments (`VAR=value`)
- **Operators**:
  - Pipes: `|`
  - Redirections: `<`, `>`, `>>`, `>&`
  - Logical operators: `&&`, `||`
  - Background: `&`
  - Sequential: `;`
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

- **anise** - *Initial work, Lexer, Prompt, Expansions* - [Animenosekai](https://github.com/Animenosekai)
- **dindedouce** - *Initial work, Parser* - [DindeDouce](https://github.com/DindeDouce)
- **GRILLERE Malo** - *Initial work, Commands* - [malogr](https://github.com/malogr)
- **BESSIERE Lena** - *Initial work, Commands* - [lena.bessiere](https://gibson.telecomnancy.univ-lorraine.fr/lena.bessiere)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
