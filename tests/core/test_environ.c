#include <stdlib.h>
#include <string.h>
#include "environ.h"
#include "snow/snow.h"

describe(environ) {
    it("should initialize environment") {
        Environ *env = init_environ(NULL);
        assertneq_ptr(env, NULL);
        free_environ(env);
        free(env);
    }

    it("should set and get variables") {
        Environ *env = init_environ(NULL);
        environ_set(env, "TEST_VAR", "test_value");
        
        char *value = environ_get(env, "TEST_VAR");
        assertneq_ptr(value, NULL);
        asserteq_str(value, "test_value");
        
        free_environ(env);
        free(env);
    }

    it("should check if variable exists") {
        Environ *env = init_environ(NULL);
        environ_set(env, "EXISTING", "value");
        
        assert(environ_contains(env, "EXISTING"));
        assert(!environ_contains(env, "NONEXISTENT"));
        
        free_environ(env);
        free(env);
    }

    it("should return NULL for nonexistent variables") {
        Environ *env = init_environ(NULL);
        char *value = environ_get(env, "NONEXISTENT");
        asserteq_ptr(value, NULL);
        
        free_environ(env);
        free(env);
    }

    it("should return default value for nonexistent variables") {
        Environ *env = init_environ(NULL);
        char *value = environ_get_default(env, "NONEXISTENT", "default");
        asserteq_str(value, "default");
        
        free_environ(env);
        free(env);
    }

    it("should set exit status") {
        Environ *env = init_environ(NULL);
        environ_set_exit_status(env, 42);
        
        char *status = environ_get(env, "?");
        assertneq_ptr(status, NULL);
        asserteq_str(status, "42");
        
        free_environ(env);
        free(env);
    }

    it("should set background PID") {
        Environ *env = init_environ(NULL);
        environ_set_background_pid(env, 12345);
        
        char *pid = environ_get(env, "!");
        assertneq_ptr(pid, NULL);
        asserteq_str(pid, "12345");
        
        free_environ(env);
        free(env);
    }

    it("should set last argument") {
        Environ *env = init_environ(NULL);
        environ_set_last_arg(env, "last_arg_value");
        
        char *arg = environ_get(env, "_");
        assertneq_ptr(arg, NULL);
        asserteq_str(arg, "last_arg_value");
        
        free_environ(env);
        free(env);
    }

    it("should copy environment") {
        Environ *src = init_environ(NULL);
        environ_set(src, "VAR1", "value1");
        environ_set(src, "VAR2", "value2");
        environ_set(src, "VAR3", "value3");
        
        Environ *dest = environ_copy(src, NULL);
        assertneq_ptr(dest, NULL);
        
        asserteq_str(environ_get(dest, "VAR1"), "value1");
        asserteq_str(environ_get(dest, "VAR2"), "value2");
        asserteq_str(environ_get(dest, "VAR3"), "value3");
        
        free_environ(src);
        free(src);
        free_environ(dest);
        free(dest);
    }

    it("should update variables") {
        Environ *env = init_environ(NULL);
        environ_set(env, "VAR", "old_value");
        environ_set(env, "VAR", "new_value");
        
        char *value = environ_get(env, "VAR");
        asserteq_str(value, "new_value");
        
        free_environ(env);
        free(env);
    }

    it("should convert to array") {
        Environ *env = init_environ(NULL);
        environ_set(env, "VAR1", "value1");
        environ_set(env, "VAR2", "value2");
        
        Array *arr = environ_to_array(env);
        assertneq_ptr(arr, NULL);
        assert(arr->count >= 2);
        
        free_array(arr);
        free(arr);
        free_environ(env);
        free(env);
    }

    it("should handle variables with empty values") {
        Environ *env = init_environ(NULL);
        environ_set(env, "EMPTY", "");
        
        char *value = environ_get(env, "EMPTY");
        assertneq_ptr(value, NULL);
        asserteq_str(value, "");
        
        free_environ(env);
        free(env);
    }

    it("should handle variables with special characters") {
        Environ *env = init_environ(NULL);
        environ_set(env, "SPECIAL", "value/with:special=chars");
        
        char *value = environ_get(env, "SPECIAL");
        asserteq_str(value, "value/with:special=chars");
        
        free_environ(env);
        free(env);
    }

    it("should handle multiple variables") {
        Environ *env = init_environ(NULL);
        
        for (int i = 0; i < 10; i++) {
            char key[32], value[32];
            snprintf(key, sizeof(key), "VAR_%d", i);
            snprintf(value, sizeof(value), "value_%d", i);
            environ_set(env, key, value);
        }
        
        for (int i = 0; i < 10; i++) {
            char key[32], expected[32];
            snprintf(key, sizeof(key), "VAR_%d", i);
            snprintf(expected, sizeof(expected), "value_%d", i);
            char *value = environ_get(env, key);
            asserteq_str(value, expected);
        }
        
        free_environ(env);
        free(env);
    }
}
