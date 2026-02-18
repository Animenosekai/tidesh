#include <stdlib.h>
#include <string.h>
#include "builtin.h"
#include "session.h"
#include "snow/snow.h"

describe(type) {
    it("should identify builtin commands with type") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"type", "cd"};
        int result = builtin_type(2, argv, session);
        asserteq(result, 0);
        
        free_session(session);
        free(session);
    }

    it("should identify special builtins with type") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"type", "exit"};
        int result = builtin_type(2, argv, session);
        asserteq(result, 0);
        
        free_session(session);
        free(session);
    }

    it("should identify aliases with type") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Add an alias
        trie_set(session->aliases, "ll", "ls -l");
        
        char *argv[] = {"type", "ll"};
        int result = builtin_type(2, argv, session);
        asserteq(result, 0);
        
        free_session(session);
        free(session);
    }

    it("should identify external commands with type") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"type", "ls"};
        int result = builtin_type(2, argv, session);
        asserteq(result, 0);
        
        free_session(session);
        free(session);
    }

    it("should handle nonexistent commands with type") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"type", "nonexistent_cmd_12345"};
        int result = builtin_type(2, argv, session);
        asserteq(result, 1);
        
        free_session(session);
        free(session);
    }

    it("should handle multiple arguments with type") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"type", "cd", "ls", "pwd"};
        int result = builtin_type(4, argv, session);
        asserteq(result, 0);
        
        free_session(session);
        free(session);
    }
}
