import json
import os
from pathlib import Path

import tidesh


def test_session_init() -> None:
    session = tidesh.Session()
    assert session.execute("true") == 0
    assert session.execute("false") != 0
    # Callable session
    assert session("true") == 0
    assert session("false") != 0


def test_environ() -> None:
    session = tidesh.Session()
    session.environ["PY_TEST"] = "works"
    assert session.environ["PY_TEST"] == "works"
    output = session.capture("echo $PY_TEST")
    assert output is not None
    assert output.strip() == "works"


def test_cwd() -> None:
    session = tidesh.Session()
    assert session.cwd == os.getcwd()
    session.execute("cd /tmp")
    assert session.cwd in ("/tmp", "/private/tmp")


def test_builtin_cd() -> None:
    session = tidesh.Session()
    initial_cwd = session.cwd
    assert session.cd("/tmp") == 0
    assert session.cwd in ("/tmp", "/private/tmp")
    session.cwd = initial_cwd
    assert session.cwd == initial_cwd

    # PathLike support
    assert session.cd(Path("/tmp")) == 0
    assert session.cwd in ("/tmp", "/private/tmp")
    session.cwd = Path(initial_cwd)
    assert session.cwd == initial_cwd


def test_builtin_methods() -> None:
    session = tidesh.Session()
    assert len(session.jobs) == 0
    assert session.hooks_enabled is True
    session.disable_hooks()
    assert session.hooks_enabled is False
    session.enable_hooks()
    assert session.hooks_enabled is True
    session.run_hook("test", {"FOO": "BAR"})


def test_history_sequence() -> None:
    session = tidesh.Session()
    session.history.clear()
    assert len(session.history) == 0
    session.history.append("echo first")
    session.history.append("echo second")
    session.history.append("echo third")
    assert len(session.history) == 3
    assert session.history[0] == "echo first"
    assert session.history[1] == "echo second"
    assert session.history[2] == "echo third"
    assert session.history[-1] == "echo third"
    assert session.history[-2] == "echo second"
    assert session.history[0:2] == ["echo first", "echo second"]
    assert list(session.history) == ["echo first", "echo second", "echo third"]
    assert "echo second" in session.history
    assert "echo nonexistent" not in session.history


def test_dirstack_push_pop() -> None:
    session = tidesh.Session()
    initial_cwd = session.cwd
    initial_len = len(session.dirstack)
    assert session.dirstack.push("/tmp") is True
    assert session.cwd in ("/tmp", "/private/tmp")
    assert len(session.dirstack) == initial_len + 1
    assert session.dirstack.pop() is not None
    assert session.cwd == initial_cwd
    assert len(session.dirstack) == initial_len


def test_options_and_set() -> None:
    session = tidesh.Session()
    assert session.options.errexit is False
    assert session.set(errexit=True, xtrace=True) == 0
    assert session.options.errexit is True
    assert session.options.xtrace is True
    assert session.set(errexit=False, xtrace=False) == 0
    assert session.options.errexit is False
    assert session.options.xtrace is False

    # Positional parameters
    session.set(positional=["hello", "world"])
    assert session.capture("echo $1 $2").strip() == "hello world"


def test_capture() -> None:
    output = tidesh.capture("echo 'hello world'")
    assert output is not None
    assert output.strip() == "hello world"


def test_features() -> None:
    session = tidesh.Session()
    assert session.features.blocks is True
    assert session.features.functions is True
    session.features.blocks = False
    assert session.features.blocks is False
    session.features.blocks = True
    assert session.features.blocks is True


def test_functions_and_which() -> None:
    session = tidesh.Session()
    res = session.execute("greet() { echo 'welcome'; }")
    assert res == 0
    assert "greet" in session.functions
    assert "greet" in list(session.functions)

    infos = session.which("greet")
    assert len(infos) == 1
    assert isinstance(infos[0], tidesh.FunctionCommandInfo)
    assert infos[0].command == "greet"

    # Remove function
    assert session.functions.remove("greet") is True
    assert "greet" not in session.functions


