import json
import os
import tidesh


def test_session_init() -> None:
    session = tidesh.Session()
    assert session.execute("true") == 0
    assert session.execute("false") != 0


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
    assert ast_res.left is not None
    assert ast_res.left.type == tidesh.NodeType.COMMAND
    assert ast_res.left.argv == ["cat", "file"]
    assert ast_res.right is not None
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
    assert block_res.type == tidesh.NodeType.BLOCK
    assert block_res.body is not None

    func_res = tidesh.ast("foo() { echo in_foo; }")
    assert isinstance(func_res, tidesh.ASTResult)
    assert func_res.type == tidesh.NodeType.FUNCTION_DEF
    assert func_res.name == "foo"
    assert func_res.body is not None

    # Empty command AST
    empty_res = tidesh.ast("")
    assert isinstance(empty_res, tidesh.ASTResult)
    assert bool(empty_res) is False
    assert empty_res.root is None
    assert empty_res.json == "null"
    assert empty_res.pretty == "(empty AST)"
