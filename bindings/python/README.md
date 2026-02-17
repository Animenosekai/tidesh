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

## Features

- Full access to `tidesh` session management.
- Environment variable handling.
- Command execution and output capture.
- Fully typed API.
