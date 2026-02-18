# tidesh-python 🌊🐍

Python bindings for the `tidesh` shell.

## Installation

```bash
uv pip install .
```

## Usage

```python
from tidesh import Session

# Create a session
session = Session()

# Execute a command
session.execute("echo Hello from Python!")

# Set environment variables
session.environ["GREETING"] = "Bonjour"
session.execute("echo $GREETING")

# Capture output
output = session.capture("pwd")
print(f"Current directory: {output}")
```

### Hooks Management

```python
from tidesh import Session
from tidesh.constants import Hook

# Create a session with hooks disabled
session = Session(run_hooks=False)

# Enable hooks dynamically
session.enable_hooks()

# Check hooks status
if session.hooks_enabled:
    print("Hooks are enabled")

# Disable hooks
session.disable_hooks()

# Using property
session.hooks_enabled = True

# Run a hook manually
session.run_hook("before_cmd")

# Run a hook with custom environment variables
session.run_hook("cd", env_vars={
    "TIDE_PARENT": "/home/user",
    "TIDE_CUSTOM": "value"
})

# Use the hooks builtin command
session.hooks("status")  # Show hooks status
session.hooks("types")   # List all hook types
session.hooks("list")    # List available hooks
session.hooks("path")    # Show hooks directory
```

## Features

- Full access to `tidesh` session management.
- Environment variable handling.
- Command execution and output capture.
- **Hooks management and control.**
- **Dynamic hook enable/disable.**
- **Manual hook execution.**
- Fully typed API.