def test_lexer_api() -> None:
    session = tidesh.Session()
    lex_res = session.lex("echo hello")
    assert isinstance(lex_res, tidesh.LexResult)
    assert len(lex_res) >= 3
    assert lex_res[0].type == tidesh.TokenType.WORD
    assert lex_res[0].value == "echo"
    assert lex_res[1].type == tidesh.TokenType.WORD
    assert lex_res[1].value == "hello"

    # Sequence iteration
    tokens_list = list(lex_res)
    assert len(tokens_list) == len(lex_res)

    # .json property
    data = json.loads(lex_res.json)
    assert isinstance(data, list)
    assert data[0]["type"] == "WORD"
    assert data[0]["value"] == "echo"

    # .pretty property
    assert "Token Stream:" in lex_res.pretty
    assert "WORD" in lex_res.pretty

    # Top-level helper
    top_res = tidesh.lex("echo test")
    assert isinstance(top_res, tidesh.LexResult)
    assert len(top_res) >= 3
    assert top_res[0].value == "echo"


def test_ast_api() -> None:
    session = tidesh.Session()
    ast_res = session.ast("cat file | grep pattern > out.txt")
    assert isinstance(ast_res, tidesh.ASTResult)
    assert bool(ast_res) is True
    assert ast_res.root is not None
    assert ast_res.type == tidesh.NodeType.PIPE

    # Node structure
    assert isinstance(ast_res.root, tidesh.PipeNode)
    assert isinstance(ast_res.root, tidesh.BinaryNode)
    assert isinstance(ast_res.root, tidesh.ASTNode)
    assert not hasattr(ast_res.root, "condition")
    assert not hasattr(ast_res.root, "body")
    assert ast_res.left is not None
    assert isinstance(ast_res.left, tidesh.CommandNode)
    assert not hasattr(ast_res.left, "condition")
    assert not hasattr(ast_res.left, "body")
    assert not hasattr(ast_res.left, "left")
    assert ast_res.left.type == tidesh.NodeType.COMMAND
    assert ast_res.left.argv == ["cat", "file"]
    assert ast_res.right is not None
    assert isinstance(ast_res.right, tidesh.CommandNode)
    assert ast_res.right.type == tidesh.NodeType.COMMAND
    assert len(ast_res.right.redirects) == 1
    assert ast_res.right.redirects[0].target == "out.txt"

    # .json property
    data = json.loads(ast_res.json)
    assert data["type"] == "PIPE"
    assert data["left"]["argv"] == ["cat", "file"]

    # .pretty property
    assert "PIPE" in ast_res.pretty
    assert "COMMAND" in ast_res.pretty
    # Unicode box drawing characters (rounded corners) by default
    assert "├──" in ast_res.pretty or "╰──" in ast_res.pretty
    # ASCII box drawing when requested via .ascii or .pretty(ascii=True)
    assert "|--" in ast_res.ascii or "\\--" in ast_res.ascii
    assert ast_res.pretty(ascii=True) == ast_res.ascii
    assert "|--" in ast_res.pretty(ascii=True) or "\\--" in ast_res.pretty(ascii=True)

    # Block and Function AST
    block_res = tidesh.ast("{ echo inside; }")
    assert isinstance(block_res, tidesh.ASTResult)
    assert isinstance(block_res.root, tidesh.BlockNode)
    assert isinstance(block_res.root, tidesh.ASTNode)
    assert not hasattr(block_res.root, "condition")
    assert not hasattr(block_res.root, "argv")
    assert block_res.type == tidesh.NodeType.BLOCK
    assert block_res.body is not None

    func_res = tidesh.ast("foo() { echo in_foo; }")
    assert isinstance(func_res, tidesh.ASTResult)
    assert isinstance(func_res.root, tidesh.FunctionDefNode)
    assert isinstance(func_res.root, tidesh.ASTNode)
    assert not hasattr(func_res.root, "condition")
    assert not hasattr(func_res.root, "argv")
    assert func_res.type == tidesh.NodeType.FUNCTION_DEF
    assert func_res.name == "foo"
    assert func_res.body is not None

    # Subshell AST
    subshell_res = tidesh.ast("( echo subshell )")
    assert isinstance(subshell_res, tidesh.ASTResult)
    assert isinstance(subshell_res.root, tidesh.SubshellNode)
    assert isinstance(subshell_res.root, tidesh.ASTNode)
    assert not hasattr(subshell_res.root, "condition")
    assert not hasattr(subshell_res.root, "argv")
    assert subshell_res.type == tidesh.NodeType.SUBSHELL
    assert subshell_res.body is not None

    # Conditional AST
    if_res = tidesh.ast("if true; then echo yes; else echo no; fi")
    assert isinstance(if_res, tidesh.ASTResult)
    assert isinstance(if_res.root, tidesh.ConditionalNode)
    assert isinstance(if_res.root, tidesh.ASTNode)
    assert not hasattr(if_res.root, "condition")
    assert not hasattr(if_res.root, "body")
    assert not hasattr(if_res.root, "argv")
    assert not hasattr(if_res.root, "left")
    assert if_res.type == tidesh.NodeType.CONDITIONAL
    assert len(if_res.root.branches) == 2
    assert if_res.root.branches[0].condition is not None
    assert isinstance(if_res.root.branches[0].condition, tidesh.CommandNode)
    assert if_res.root.branches[0].body is not None
    assert isinstance(if_res.root.branches[0].body, tidesh.CommandNode)
    assert if_res.root.branches[1].condition is None
    assert if_res.root.branches[1].body is not None
    assert isinstance(if_res.root.branches[1].body, tidesh.CommandNode)

    # Empty command AST
    empty_res = tidesh.ast("")
    assert isinstance(empty_res, tidesh.ASTResult)
    assert bool(empty_res) is False
    assert empty_res.root is None
    assert empty_res.json == "null"
    assert empty_res.pretty == "(empty AST)"


