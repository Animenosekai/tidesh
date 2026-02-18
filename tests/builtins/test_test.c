#include <stdlib.h>
#include <string.h>
#include "builtin.h"
#include "session.h"
#include "snow/snow.h"

describe(test) {
    // String tests
    it("should test non-empty string with -n") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-n", "hello"};
        int result = builtin_test(3, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test empty string with -z") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-z", ""};
        int result = builtin_test(3, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test string equality") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "abc", "=", "abc"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test string inequality") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "abc", "!=", "xyz"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test string less than") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "abc", "<", "xyz"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test string greater than") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "xyz", ">", "abc"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    // Numeric tests
    it("should test numeric equality") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "10", "-eq", "10"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test numeric inequality") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "10", "-ne", "20"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test numeric less than") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "5", "-lt", "10"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test numeric less than or equal") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "10", "-le", "10"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test numeric greater than") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "20", "-gt", "10"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should test numeric greater than or equal") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "10", "-ge", "10"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    // File tests
    it("should test file existence") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-e", "Makefile"};
        int result = builtin_test(3, argv, session);
        asserteq(result, 0); // true (Makefile should exist)
        
        free_session(session);
        free(session);
    }

    it("should test regular file") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-f", "Makefile"};
        int result = builtin_test(3, argv, session);
        asserteq(result, 0); // true (Makefile is a regular file)
        
        free_session(session);
        free(session);
    }

    it("should test directory") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-d", "src"};
        int result = builtin_test(3, argv, session);
        asserteq(result, 0); // true (src is a directory)
        
        free_session(session);
        free(session);
    }

    it("should test readable file") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-r", "Makefile"};
        int result = builtin_test(3, argv, session);
        asserteq(result, 0); // true (Makefile should be readable)
        
        free_session(session);
        free(session);
    }

    it("should test non-empty file") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-s", "Makefile"};
        int result = builtin_test(3, argv, session);
        asserteq(result, 0); // true (Makefile should have content)
        
        free_session(session);
        free(session);
    }

    it("should test nonexistent file") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "-e", "nonexistent_file_12345"};
        int result = builtin_test(3, argv, session);
        asserteq(result, 1); // false (file doesn't exist)
        
        free_session(session);
        free(session);
    }

    // Negation
    it("should handle negation operator") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "!", "-f", "nonexistent_file_12345"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true (file doesn't exist, negated)
        
        free_session(session);
        free(session);
    }

    it("should handle negation with string test") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "!", "-z", "hello"};
        int result = builtin_test(4, argv, session);
        asserteq(result, 0); // true (string is not empty, negated)
        
        free_session(session);
        free(session);
    }

    // Bracket command
    it("should support bracket syntax for test") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Get the builtin function for '['
        int (*bracket_func)(int, char **, Session *) = get_builtin("[");
        assertneq(bracket_func, NULL);
        
        char *argv[] = {"[", "-n", "test", "]"};
        int result = bracket_func(4, argv, session);
        asserteq(result, 0); // true
        
        free_session(session);
        free(session);
    }

    it("should require closing bracket") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        int (*bracket_func)(int, char **, Session *) = get_builtin("[");
        
        // Missing closing bracket
        char *argv[] = {"[", "-n", "test"};
        int result = bracket_func(3, argv, session);
        asserteq(result, 2); // error
        
        free_session(session);
        free(session);
    }

    // Edge cases
    it("should handle test with no arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test"};
        int result = builtin_test(1, argv, session);
        asserteq(result, 1); // false (no expression)
        
        free_session(session);
        free(session);
    }

    it("should handle test with single string argument") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", "hello"};
        int result = builtin_test(2, argv, session);
        asserteq(result, 0); // true (non-empty string)
        
        free_session(session);
        free(session);
    }

    it("should handle test with empty string argument") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"test", ""};
        int result = builtin_test(2, argv, session);
        asserteq(result, 1); // false (empty string)
        
        free_session(session);
        free(session);
    }
}
