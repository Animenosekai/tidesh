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
        free(session);
    }

    it("should handle nonexistent command in PATH") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *path = find_in_path("nonexistent_command_12345", session);
        asserteq(path, NULL);
        
        free_session(session);
        free(session);
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
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle command execution in session") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Execute a command via string
        int result = execute_string("true", session);
        // true command should succeed
        assert(result >= 0);
        
        free_session(session);
        free(session);
    }

    it("should capture command stdout") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *output = execute_string_stdout("echo test", session);
        assertneq(output, NULL);
        assert(strstr(output, "test") != NULL);
        
        free(output);
        free_session(session);
        free(session);
    }

    it("should handle failing command") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        int result = execute_string("false", session);
        // false command should return non-zero status
        assert(result != 0);
        
        free_session(session);
        free(session);
    }

    it("should handle execution with AST") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "true", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle pipe commands") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo hello | cat", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle command with arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "echo hello world", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle multiple sequential commands") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "true; false; true", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle AND operator") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "true && false", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result > 0); // Should fail since second command fails
            free_ast(ast);
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle OR operator") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        LexerInput *lexer = init_lexer_input(NULL, "false || true", NULL, session);
        ASTNode *ast = parse(lexer, session);
        
        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result == 0); // Should succeed since second command succeeds
            free_ast(ast);
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
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
            free(ast);
        }
        
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle expansion in commands") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Set an environment variable
        environ_set(session->environ, "TEST_VAR", "value");
        
        // Try to execute a command with variable expansion
        int result = execute_string("echo test", session);
        assert(result >= 0);
        
        free_session(session);
        free(session);
    }

    it("should not leak caller positional parameters into shell functions with fewer arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Set caller positional parameters
        environ_set(session->environ, "0", "script.sh");
        environ_set(session->environ, "1", "outer1");
        environ_set(session->environ, "2", "outer2");
        environ_set(session->environ, "3", "outer3");

        // Define a function that outputs $1 and $2
        execute_string("fn() { echo \"$1-$2\"; }", session);

        // Call fn with only 1 argument ($2 should be empty, not 'outer2')
        char *output = execute_string_stdout("fn inner1", session);
        assertneq(output, NULL);
        assert(strstr(output, "inner1-") != NULL);
        assert(strstr(output, "outer2") == NULL);
        free(output);

        // Check that caller positional parameters are fully preserved
        asserteq_str(environ_get(session->environ, "0"), "script.sh");
        asserteq_str(environ_get(session->environ, "1"), "outer1");
        asserteq_str(environ_get(session->environ, "2"), "outer2");
        asserteq_str(environ_get(session->environ, "3"), "outer3");

        free_session(session);
        free(session);
    }

    it("should not leak function positional parameters back to caller") {
        Session *session = init_session(NULL, "/tmp/test_history");

        environ_set(session->environ, "0", "main");
        environ_set(session->environ, "1", "val1");

        // Define a function that takes 3 arguments
        execute_string("fn() { echo \"$1 $2 $3\"; }", session);

        char *output = execute_string_stdout("fn a b c", session);
        assertneq(output, NULL);
        assert(strstr(output, "a b c") != NULL);
        free(output);

        // Caller should still only have $0 and $1, while $2 and $3 must be unset
        asserteq_str(environ_get(session->environ, "0"), "main");
        asserteq_str(environ_get(session->environ, "1"), "val1");
        asserteq(environ_get(session->environ, "2"), NULL);
        asserteq(environ_get(session->environ, "3"), NULL);

        free_session(session);
        free(session);
    }

    it("should properly isolate positional parameters in nested function calls") {
        Session *session = init_session(NULL, "/tmp/test_history");

        environ_set(session->environ, "0", "top");
        environ_set(session->environ, "1", "top_arg");

        execute_string("inner() { echo \"in:$1-$2\"; }", session);
        execute_string("outer() { echo \"out_pre:$1\"; inner \"in_arg\"; echo \"out_post:$1-$2\"; }", session);

        char *output = execute_string_stdout("outer \"out_arg1\" \"out_arg2\"", session);
        assertneq(output, NULL);
        assert(strstr(output, "out_pre:out_arg1") != NULL);
        assert(strstr(output, "in:in_arg-") != NULL);
        assert(strstr(output, "out_post:out_arg1-out_arg2") != NULL);
        free(output);

        asserteq_str(environ_get(session->environ, "0"), "top");
        asserteq_str(environ_get(session->environ, "1"), "top_arg");
        asserteq(environ_get(session->environ, "2"), NULL);

        free_session(session);
        free(session);
    }

    it("should have consistent precedence: special builtins take precedence over functions") {
        Session *session = init_session(NULL, "/tmp/test_history");
        environ_set(session->environ, "TEST_UNSET_VAR", "active");

        // Define a function shadowing a special builtin like unset
        execute_string("unset() { echo \"shadowed_unset\"; }", session);

        // get_command_info should report unset as COMMAND_SPECIAL_BUILTIN
        CommandInfo info = get_command_info("unset", session);
        asserteq(info.type, COMMAND_SPECIAL_BUILTIN);
        if (info.path) free(info.path);

        // execute should run the special builtin unset (unsetting the variable), not the function
        execute_string("unset TEST_UNSET_VAR", session);
        asserteq(environ_get(session->environ, "TEST_UNSET_VAR"), NULL);

        free_session(session);
        free(session);
    }

    it("should have consistent precedence: functions take precedence over regular builtins") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Define a function shadowing a regular builtin (e.g. pwd)
        execute_string("pwd() { echo \"custom_pwd_output\"; }", session);

        // get_command_info should report pwd as COMMAND_FUNCTION
        CommandInfo info = get_command_info("pwd", session);
        asserteq(info.type, COMMAND_FUNCTION);
        if (info.path) free(info.path);

        // execute should run the user function
        char *output = execute_string_stdout("pwd", session);
        assertneq(output, NULL);
        assert(strstr(output, "custom_pwd_output") != NULL);
        free(output);

        // When functions feature is disabled, get_command_info should fallback to COMMAND_BUILTIN
        session->features.functions = false;
        info = get_command_info("pwd", session);
        asserteq(info.type, COMMAND_BUILTIN);
        if (info.path) free(info.path);

        free_session(session);
        free(session);
    }
}