def test_command_substitution_lazy() -> None:
    session = tidesh.Session()

    # AST generation must not evaluate command substitution
    ast_res = tidesh.ast("echo $(nonexistent_cmd_xyz_999)")
    assert ast_res.root is not None
    assert ast_res.root.argv == ["echo", "$(nonexistent_cmd_xyz_999)"]

    # Lexer must preserve $(...) literally
    lex_res = tidesh.lex("echo $(nonexistent_cmd_xyz_999)")
    tokens = [t.value for t in lex_res if t.type == tidesh.TokenType.WORD]
    assert tokens == ["echo", "$(nonexistent_cmd_xyz_999)"]

    # Execution in false branch must not evaluate
    session.execute("FOO=initial; if false; then FOO=$(echo changed); fi")
    assert session.environ.get("FOO") == "initial"

    # Execution in true branch must evaluate
    session.execute("FOO=initial; if true; then FOO=$(echo changed); fi")
    assert session.environ.get("FOO") == "changed"

    # Execution in assignment
    session.execute("BAR=$(echo hello_py)")
    assert session.environ.get("BAR") == "hello_py"


def test_redirection_syntax() -> None:
    session = tidesh.Session()

    # Lexing &> and &>>
    lex_res1 = tidesh.lex("echo test &> file.txt")
    assert any(t.type == tidesh.TokenType.REDIRECT_ALL for t in lex_res1)

    lex_res2 = tidesh.lex("echo test &>> file.txt")
    assert any(t.type == tidesh.TokenType.REDIRECT_APPEND_ERR for t in lex_res2)

    # AST generation for &>
    ast_res = tidesh.ast("echo test &> file.txt")
    assert ast_res.root is not None
    assert len(ast_res.root.redirects) == 1
    assert ast_res.root.redirects[0].target == "file.txt"
    assert ast_res.root.redirects[0].type == tidesh.TokenType.REDIRECT_OUT_ERR

    # Execution with &>
    tmp_file = "/tmp/test_py_redir.txt"
    if os.path.exists(tmp_file):
        os.remove(tmp_file)

    session.execute(f"echo py_redir &> {tmp_file}")
    with open(tmp_file) as f:
        content = f.read()
    assert "py_redir" in content

    # Execution with &>>
    session.execute(f"echo py_append &>> {tmp_file}")
    with open(tmp_file) as f:
        content = f.read()
    assert "py_redir" in content
    assert "py_append" in content

    # Execution with 2>&1
    tmp_dup = "/tmp/test_py_dup.txt"
    if os.path.exists(tmp_dup):
        os.remove(tmp_dup)

    session.execute(f"ls /nonexistent_py_dir > {tmp_dup} 2>&1")
    with open(tmp_dup) as f:
        dup_content = f.read()
    assert "nonexistent_py_dir" in dup_content

    if os.path.exists(tmp_file):
        os.remove(tmp_file)
    if os.path.exists(tmp_dup):
        os.remove(tmp_dup)


