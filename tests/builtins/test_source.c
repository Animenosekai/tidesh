#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"
#include "environ.h"
#include "session.h"
#include "snow/snow.h"

describe(source) {
    it("should source a simple script") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Create a temporary script file
        FILE *f = fopen("/tmp/test_source.sh", "w");
        if (f) {
            fprintf(f, "export TEST_VAR=sourced_value\n");
            fclose(f);
            
            char *argv[] = {"source", "/tmp/test_source.sh"};
            int result = builtin_source(2, argv, session);
            asserteq(result, 0);
            
            // Check if the variable was set
            const char *value = environ_get(session->environ, "TEST_VAR");
            assertneq(value, NULL);
            if (value) {
                asserteq(strcmp(value, "sourced_value"), 0);
            }
            
            unlink("/tmp/test_source.sh");
        }
        
        free_session(session);
        free(session);
    }

    it("should handle dot command for sourcing") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Create a temporary script file
        FILE *f = fopen("/tmp/test_dot.sh", "w");
        if (f) {
            fprintf(f, "export DOT_VAR=dot_value\n");
            fclose(f);
            
            // Get the builtin function for '.'
            int (*dot_func)(int, char **, Session *) = get_builtin(".");
            assertneq(dot_func, NULL);
            
            char *argv[] = {".", "/tmp/test_dot.sh"};
            int result = dot_func(2, argv, session);
            asserteq(result, 0);
            
            // Check if the variable was set
            const char *value = environ_get(session->environ, "DOT_VAR");
            assertneq(value, NULL);
            if (value) {
                asserteq(strcmp(value, "dot_value"), 0);
            }
            
            unlink("/tmp/test_dot.sh");
        }
        
        free_session(session);
        free(session);
    }

    it("should handle missing file in source") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"source", "/tmp/nonexistent_source_file.sh"};
        int result = builtin_source(2, argv, session);
        asserteq(result, 1); // Should return error
        
        free_session(session);
        free(session);
    }

    it("should handle source with no arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        char *argv[] = {"source"};
        int result = builtin_source(1, argv, session);
        asserteq(result, 1); // Should return error
        
        free_session(session);
        free(session);
    }

    it("should execute commands from sourced file") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Create a script with multiple commands
        FILE *f = fopen("/tmp/test_multi.sh", "w");
        if (f) {
            fprintf(f, "export VAR1=value1\n");
            fprintf(f, "export VAR2=value2\n");
            fprintf(f, "export VAR3=value3\n");
            fclose(f);
            
            char *argv[] = {"source", "/tmp/test_multi.sh"};
            int result = builtin_source(2, argv, session);
            asserteq(result, 0);
            
            // Check all variables were set
            const char *v1 = environ_get(session->environ, "VAR1");
            const char *v2 = environ_get(session->environ, "VAR2");
            const char *v3 = environ_get(session->environ, "VAR3");
            
            assertneq(v1, NULL);
            assertneq(v2, NULL);
            assertneq(v3, NULL);
            
            if (v1 && v2 && v3) {
                asserteq(strcmp(v1, "value1"), 0);
                asserteq(strcmp(v2, "value2"), 0);
                asserteq(strcmp(v3, "value3"), 0);
            }
            
            unlink("/tmp/test_multi.sh");
        }
        
        free_session(session);
        free(session);
    }

    it("should persist aliases from sourced file") {
        Session *session = init_session(NULL, "/tmp/test_history");
        
        // Create a script with alias
        FILE *f = fopen("/tmp/test_alias.sh", "w");
        if (f) {
            fprintf(f, "alias ll='ls -l'\n");
            fclose(f);
            
            char *argv[] = {"source", "/tmp/test_alias.sh"};
            int result = builtin_source(2, argv, session);
            asserteq(result, 0);
            
            // Check if alias was set
            const char *alias_value = trie_get(session->aliases, "ll");
            assertneq(alias_value, NULL);
            if (alias_value) {
                asserteq(strcmp(alias_value, "ls -l"), 0);
            }
            
            unlink("/tmp/test_alias.sh");
        }
        
        free_session(session);
        free(session);
    }
}
