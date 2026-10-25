#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "builtin.h"
#include "builtins/return.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

describe(return_builtin) {
    it("should identify return as builtin and special builtin") {
        assert(is_builtin("return"));
        assert(is_special_builtin("return"));
        assertneq(get_builtin("return"), NULL);
    }

    it("should fail when called outside a function or sourced script") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"return"};
        int   res    = builtin_return(1, argv, session);
        asserteq(res, 1);

        // Through execute_string
        int st = execute_string("return", session);
        asserteq(st, 1);

        free_session(session);
        free(session);
    }

    it("should return specified exit code from a function") {
        Session *session = init_session(NULL, NULL);

        execute_string("fn() { return 42; }", session);
        int st = execute_string("fn", session);
        asserteq(st, 42);
        asserteq_str(environ_get(session->environ, "?"), "42");

        free_session(session);
        free(session);
    }

    it("should return exit status of previous command when called with no "
       "arguments") {
        Session *session = init_session(NULL, NULL);

        execute_string("fn_zero() { (exit 0); return; }", session);
        asserteq(execute_string("fn_zero", session), 0);

        execute_string("fn_nonzero() { (exit 7); return; }", session);
        asserteq(execute_string("fn_nonzero", session), 7);

        free_session(session);
        free(session);
    }

    it("should wrap return code to 8-bit unsigned value") {
        Session *session = init_session(NULL, NULL);

        execute_string("fn_wrap() { return 256; }", session);
        asserteq(execute_string("fn_wrap", session), 0);

        execute_string("fn_wrap2() { return 257; }", session);
        asserteq(execute_string("fn_wrap2", session), 1);

        free_session(session);
        free(session);
    }

    it("should fail when given non-numeric argument") {
        Session *session = init_session(NULL, NULL);

        execute_string("fn_bad() { return abc; }", session);
        asserteq(execute_string("fn_bad", session), 2);

        free_session(session);
        free(session);
    }

    it("should fail when given too many arguments") {
        Session *session = init_session(NULL, NULL);

        execute_string("fn_many() { return 1 2; }", session);
        asserteq(execute_string("fn_many", session), 1);

        free_session(session);
        free(session);
    }

    it("should terminate function execution immediately") {
        Session *session = init_session(NULL, NULL);

        execute_string("fn_early() { FOO=before; return 0; FOO=after; }",
                       session);
        execute_string("fn_early", session);
        asserteq_str(environ_get(session->environ, "FOO"), "before");

        free_session(session);
        free(session);
    }

    it("should break out of loops and blocks inside a function") {
        Session *session = init_session(NULL, NULL);

        execute_string("fn_loop() { while true; do return 15; done; "
                       "LOOP_AFTER=yes; }",
                       session);
        asserteq(execute_string("fn_loop", session), 15);
        assert(environ_get(session->environ, "LOOP_AFTER") == NULL);

        execute_string("fn_for() { for i in 1 2 3; do return 25; done; }",
                       session);
        asserteq(execute_string("fn_for", session), 25);

        execute_string("fn_block() { { return 35; }; BLOCK_AFTER=yes; }",
                       session);
        asserteq(execute_string("fn_block", session), 35);
        assert(environ_get(session->environ, "BLOCK_AFTER") == NULL);

        free_session(session);
        free(session);
    }

    it("should only return from the innermost function in nested calls") {
        Session *session = init_session(NULL, NULL);

        execute_string("inner() { return 12; UNREACHED_INNER=yes; }", session);
        execute_string("outer() { inner; INNER_RET=$?; return 24; }", session);
        asserteq(execute_string("outer", session), 24);
        assert(environ_get(session->environ, "UNREACHED_INNER") == NULL);
        asserteq_str(environ_get(session->environ, "INNER_RET"), "12");

        free_session(session);
        free(session);
    }

    it("should return from a sourced script and resume caller") {
        Session *session = init_session(NULL, NULL);

        char tmp_file[] = "/tmp/tidesh_test_return_XXXXXX";
        int  fd         = mkstemp(tmp_file);
        assert(fd >= 0);

        const char *script = "SRC_VAR=first\nreturn 33\nSRC_VAR=second\n";
        write(fd, script, strlen(script));
        close(fd);

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "source %s; AFTER_SRC=$?", tmp_file);
        execute_string(cmd, session);

        asserteq_str(environ_get(session->environ, "SRC_VAR"), "first");
        asserteq_str(environ_get(session->environ, "AFTER_SRC"), "33");

        unlink(tmp_file);
        free_session(session);
        free(session);
    }

    it("should trigger errexit when non-zero return is unhandled") {
        Session *session = init_session(NULL, NULL);

        session->options.errexit = true;
        execute_string("fn_err() { return 1; }", session);
        execute_string("fn_err; UNREACHED=yes", session);
        assert(environ_get(session->environ, "UNREACHED") == NULL);

        free_session(session);
        free(session);
    }
}
