"""Example of using tidesh to execute a command."""

from tidesh import Session

with Session() as session:
    session.cwd
    for token in session.tokenize("echo 'hello world'"):
        print(f"{token.type}: {token.value}")
