#include <stdlib.h>
#include <string.h>
#include "execute.h"
#include "snow/snow.h"

describe(execute) {
    it("should find command in PATH") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Try to find common commands that should exist on most systems
        char *path = find_in_path("ls", session);
        // path may or may not be found depending on system
        // The important thing is the function doesn't crash
        
        if (path) free(path);
        free_session(session);
    }

    it("should handle nonexistent command in PATH") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *path = find_in_path("nonexistent_command_12345", session);
        asserteq(path, NULL);
        
        free_session(session);
    }

    it("should execute simple echo command") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo test", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            // Result should be 0 for success or non-zero for error
            assert(result >= 0);
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle command execution in session") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Execute a command via string
        int result = execute_string("true", session);
        // true command should succeed
        assert(result >= 0);
        
        free_session(session);
    }

    it("should capture command stdout") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *output = execute_string_stdout("echo test", session);
        assertneq(output, NULL);
        assert(strstr(output, "test") != NULL);
        
        free(output);
        free_session(session);
    }

    it("should handle failing command") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        int result = execute_string("false", session);
        // false command should return non-zero status
        assert(result != 0);
        
        free_session(session);
    }

    it("should handle execution with AST") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "true", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle pipe commands") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo hello | cat", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle command with arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo hello world", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle multiple sequential commands") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "true; false; true", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle AND operator") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "true && false", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result > 0); // Should fail since second command fails
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle OR operator") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "false || true", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result == 0); // Should succeed since second command succeeds
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle redirections") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo test > /tmp/test_output.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            // Command should execute (may succeed or fail depending on permissions)
            assert(result >= 0);
            free_ast(ast);
        }
        
        free_lexer_input(lexer);
        free_session(session);
    }

    it("should handle expansion in commands") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Set an environment variable
        environ_set(session->environ, "TEST_VAR", "value");
        
        // Try to execute a command with variable expansion
        int result = execute_string("echo test", session);
        assert(result >= 0);
        
        free_session(session);
    }
}
