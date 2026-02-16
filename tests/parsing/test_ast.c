#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "snow/snow.h"

describe(ast) {
    it("should parse simple command") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo hello", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 2);
        assertneq(ast->argv, NULL);
        asserteq_str(ast->argv[0], "echo");
        asserteq_str(ast->argv[1], "hello");
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse command with arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "ls -la /tmp", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 3);
        assertneq(ast->argv, NULL);
        asserteq_str(ast->argv[0], "ls");
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse pipeline") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "cat file | grep pattern", NULL, session);
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
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse sequence with semicolon") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "pwd; ls", NULL, session);
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
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse AND operator") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "test -f file && cat file", NULL, session);
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
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse OR operator") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "command || fallback", NULL, session);
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
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse background command") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "sleep 10 &", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->background);
        assertneq(ast->argv, NULL);
        asserteq_str(ast->argv[0], "sleep");
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse subshell") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "(echo test)", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_SUBSHELL);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse redirections") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo test > file.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->redirects, NULL);
        asserteq(ast->redirects->type, TOKEN_REDIRECT_OUT);
        assertneq(ast->redirects->target, NULL);
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse input redirection") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "cat < input.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->redirects, NULL);
        asserteq(ast->redirects->type, TOKEN_REDIRECT_IN);
        assertneq(ast->redirects->target, NULL);
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse append redirection") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo test >> file.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->redirects, NULL);
        asserteq(ast->redirects->type, TOKEN_REDIRECT_APPEND);
        assertneq(ast->redirects->target, NULL);
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse variable assignment") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "VAR=value echo test", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq(ast->assignments, NULL);
        assert(ast->assignments->count > 0);
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle empty input") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        // Empty input may return NULL or an empty command
        // Both are acceptable
        
        if (ast) free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse complex pipeline") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "cat file | grep pattern | wc -l", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_PIPE);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        assert(ast->right->type == NODE_PIPE || ast->right->type == NODE_COMMAND);
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should parse nested structures") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "cmd1 && (cmd2 | cmd3) || cmd4", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_OR);
        assert(!ast->background);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_AND);
        asserteq(ast->right->type, NODE_COMMAND);
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should free AST recursively") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "cmd1 | cmd2 && cmd3", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_AND);
        assertneq(ast->left, NULL);
        assertneq(ast->right, NULL);
        asserteq(ast->left->type, NODE_PIPE);
        free_ast(ast); // Should not crash
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle quotes in arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo \"hello world\"", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 2);
        asserteq_str(ast->argv[0], "echo");
        asserteq_str(ast->argv[1], "hello world");
        
        free_ast(ast);
        free_lexer_input(lexer);
        free_session(session);
    }
}
