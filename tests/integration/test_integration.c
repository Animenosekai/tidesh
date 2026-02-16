#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "environ.h"
#include "execute.h"
#include "expand.h"
#include "history.h"
#include "lexer.h"
#include "prompt.h"
#include "session.h"
#include "data/dynamic.h"
#include "snow/snow.h"

describe(integration) {
    it("should initialize shell session") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session, NULL);
        assertneq_ptr(session->environ, NULL);
        assertneq_ptr(session->history, NULL);
        assertneq_ptr(session->dirstack, NULL);
        assertneq_ptr(session->aliases, NULL);
        free_session(session);
        free(session);
    }

    it("should parse and create simple command AST") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo hello", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq_ptr(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assert(ast->argc >= 2);
        assertneq_ptr(ast->argv, NULL);
        asserteq_str(ast->argv[0], "echo");
        asserteq_str(ast->argv[1], "hello");
        
        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle environment variables in session") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        environ_set(session->environ, "TEST_VAR", "test_value");
        char *value = environ_get(session->environ, "TEST_VAR");
        asserteq_str(value, "test_value");
        
        free_session(session);
        free(session);
    }

    it("should maintain history across operations") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        history_append(session->history, "ls");
        history_append(session->history, "pwd");
        history_append(session->history, "echo hello");
        
        char *last = history_last_command(session->history);
        asserteq_str(last, "echo hello");
        
        free_session(session);
        free(session);
    }

    it("should handle aliases") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        trie_set(session->aliases, "ll", "ls -la");
        char *alias_value = trie_get(session->aliases, "ll");
        asserteq_str(alias_value, "ls -la");
        
        free_session(session);
        free(session);
    }

    it("should track working directory") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session->current_working_dir, NULL);
        
        update_working_dir(session);
        assertneq_ptr(session->current_working_dir, NULL);
        
        free_session(session);
        free(session);
    }

    it("should handle command with redirections") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo test > /tmp/out.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq_ptr(ast, NULL);
        asserteq(ast->type, NODE_COMMAND);
        assertneq_ptr(ast->redirects, NULL);
        asserteq(ast->redirects->type, TOKEN_REDIRECT_OUT);
        assertneq_ptr(ast->redirects->target, NULL);
        
        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle pipe commands") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "cat file | grep pattern", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq_ptr(ast, NULL);
        asserteq(ast->type, NODE_PIPE);
        assert(!ast->background);
        assertneq_ptr(ast->left, NULL);
        assertneq_ptr(ast->right, NULL);
        asserteq(ast->left->type, NODE_COMMAND);
        asserteq(ast->right->type, NODE_COMMAND);
        asserteq_str(ast->left->argv[0], "cat");
        asserteq_str(ast->right->argv[0], "grep");
        
        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle logical operators") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "cmd1 && cmd2 || cmd3", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq_ptr(ast, NULL);
        asserteq(ast->type, NODE_OR);
        assert(!ast->background);
        assertneq_ptr(ast->left, NULL);
        assertneq_ptr(ast->right, NULL);
        asserteq(ast->left->type, NODE_AND);
        asserteq(ast->right->type, NODE_COMMAND);
        asserteq(ast->left->left->type, NODE_COMMAND);
        asserteq(ast->left->right->type, NODE_COMMAND);
        
        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle background processes") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "sleep 10 &", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq_ptr(ast, NULL);
        assert(ast->background);
        asserteq(ast->type, NODE_COMMAND);
        assertneq_ptr(ast->argv, NULL);
        asserteq_str(ast->argv[0], "sleep");
        
        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse multiple commands in sequence") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "pwd; ls; echo done", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        assertneq_ptr(ast, NULL);
        asserteq(ast->type, NODE_SEQUENCE);
        assert(!ast->background);
        assertneq_ptr(ast->left, NULL);
        assertneq_ptr(ast->right, NULL);
        asserteq(ast->left->type, NODE_SEQUENCE);
        asserteq(ast->right->type, NODE_COMMAND);
        
        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle exit flag") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assert(!session->exit_requested);
        session->exit_requested = true;
        assert(session->exit_requested);
        
        free_session(session);
        free(session);
    }

    it("should handle array operations in context") {
        Array *arr = init_array(NULL);
        array_add(arr, "cmd1");
        array_add(arr, "cmd2");
        
        asserteq(arr->count, 2);
        
        free_array(arr);
        free(arr);
    }

    it("should handle dynamic string operations") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "test");
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "test");
        free(str);
        
        free_dynamic(dyn);
        free(dyn);
    }

    it("should create and manage trie for commands") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "echo", "/bin/echo");
        trie_set(trie, "cat", "/bin/cat");
        
        assert(trie_contains(trie, "echo"));
        assert(trie_contains(trie, "cat"));
        
        free_trie(trie);
        free(trie);
    }
}
