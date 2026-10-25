#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "builtins/command.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

describe(command_builtin) {
    it("should identify command as builtin and special builtin") {
        assert(is_builtin("command"));
        assert(is_special_builtin("command"));
        assertneq(get_builtin("command"), NULL);
    }

    it("should return 0 when called with no command") {
        Session *session = init_session(NULL, NULL);

        char *argv1[] = {"command", NULL};
        asserteq(builtin_command(1, argv1, session), 0);

        char *argv2[] = {"command", "-p", NULL};
        asserteq(builtin_command(2, argv2, session), 0);

        char *argv3[] = {"command", "--", NULL};
        asserteq(builtin_command(2, argv3, session), 0);

        free_session(session);
        free(session);
    }

    it("should fail with 2 on invalid option") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"command", "-z", NULL};
        asserteq(builtin_command(2, argv, session), 2);

        free_session(session);
        free(session);
    }

    it("should query command with -v") {
        Session *session = init_session(NULL, NULL);

        // Builtin
        char *out_builtin = execute_string_stdout("command -v cd", session);
        assertneq(out_builtin, NULL);
        asserteq_str(out_builtin, "cd");
        free(out_builtin);

        // Function
        execute_string("my_test_fn() { :; }", session);
        char *out_fn = execute_string_stdout("command -v my_test_fn", session);
        assertneq(out_fn, NULL);
        asserteq_str(out_fn, "my_test_fn");
        free(out_fn);

        // External command in PATH
        char *out_ext = execute_string_stdout("command -v ls", session);
        assertneq(out_ext, NULL);
        assert(strstr(out_ext, "ls") != NULL);
        free(out_ext);

        // Nonexistent command
        int st = execute_string("command -v nonexistent_command_xyz123", session);
        asserteq(st, 1);

        char *out_none = execute_string_stdout("command -v nonexistent_command_xyz123", session);
        asserteq_str(out_none, "");
        free(out_none);

        free_session(session);
        free(session);
    }

    it("should query command with -V") {
        Session *session = init_session(NULL, NULL);

        // Builtin
        char *out_builtin = execute_string_stdout("command -V cd", session);
        assertneq(out_builtin, NULL);
        assert(strstr(out_builtin, "built-in") != NULL);
        free(out_builtin);

        // Function
        execute_string("my_test_fn_v() { :; }", session);
        char *out_fn = execute_string_stdout("command -V my_test_fn_v", session);
        assertneq(out_fn, NULL);
        assert(strstr(out_fn, "function") != NULL);
        free(out_fn);

        // External command
        char *out_ext = execute_string_stdout("command -V ls", session);
        assertneq(out_ext, NULL);
        assert(strstr(out_ext, "ls is") != NULL);
        free(out_ext);

        // Nonexistent command
        int st = execute_string("command -V nonexistent_command_xyz123", session);
        asserteq(st, 1);

        free_session(session);
        free(session);
    }

    it("should bypass function lookup and invoke builtin directly") {
        char    *orig_cwd = getcwd(NULL, 0);
        Session *session  = init_session(NULL, NULL);

        // Define a function that overrides cd
        execute_string("cd() { TIDE_FUNC_CALLED=1; }", session);

        // Invoke cd through command
        execute_string("command cd /tmp", session);

        // Function should not have been called
        asserteq(environ_get(session->environ, "TIDE_FUNC_CALLED"), NULL);

        // Session directory should have changed to /tmp
        assert(strcmp(session->current_working_dir, "/tmp") == 0 ||
               strcmp(session->current_working_dir, "/private/tmp") == 0);

        if (orig_cwd) {
            chdir(orig_cwd);
            free(orig_cwd);
        }

        free_session(session);
        free(session);
    }

    it("should bypass function lookup and invoke external command") {
        Session *session = init_session(NULL, NULL);

        // Define function overriding echo
        execute_string("echo() { TIDE_ECHO_FUNC=1; }", session);

        char *out = execute_string_stdout(
            "command /bin/echo hello_from_command", session);
        assertneq(out, NULL);
        asserteq_str(out, "hello_from_command");
        free(out);

        asserteq(environ_get(session->environ, "TIDE_ECHO_FUNC"), NULL);

        free_session(session);
        free(session);
    }

    it("should execute standard path utilities with -p flag") {
        Session *session = init_session(NULL, NULL);

        // Clear PATH in environment
        environ_set(session->environ, "PATH", "");

        // With -p, should still find standard utilities like ls or cat
        int st = execute_string("command -p /bin/cat /dev/null", session);
        asserteq(st, 0);

        st = execute_string("command -p cat /dev/null", session);
        asserteq(st, 0);

        free_session(session);
        free(session);
    }

    it("should return 127 when command is not found") {
        Session *session = init_session(NULL, NULL);

        int st = execute_string("command nonexistent_cmd_xyz123", session);
        asserteq(st, 127);

        free_session(session);
        free(session);
    }

    it("should propagate exit status from executed command") {
        Session *session = init_session(NULL, NULL);

        int st = execute_string("command /bin/sh -c 'exit 47'", session);
        asserteq(st, 47);

        free_session(session);
        free(session);
    }
}
