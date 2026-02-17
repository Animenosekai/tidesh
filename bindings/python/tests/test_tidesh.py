import tidesh


def test_session_init() -> None:
    session = tidesh.Session()
    assert session.execute("true") == 0
    assert session.execute("false") != 0


def test_environ() -> None:
    session = tidesh.Session()
    session.environ["PY_TEST"] = "works"
    assert session.environ["PY_TEST"] == "works"
    # Execute something that uses it
    output = session.capture("echo $PY_TEST")
    assert output is not None
    assert output.strip() == "works"


def test_cwd() -> None:
    session = tidesh.Session()
    import os

    assert session.cwd == os.getcwd()
    session.execute("cd /tmp")
    # Note: tidesh cd might update its own session state but
    # we need to see if it updates session.current_working_dir
    # Usually it does if we use tilesh's builtin cd.
    assert session.cwd in ("/tmp", "/private/tmp")


def test_capture() -> None:
    output = tidesh.capture("echo 'hello world'")
    assert output is not None
    assert output.strip() == "hello world"