def test_for_loop() -> None:
    session = tidesh.Session()

    # Lexer
    lex_res = tidesh.lex("for i in a b; do echo $i; done")
    assert any(t.type == tidesh.TokenType.FOR for t in lex_res)
    assert any(t.type == tidesh.TokenType.IN for t in lex_res)
    assert any(t.type == tidesh.TokenType.DO for t in lex_res)
    assert any(t.type == tidesh.TokenType.DONE for t in lex_res)

    # AST with in
    ast_res = tidesh.ast("for x in 1 2 3; do echo $x; done")
    assert isinstance(ast_res, tidesh.ASTResult)
    assert ast_res.root is not None
    assert isinstance(ast_res.root, tidesh.ForNode)
    assert isinstance(ast_res.root, tidesh.ASTNode)
    assert not hasattr(ast_res.root, "condition")
    assert not hasattr(ast_res.root, "argv")
    assert not hasattr(ast_res.root, "left")
    assert ast_res.type == tidesh.NodeType.FOR
    assert ast_res.root.var_name == "x"
    assert ast_res.root.has_in is True
    assert ast_res.root.words == ["1", "2", "3"]
    assert ast_res.root.body is not None
    assert isinstance(ast_res.root.body, tidesh.CommandNode)

    # AST JSON
    data = json.loads(ast_res.json)
    assert data["type"] == "FOR"
    assert data["variable"] == "x"
    assert data["words"] == ["1", "2", "3"]
    assert "variable: 'x'" in ast_res.pretty
    assert "words: ['1', '2', '3']" in ast_res.pretty
    assert "body: COMMAND" in ast_res.pretty

    # Execution
    out = session.capture('for i in hello world; do echo -n "$i "; done')
    assert out.strip() == "hello world"

    # Execution with break and continue
    out_ctrl = session.capture(
        'for i in 1 2 3 4; do if test $i -eq 2; then continue; fi; if test $i -eq 4; then break; fi; echo -n "$i "; done',
    )
    assert out_ctrl.strip() == "1 3"


def test_while_loop() -> None:
    session = tidesh.Session()

    # Lexer
    lex_res = tidesh.lex("while true; do echo loop; done")
    assert any(t.type == tidesh.TokenType.WHILE for t in lex_res)
    assert any(t.type == tidesh.TokenType.DO for t in lex_res)
    assert any(t.type == tidesh.TokenType.DONE for t in lex_res)

    # AST
    ast_res = tidesh.ast("while test $x != 3; do echo $x; done")
    assert isinstance(ast_res, tidesh.ASTResult)
    assert ast_res.root is not None
    assert isinstance(ast_res.root, tidesh.WhileNode)
    assert isinstance(ast_res.root, tidesh.ASTNode)
    assert not hasattr(ast_res.root, "var_name")
    assert not hasattr(ast_res.root, "words")
    assert not hasattr(ast_res.root, "argv")
    assert not hasattr(ast_res.root, "left")
    assert ast_res.type == tidesh.NodeType.WHILE
    assert ast_res.root.condition is not None
    assert isinstance(ast_res.root.condition, tidesh.CommandNode)
    assert ast_res.root.body is not None
    assert isinstance(ast_res.root.body, tidesh.CommandNode)

    # AST JSON
    data = json.loads(ast_res.json)
    assert data["type"] == "WHILE"
    assert "condition" in data
    assert "body" in data
    assert "condition: COMMAND" in ast_res.pretty
    assert "body: COMMAND" in ast_res.pretty

    # Execution
    session.execute(
        'I=""; ACC=""; while test "$I" != "aaa"; do I="${I}a"; ACC="${ACC}$I,"; done',
    )
    assert session.environ.get("ACC") == "a,aa,aaa,"

    # Execution with break and continue
    session.execute(
        'I=""; ACC=""; while test "$I" != "aaaaa"; do I="${I}a"; if test "$I" = "aa"; then continue; fi; if test "$I" = "aaaa"; then break; fi; ACC="${ACC}$I,"; done',
    )
    assert session.environ.get("ACC") == "a,aaa,"


