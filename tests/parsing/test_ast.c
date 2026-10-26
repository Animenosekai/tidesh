#include "ast.h"
#include "snow/snow.h"
#include <stdlib.h>
#include <string.h>

describe(ast) {
    it("should parse simple command") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(NULL, "echo hello", NULL, session);
        ASTNode    *ast   = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 2);
        assertneq(ast->argv, NULL);
        asserteq_str(ast->argv[0], "echo");
        asserteq_str(ast->argv[1], "hello");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse command with arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "ls -la /tmp", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 3);
        assertneq(ast->argv, NULL);
        asserteq_str(ast->argv[0], "ls");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse pipeline") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "cat file | grep pattern", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_PIPE);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_COMMAND);
        asserteq(ast->right->type, NODE_COMMAND);
        assertneq(ast->left->argv, NULL);
        assertneq(ast->right->argv, NULL);
        asserteq_str(ast->left->argv[0], "cat");
        asserteq_str(ast->right->argv[0], "grep");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse sequence with semicolon") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(NULL, "pwd; ls", NULL, session);
        ASTNode    *ast   = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_SEQUENCE);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_COMMAND);
        asserteq(ast->right->type, NODE_COMMAND);
        asserteq_str(ast->left->argv[0], "pwd");
        asserteq_str(ast->right->argv[0], "ls");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse sequence with comment between commands") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "pwd\n# comment\nls", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_SEQUENCE);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_COMMAND);
        asserteq(ast->right->type, NODE_COMMAND);
        asserteq_str(ast->left->argv[0], "pwd");
        asserteq_str(ast->right->argv[0], "ls");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse AND operator") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "test -f file && cat file", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_AND);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_COMMAND);
        asserteq(ast->right->type, NODE_COMMAND);
        asserteq_str(ast->left->argv[0], "test");
        asserteq_str(ast->right->argv[0], "cat");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse OR operator") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "command || fallback", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_OR);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_COMMAND);
        asserteq(ast->right->type, NODE_COMMAND);
        asserteq_str(ast->left->argv[0], "command");
        asserteq_str(ast->right->argv[0], "fallback");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse background command") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(NULL, "sleep 10 &", NULL, session);
        ASTNode    *ast   = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->background);
        assertneq(ast->argv, NULL);
        asserteq_str(ast->argv[0], "sleep");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse subshell") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "(echo test)", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_SUBSHELL);
        assert(!ast->background);
        assertneq(ast->left, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse redirections") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "echo test > file.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->redirects, NULL);
        asserteq(ast->redirects->type, TOKEN_REDIRECT_OUT);
        assertneq(ast->redirects->target, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse input redirection") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "cat < input.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->redirects, NULL);
        asserteq(ast->redirects->type, TOKEN_REDIRECT_IN);
        assertneq(ast->redirects->target, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse append redirection") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "echo test >> file.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->redirects, NULL);
        asserteq(ast->redirects->type, TOKEN_REDIRECT_APPEND);
        assertneq(ast->redirects->target, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse variable assignment") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "VAR=value echo test", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->assignments, NULL);
        assert(ast->assignments->count > 0);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle empty input") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(NULL, "", NULL, session);
        ASTNode    *ast   = parse(lexer, session);

        // Empty input may return NULL or an empty command
        // Both are acceptable

        if (ast) {
            free_ast(ast);
            free(ast);
        }
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse complex pipeline") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(
            NULL, "cat file | grep pattern | wc -l", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_PIPE);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        assert(ast->right->type == NODE_PIPE ||
               ast->right->type == NODE_COMMAND);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse nested structures") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(
            NULL, "cmd1 && (cmd2 | cmd3) || cmd4", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_OR);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_AND);
        asserteq(ast->right->type, NODE_COMMAND);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should free AST recursively") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "cmd1 | cmd2 && cmd3", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_AND);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_PIPE);
        free_ast(ast);
        free(ast);

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle quotes in arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "echo \"hello world\"", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 2);
        asserteq_str(ast->argv[0], "echo");
        asserteq_str(ast->argv[1], "hello world");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse command with NULL session") {
        LexerInput *lexer = init_lexer_input(NULL, "echo hello", NULL, NULL);
        ASTNode    *ast   = parse(lexer, NULL);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 2);
        asserteq_str(ast->argv[0], "echo");
        asserteq_str(ast->argv[1], "hello");

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
    }

    it("should parse for loop with in") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *lexer   = init_lexer_input(
            NULL, "for x in a b c; do echo $x; done", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_FOR);
        assert(ast->for_has_in);
        asserteq(ast->argc, 4);
        asserteq_str(ast->argv[0], "x");
        asserteq_str(ast->argv[1], "a");
        asserteq_str(ast->argv[2], "b");
        asserteq_str(ast->argv[3], "c");
        assertneq(ast->left, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse for loop without in") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *lexer   = init_lexer_input(
            NULL, "for item; do echo $item; done", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_FOR);
        assert(!ast->for_has_in);
        asserteq(ast->argc, 1);
        asserteq_str(ast->argv[0], "item");
        assertneq(ast->left, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should return NULL on syntax errors in for loop") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Missing variable name
        LexerInput *l1 =
            init_lexer_input(NULL, "for; do echo 1; done", NULL, session);
        ASTNode *ast1 = parse(l1, session);
        asserteq(ast1, NULL);
        free_lexer_input(l1);
        free(l1);

        // Invalid variable name
        LexerInput *l2 = init_lexer_input(
            NULL, "for 123 in a b; do echo 1; done", NULL, session);
        ASTNode *ast2 = parse(l2, session);
        asserteq(ast2, NULL);
        free_lexer_input(l2);
        free(l2);

        // Missing do
        LexerInput *l3 =
            init_lexer_input(NULL, "for x in a b; echo 1; done", NULL, session);
        ASTNode *ast3 = parse(l3, session);
        asserteq(ast3, NULL);
        free_lexer_input(l3);
        free(l3);

        // Missing done
        LexerInput *l4 =
            init_lexer_input(NULL, "for x in a b; do echo 1", NULL, session);
        ASTNode *ast4 = parse(l4, session);
        if (ast4) {
            free_ast(ast4);
            free(ast4);
        }
        free_lexer_input(l4);
        free(l4);

        free_session(session);
        free(session);
    }

    it("should parse while loop with condition and body") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *lexer   = init_lexer_input(
            NULL, "while test $x -lt 5; do echo $x; done", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_WHILE);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should return NULL on syntax errors in while loop") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Missing condition
        LexerInput *l1 =
            init_lexer_input(NULL, "while; do echo 1; done", NULL, session);
        ASTNode *ast1 = parse(l1, session);
        asserteq(ast1, NULL);
        free_lexer_input(l1);
        free(l1);

        // Missing do
        LexerInput *l2 =
            init_lexer_input(NULL, "while true; echo 1; done", NULL, session);
        ASTNode *ast2 = parse(l2, session);
        asserteq(ast2, NULL);
        free_lexer_input(l2);
        free(l2);

        // Missing done
        LexerInput *l3 =
            init_lexer_input(NULL, "while true; do echo 1", NULL, session);
        ASTNode *ast3 = parse(l3, session);
        if (ast3) {
            free_ast(ast3);
            free(ast3);
        }
        free_lexer_input(l3);
        free(l3);

        free_session(session);
        free(session);
    }

    it("should parse until loop with condition and body") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *lexer   = init_lexer_input(
            NULL, "until test $x -ge 5; do echo $x; done", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_UNTIL);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should return NULL on syntax errors in until loop") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Missing condition
        LexerInput *l1 =
            init_lexer_input(NULL, "until; do echo 1; done", NULL, session);
        ASTNode *ast1 = parse(l1, session);
        asserteq(ast1, NULL);
        free_lexer_input(l1);
        free(l1);

        // Missing do
        LexerInput *l2 =
            init_lexer_input(NULL, "until true; echo 1; done", NULL, session);
        ASTNode *ast2 = parse(l2, session);
        asserteq(ast2, NULL);
        free_lexer_input(l2);
        free(l2);

        // Missing done
        LexerInput *l3 =
            init_lexer_input(NULL, "until true; do echo 1", NULL, session);
        ASTNode *ast3 = parse(l3, session);
        if (ast3) {
            free_ast(ast3);
            free(ast3);
        }
        free_lexer_input(l3);
        free(l3);

        free_session(session);
        free(session);
    }

    it("should parse case statement with patterns and clauses") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *lexer   = init_lexer_input(
            NULL, "case $x in a) echo 1 ;; b|c) echo 2 ;; *) echo def ;; esac",
            NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_CASE);
        asserteq(ast->argc, 1);
        asserteq_str(ast->argv[0], "$x");

        CaseItem *i1 = ast->case_items;
        assertneq(i1, NULL);
        asserteq(i1->pattern_count, 1);
        asserteq_str(i1->patterns[0], "a");
        asserteq(i1->terminator, CASE_TERM_BREAK);
        assertneq(i1->body, NULL);

        CaseItem *i2 = i1->next;
        assertneq(i2, NULL);
        asserteq(i2->pattern_count, 2);
        asserteq_str(i2->patterns[0], "b");
        asserteq_str(i2->patterns[1], "c");
        asserteq(i2->terminator, CASE_TERM_BREAK);
        assertneq(i2->body, NULL);

        CaseItem *i3 = i2->next;
        assertneq(i3, NULL);
        asserteq(i3->pattern_count, 1);
        asserteq_str(i3->patterns[0], "*");
        asserteq(i3->terminator, CASE_TERM_BREAK);
        assertneq(i3->body, NULL);
        asserteq(i3->next, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse case statement with optional leading paren and different "
       "terminators") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *lexer   = init_lexer_input(
            NULL, "case foo in (a) echo 1 ;& (b) echo 2 ;;& (c) echo 3 ;; esac",
            NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_CASE);

        CaseItem *i1 = ast->case_items;
        assertneq(i1, NULL);
        asserteq_str(i1->patterns[0], "a");
        asserteq(i1->terminator, CASE_TERM_FALLTHROUGH);

        CaseItem *i2 = i1->next;
        assertneq(i2, NULL);
        asserteq_str(i2->patterns[0], "b");
        asserteq(i2->terminator, CASE_TERM_CONTINUE);

        CaseItem *i3 = i2->next;
        assertneq(i3, NULL);
        asserteq_str(i3->patterns[0], "c");
        asserteq(i3->terminator, CASE_TERM_BREAK);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should return NULL on syntax errors in case statement") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Missing in
        LexerInput *l1 =
            init_lexer_input(NULL, "case $x a) echo 1 ;; esac", NULL, session);
        ASTNode *ast1 = parse(l1, session);
        asserteq(ast1, NULL);
        free_lexer_input(l1);
        free(l1);

        // Missing esac
        LexerInput *l2 =
            init_lexer_input(NULL, "case $x in a) echo 1 ;;", NULL, session);
        ASTNode *ast2 = parse(l2, session);
        if (ast2) {
            free_ast(ast2);
            free(ast2);
        }
        free_lexer_input(l2);
        free(l2);

        // Missing closing paren after pattern
        LexerInput *l3   = init_lexer_input(NULL, "case $x in a echo 1 ;; esac",
                                            NULL, session);
        ASTNode    *ast3 = parse(l3, session);
        asserteq(ast3, NULL);
        free_lexer_input(l3);
        free(l3);

        free_session(session);
        free(session);
    }

    it("should parse function definition with named parameters") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(
            NULL, "my_func(param1, param2) { echo $param1; }", NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_FUNCTION_DEF);
        asserteq(ast->argc, 3);
        asserteq_str(ast->argv[0], "my_func");
        asserteq_str(ast->argv[1], "param1");
        asserteq_str(ast->argv[2], "param2");
        assertneq(ast->left, NULL);
        asserteq(ast->left->type, NODE_BLOCK);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse function call with named and positional arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *l1 = init_lexer_input(
            NULL,
            "my_func(param2 = \"something\", param1 = \"something else\")",
            NULL, session);
        ASTNode *ast1 = parse(l1, session);

        assertneq(ast1, NULL);
        asserteq(ast1->type, NODE_COMMAND);
        asserteq(ast1->argc, 3);
        asserteq_str(ast1->argv[0], "my_func");
        assertneq(ast1->arg_keys, NULL);
        asserteq_str(ast1->arg_keys[1], "param2");
        asserteq_str(ast1->argv[1], "something");
        asserteq_str(ast1->arg_keys[2], "param1");
        asserteq_str(ast1->argv[2], "something else");

        free_ast(ast1);
        free(ast1);
        free_lexer_input(l1);
        free(l1);

        LexerInput *l2 = init_lexer_input(
            NULL, "my_func(\"something else\", \"something\")", NULL, session);
        ASTNode *ast2 = parse(l2, session);

        assertneq(ast2, NULL);
        asserteq(ast2->type, NODE_COMMAND);
        asserteq(ast2->argc, 3);
        asserteq_str(ast2->argv[0], "my_func");
        asserteq(ast2->arg_keys[1], NULL);
        asserteq_str(ast2->argv[1], "something else");
        asserteq(ast2->arg_keys[2], NULL);
        asserteq_str(ast2->argv[2], "something");

        free_ast(ast2);
        free(ast2);
        free_lexer_input(l2);
        free(l2);

        free_session(session);
        free(session);
    }
}
