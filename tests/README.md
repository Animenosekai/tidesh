# Tidesh Test Suite

> [!NOTE]  
> You might need to run `git submodule init` and `git submodule update` to initialize the Snow testing framework before building the tests.

This directory contains comprehensive tests for the tidesh shell project using the Snow testing framework.

## Overview

The test suite aims to achieve maximum code coverage by testing all major components of the tidesh shell including:

- **Data Structures**: Array, Dynamic string, Trie, Lexer
- **Core Components**: Session management, Environment variables, History, Directory stack
- **Parsing**: Abstract Syntax Tree (AST) parsing, Lexer tokenization
- **Integration**: Full shell functionality with multiple components

## Building and Running Tests

### Build all tests

```bash
make test
```

### Build and run tests with Debug mode

```bash
make BUILD_TYPE=debug test
```

### Build and run tests in Release mode

```bash
make BUILD_TYPE=release test
```

### Run specific test module

```bash
./bin/tidesh-test-<version>-<platform>-<build_type> array
./bin/tidesh-test-<version>-<platform>-<build_type> dynamic
./bin/tidesh-test-<version>-<platform>-<build_type> session
```

### Run with test options

```bash
# List all tests
./bin/tidesh-test-1.0-* --list

# Run with verbose output
./bin/tidesh-test-1.0-* --no-quiet

# Run with timing information
./bin/tidesh-test-1.0-* --timer
```

### Run with memory checking (using valgrind)

```bash
valgrind --leak-check=full --show-leak-kinds=all ./bin/tidesh-test-1.0-*
```

## Test Framework

Tests use the **Snow** testing framework:

- Header-only C testing library
- Simple, readable test syntax
- Assertion macros for comparison
- Automatic test discovery and execution
- Memory safety integration (works with valgrind)

See [`tests/snow/README.md`](snow/README.md) for more information.

## Writing New Tests

To add tests for a new module:

1. Create a new file `tests/test_module.c`
2. Include necessary headers and `snow/snow.h`
3. Write test cases using the `describe` and `it` macros:

```c
#include "snow/snow.h"
#include "module.h"

describe(module_name) {
    it("should do something") {
        // Arrange
        Module *m = init_module(NULL);

        // Act
        int result = module_operation(m);

        // Assert
        asserteq(result, expected_value);

        // Cleanup
        free_module(m);
        free(m);
    }
}
```

## Tips for Test Writing

- Use descriptive test names that clearly state what is being tested
- Follow the Arrange-Act-Assert pattern
- Clean up resources using `defer()` or manual cleanup
- Test both happy path and edge cases
- Use appropriate assertions: `asserteq`, `assertneq`, `assert`, etc.
- Check for NULL pointers and error conditions
- Test boundary conditions
- Verify memory is properly managed (use valgrind)

## Known Limitations

- Some tests require file system access and may depend on `/tmp` availability
- History tests may create temporary files
- Tests are compiled with address sanitizer in debug mode to detect memory issues

## Continuous Integration

These tests are designed to work in CI/CD environments:

- No interactive input required
- Deterministic results
- Exit codes properly set (0 for success, non-zero for failure)
- Can be run without display/terminal