def test_case() -> None:
    session = tidesh.Session()

    # Lexer
    lex_res = tidesh.lex("case $x in a|b) echo 1 ;; esac")
    assert any(t.type == tidesh.TokenType.CASE for t in lex_res)
    assert any(t.type == tidesh.TokenType.IN for t in lex_res)
    assert any(t.type == tidesh.TokenType.ESAC for t in lex_res)
    assert any(t.type == tidesh.TokenType.DOUBLE_SEMICOLON for t in lex_res)

    # AST
    ast_res = tidesh.ast("case $x in a|b) echo 1 ;; *) echo 2 ;; esac")
    assert isinstance(ast_res, tidesh.ASTResult)
    assert ast_res.root is not None
    assert isinstance(ast_res.root, tidesh.CaseNode)
    assert isinstance(ast_res.root, tidesh.ASTNode)
    assert ast_res.type == tidesh.NodeType.CASE
    assert ast_res.root.word == "$x"
    assert len(ast_res.root.items) == 2

    item0 = ast_res.root.items[0]
    assert isinstance(item0, tidesh.CaseItem)
    assert item0.patterns == ["a", "b"]
    assert item0.terminator == ";;"
    assert item0.body is not None
    assert isinstance(item0.body, tidesh.CommandNode)

    item1 = ast_res.root.items[1]
    assert item1.patterns == ["*"]
    assert item1.terminator == ";;"

    # AST JSON
    data = json.loads(ast_res.json)
    assert data["type"] == "CASE"
    assert data["word"] == "$x"
    assert len(data["items"]) == 2
    assert data["items"][0]["patterns"] == ["a", "b"]
    assert data["items"][0]["terminator"] == ";;"
    assert 'word: "$x"' in ast_res.pretty
    assert 'patterns=["a", "b"]' in ast_res.pretty

    # Execution: match pattern
    session.execute(
        'X="banana"; RES=""; case "$X" in apple|banana) RES="fruit" ;; *) RES="other" ;; esac',
    )
    assert session.environ.get("RES") == "fruit"

    # Execution: default match
    session.execute(
        'X="car"; RES=""; case "$X" in apple|banana) RES="fruit" ;; *) RES="default" ;; esac',
    )
    assert session.environ.get("RES") == "default"

    # Execution: wildcard
    session.execute(
        'X="script.sh"; RES=""; case "$X" in *.sh) RES="shell" ;; *) RES="unknown" ;; esac',
    )
    assert session.environ.get("RES") == "shell"

    # Execution: fallthrough ;&
    session.execute(
        'ACC=""; case "1" in 1) ACC="${ACC}one,";& 2) ACC="${ACC}two,";; esac',
    )
    assert session.environ.get("ACC") == "one,two,"

    # Execution: continue matching ;;&
    session.execute(
        'ACC=""; case "test_case" in t*) ACC="${ACC}starts_t,";;& *_case) ACC="${ACC}ends_case,";;& *other) ACC="${ACC}other,";; esac',
    )
    assert session.environ.get("ACC") == "starts_t,ends_case,"

    # Execution: capture
    out = session.capture('case "hello" in hello) echo "matched" ;; esac')
    assert out.strip() == "matched"


def test_exec_builtin() -> None:
    session = tidesh.Session()
    # exec without arguments should succeed and set permanent_redirections
    assert session.execute("exec") == 0

    # exec redirection inside subshell
    tmp_path = Path("/tmp/test_py_exec_subshell.txt")
    if tmp_path.exists():
        tmp_path.unlink()
    try:
        assert (
            session.execute(f"( exec > {tmp_path}; echo 'py exec subshell test' )") == 0
        )
        assert tmp_path.read_text().strip() == "py exec subshell test"
    finally:
        if tmp_path.exists():
            tmp_path.unlink()

    # exec command failure in subshell
    assert (
        session.execute(
            "( exec /nonexistent/cmd/test/py )",
        )
        == 127
    )


def test_trap_builtin() -> None:
    session = tidesh.Session()

    # trap -l returns 0
    assert session.execute("trap -l") == 0

    # Setting trap on EXIT
    assert session.execute("trap 'echo exiting' EXIT") == 0
    out = session.capture("trap -p EXIT")
    assert out is not None and "exiting" in out

    # Clear EXIT trap
    assert session.execute("trap - EXIT") == 0
    out_cleared = session.capture("trap -p EXIT")
    assert out_cleared is None or out_cleared.strip() == ""

    # Subshell EXIT trap executes independently
    tmp_path = Path("/tmp/test_py_trap_subshell.txt")
    if tmp_path.exists():
        tmp_path.unlink()
    try:
        assert (
            session.execute(
                f"( trap 'echo trap_fired >> {tmp_path}' EXIT; echo inside >> {tmp_path} )",
            )
            == 0
        )
        assert tmp_path.read_text() == "inside\ntrap_fired\n"
    finally:
        if tmp_path.exists():
            tmp_path.unlink()


