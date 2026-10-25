#include <stdlib.h>
#include <string.h>

#include "builtin.h"
#include "builtins/set.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

describe(set) {
    it("should identify set as builtin and special builtin") {
        assert(is_builtin("set"));
        assert(is_special_builtin("set"));
        assertneq(get_builtin("set"), NULL);
    }

    it("should list variables when called with no arguments") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "TEST_SET_VAR", "test_value");

        char *argv[] = {"set"};
        int   res    = builtin_set(1, argv, session);
        asserteq(res, 0);

        free_session(session);
        free(session);
    }

    it("should set positional parameters") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"set", "foo", "bar", "baz"};
        int   res    = builtin_set(4, argv, session);
        asserteq(res, 0);

        asserteq_str(environ_get(session->environ, "1"), "foo");
        asserteq_str(environ_get(session->environ, "2"), "bar");
        asserteq_str(environ_get(session->environ, "3"), "baz");
        asserteq_str(environ_get(session->environ, "#"), "3");
        asserteq_str(environ_get(session->environ, "@"), "foo bar baz");
        asserteq_str(environ_get(session->environ, "*"), "foo bar baz");

        free_session(session);
        free(session);
    }

    it("should set positional parameters with double dash") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"set", "--", "alpha", "beta"};
        int   res    = builtin_set(4, argv, session);
        asserteq(res, 0);

        asserteq_str(environ_get(session->environ, "1"), "alpha");
        asserteq_str(environ_get(session->environ, "2"), "beta");
        asserteq_str(environ_get(session->environ, "#"), "2");
        asserteq_str(environ_get(session->environ, "@"), "alpha beta");
        asserteq(environ_get(session->environ, "3"), NULL);

        free_session(session);
        free(session);
    }

    it("should unset all positional parameters with set --") {
        Session *session = init_session(NULL, NULL);

        char *argv1[] = {"set", "one", "two", "three"};
        builtin_set(4, argv1, session);
        asserteq_str(environ_get(session->environ, "#"), "3");

        char *argv2[] = {"set", "--"};
        int   res     = builtin_set(2, argv2, session);
        asserteq(res, 0);

        asserteq_str(environ_get(session->environ, "#"), "0");
        asserteq_str(environ_get(session->environ, "@"), "");
        asserteq_str(environ_get(session->environ, "*"), "");
        asserteq(environ_get(session->environ, "1"), NULL);
        asserteq(environ_get(session->environ, "2"), NULL);
        asserteq(environ_get(session->environ, "3"), NULL);

        free_session(session);
        free(session);
    }

    it("should preserve $0 when setting or unsetting positional parameters") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "0", "my_shell");

        char *argv1[] = {"set", "p1", "p2"};
        builtin_set(3, argv1, session);
        asserteq_str(environ_get(session->environ, "0"), "my_shell");

        char *argv2[] = {"set", "--"};
        builtin_set(2, argv2, session);
        asserteq_str(environ_get(session->environ, "0"), "my_shell");

        free_session(session);
        free(session);
    }

    it("should toggle errexit with -e and +e") {
        Session *session = init_session(NULL, NULL);
        assert(!session->options.errexit);

        char *argv_on[] = {"set", "-e"};
        asserteq(builtin_set(2, argv_on, session), 0);
        assert(session->options.errexit);

        char *argv_off[] = {"set", "+e"};
        asserteq(builtin_set(2, argv_off, session), 0);
        assert(!session->options.errexit);

        free_session(session);
        free(session);
    }

    it("should toggle xtrace with -x and +x") {
        Session *session = init_session(NULL, NULL);
        assert(!session->options.xtrace);

        char *argv_on[] = {"set", "-x"};
        asserteq(builtin_set(2, argv_on, session), 0);
        assert(session->options.xtrace);

        char *argv_off[] = {"set", "+x"};
        asserteq(builtin_set(2, argv_off, session), 0);
        assert(!session->options.xtrace);

        free_session(session);
        free(session);
    }

    it("should toggle nounset with -u and +u") {
        Session *session = init_session(NULL, NULL);
        assert(!session->options.nounset);

        char *argv_on[] = {"set", "-u"};
        asserteq(builtin_set(2, argv_on, session), 0);
        assert(session->options.nounset);

        char *argv_off[] = {"set", "+u"};
        asserteq(builtin_set(2, argv_off, session), 0);
        assert(!session->options.nounset);

        free_session(session);
        free(session);
    }

    it("should toggle noglob with -f and +f, syncing with filename_expansion") {
        Session *session = init_session(NULL, NULL);
        assert(session->features.filename_expansion);
        assert(!session->options.noglob);

        char *argv_on[] = {"set", "-f"};
        asserteq(builtin_set(2, argv_on, session), 0);
        assert(session->options.noglob);
        assert(!session->features.filename_expansion);

        char *argv_off[] = {"set", "+f"};
        asserteq(builtin_set(2, argv_off, session), 0);
        assert(!session->options.noglob);
        assert(session->features.filename_expansion);

        free_session(session);
        free(session);
    }

    it("should toggle options by name with -o and +o") {
        Session *session = init_session(NULL, NULL);

        char *argv_err_on[] = {"set", "-o", "errexit"};
        asserteq(builtin_set(3, argv_err_on, session), 0);
        assert(session->options.errexit);

        char *argv_err_off[] = {"set", "+o", "errexit"};
        asserteq(builtin_set(3, argv_err_off, session), 0);
        assert(!session->options.errexit);

        char *argv_pipe_on[] = {"set", "-o", "pipefail"};
        asserteq(builtin_set(3, argv_pipe_on, session), 0);
        assert(session->options.pipefail);

        char *argv_pipe_off[] = {"set", "+o", "pipefail"};
        asserteq(builtin_set(3, argv_pipe_off, session), 0);
        assert(!session->options.pipefail);

        char *argv_noglob_on[] = {"set", "-o", "noglob"};
        asserteq(builtin_set(3, argv_noglob_on, session), 0);
        assert(session->options.noglob);
        assert(!session->features.filename_expansion);

        char *argv_noglob_off[] = {"set", "+o", "noglob"};
        asserteq(builtin_set(3, argv_noglob_off, session), 0);
        assert(!session->options.noglob);
        assert(session->features.filename_expansion);

        free_session(session);
        free(session);
    }

    it("should display options with -o and +o without arguments") {
        Session *session = init_session(NULL, NULL);

        char *argv1[] = {"set", "-o"};
        asserteq(builtin_set(2, argv1, session), 0);

        char *argv2[] = {"set", "+o"};
        asserteq(builtin_set(2, argv2, session), 0);

        free_session(session);
        free(session);
    }

    it("should support combining flags like -ex") {
        Session *session = init_session(NULL, NULL);

        char *argv_on[] = {"set", "-ex"};
        asserteq(builtin_set(2, argv_on, session), 0);
        assert(session->options.errexit);
        assert(session->options.xtrace);

        char *argv_off[] = {"set", "+ex"};
        asserteq(builtin_set(2, argv_off, session), 0);
        assert(!session->options.errexit);
        assert(!session->options.xtrace);

        free_session(session);
        free(session);
    }

    it("should set options and positional parameters together") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"set", "-e", "hello", "world"};
        asserteq(builtin_set(4, argv, session), 0);
        assert(session->options.errexit);
        asserteq_str(environ_get(session->environ, "1"), "hello");
        asserteq_str(environ_get(session->environ, "2"), "world");
        asserteq_str(environ_get(session->environ, "#"), "2");

        free_session(session);
        free(session);
    }

    it("should handle single dash - to turn off verbose and xtrace") {
        Session *session         = init_session(NULL, NULL);
        session->options.verbose = true;
        session->options.xtrace  = true;

        char *argv[] = {"set", "-", "param1"};
        asserteq(builtin_set(3, argv, session), 0);
        assert(!session->options.verbose);
        assert(!session->options.xtrace);
        asserteq_str(environ_get(session->environ, "1"), "param1");
        asserteq_str(environ_get(session->environ, "#"), "1");

        free_session(session);
        free(session);
    }

    it("should reject invalid options and return 1") {
        Session *session = init_session(NULL, NULL);

        char *argv_bad_flag[] = {"set", "-z"};
        asserteq(builtin_set(2, argv_bad_flag, session), 1);

        char *argv_bad_name[] = {"set", "-o", "nonexistent_option"};
        asserteq(builtin_set(3, argv_bad_name, session), 1);

        free_session(session);
        free(session);
    }

    it("should execute set and expand positional parameters in shell "
       "commands") {
        Session *session = init_session(NULL, NULL);

        asserteq(execute_string("set a b c", session), 0);
        asserteq_str(environ_get(session->environ, "1"), "a");
        asserteq_str(environ_get(session->environ, "2"), "b");
        asserteq_str(environ_get(session->environ, "3"), "c");
        asserteq_str(environ_get(session->environ, "#"), "3");

        asserteq(execute_string("set -- first second", session), 0);
        asserteq_str(environ_get(session->environ, "1"), "first");
        asserteq_str(environ_get(session->environ, "2"), "second");
        asserteq_str(environ_get(session->environ, "#"), "2");
        asserteq(environ_get(session->environ, "3"), NULL);

        asserteq(execute_string("set --", session), 0);
        asserteq_str(environ_get(session->environ, "#"), "0");
        asserteq(environ_get(session->environ, "1"), NULL);

        free_session(session);
        free(session);
    }

    it("should respect errexit in command execution") {
        Session *session = init_session(NULL, NULL);

        // Without -e, sequence continues after failure
        session->exit_requested = false;
        asserteq(execute_string("false; true", session), 0);
        assert(!session->exit_requested);

        // With -e, sequence halts on failure
        execute_string("set -e", session);
        assert(session->options.errexit);
        session->exit_requested = false;

        execute_string("false; true", session);
        assert(session->exit_requested);

        free_session(session);
        free(session);
    }

    it("should respect nounset in variable expansion") {
        Session *session = init_session(NULL, NULL);

        execute_string("set -u", session);
        assert(session->options.nounset);

        // Unbound variable should fail
        int res = execute_string("echo $TEST_UNBOUND_VAR_XYZ_123", session);
        assertneq(res, 0);

        // Variable with default modifier should still work
        int res_default = execute_string(
            "echo ${TEST_UNBOUND_VAR_XYZ_123:-fallback}", session);
        asserteq(res_default, 0);

        free_session(session);
        free(session);
    }
}
