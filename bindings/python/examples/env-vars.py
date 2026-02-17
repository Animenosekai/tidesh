"""Example of using environment variables in a session."""

from tidesh import Session

with Session() as session:
    session.environ["FOO"] = "bar"
    session.execute("echo $FOO")