def test_command_builtin() -> None:
    session = tidesh.Session()

    # command with no arguments
    assert session.execute("command") == 0
    assert session.execute("command -p") == 0

    # command -v
    out = session.capture("command -v cd")
    assert out is not None and out.strip() == "cd"

    out_ls = session.capture("command -v ls")
    assert out_ls is not None and "ls" in out_ls

    # command -V
    out_v = session.capture("command -V cd")
    assert out_v is not None and "built-in" in out_v

    # command bypassing function
    session.execute("cd() { TIDE_PY_CD=1; }")
    orig_cwd = session.cwd
    try:
        assert session.execute("command cd /tmp") == 0
        assert session.environ.get("TIDE_PY_CD") is None
        assert session.cwd in ("/tmp", "/private/tmp")
    finally:
        session.cd(orig_cwd)


def test_terminal_test_builtin() -> None:
    session = tidesh.Session()

    # unopened file descriptor 99 is not a terminal
    assert session.execute("test -t 99") == 1
    assert session.execute("[ -t 99 ]") == 1
    assert session.execute("[ ! -t 99 ]") == 0

    # invalid integer returns error status 2
    assert session.execute("test -t abc") == 2
    assert session.execute("[ -t notanumber ]") == 2


def test_with_statement_ast() -> None:
    session = tidesh.Session()
    ast_res = session.ast('with env(FOO = "bar"), cwd("/tmp") {\n    echo $FOO\n}')
    assert ast_res.type == tidesh.NodeType.WITH
    assert isinstance(ast_res.root, tidesh.WithNode)


def test_session_python_context_managers() -> None:
    session = tidesh.Session()
    orig_cwd = session.cwd

    # test session.env
    with session.env(TEMP_VAR="hello_python"):
        assert session.environ.get("TEMP_VAR") == "hello_python"
    assert "TEMP_VAR" not in session.environ

    # test session.temp_cwd
    with session.temp_cwd("/tmp"):
        assert session.cwd in ("/tmp", "/private/tmp")
    assert session.cwd == orig_cwd

    # test session.scope
    with session.scope(cwd="/tmp", env={"SCOPED_A": "1"}, SCOPED_B="2"):
        assert session.cwd in ("/tmp", "/private/tmp")
        assert session.environ.get("SCOPED_A") == "1"
        assert session.environ.get("SCOPED_B") == "2"
    assert session.cwd == orig_cwd
    assert "SCOPED_A" not in session.environ
    assert "SCOPED_B" not in session.environ

    # test session.session() snapshot context manager
    session.environ["BASE_VAR"] = "base"
    with session.session():
        session.cd("/tmp")
        session.environ["BASE_VAR"] = "changed_in_snapshot"
        session.environ["INNER_NEW"] = "added_in_snapshot"
        session.execute("alias py_test_alias='echo 42'")
        session.execute("py_test_fn() { echo 100; }")
        assert session.cwd in ("/tmp", "/private/tmp")
        assert session.environ.get("BASE_VAR") == "changed_in_snapshot"
        assert session.environ.get("INNER_NEW") == "added_in_snapshot"
        assert session.aliases.get("py_test_alias") == "echo 42"
        assert "py_test_fn" in session.functions

    # After exiting session snapshot context:
    assert session.cwd == orig_cwd
    assert session.environ.get("BASE_VAR") == "base"
    assert "INNER_NEW" not in session.environ
    assert "py_test_alias" not in session.aliases
    assert "py_test_fn" not in session.functions

    # test with session in tidesh shell syntax
    session.execute("with session {\n    cd /tmp\n    export SHELL_TEST_VAR='123'\n}")
    assert session.cwd == orig_cwd
    assert "SHELL_TEST_VAR" not in session.environ


if __name__ == "__main__":
    import inspect
    import sys

    passed = 0
    failed = 0
    current_module = sys.modules[__name__]
    for name, func in inspect.getmembers(current_module, inspect.isfunction):
        if name.startswith("test_"):
            try:
                func()
                print(f"✓ {name}")
                passed += 1
            except Exception as e:
                print(f"✗ {name}: {e}")
                failed += 1
                raise
    print(f"\n{passed} passed, {failed} failed")
