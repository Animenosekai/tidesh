#include <stdlib.h>
#include <string.h>

#include "builtin.h"
#include "builtins/set.h"
#include "builtins/shift.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

describe(shift) {
    it("should identify shift as builtin and special builtin") {
        assert(is_builtin("shift"));
        assert(is_special_builtin("shift"));
        assertneq(get_builtin("shift"), NULL);
    }

    it("should shift by 1 by default") {
        Session *session = init_session(NULL, NULL);

        char *set_argv[] = {"set", "a", "b", "c"};
        builtin_set(4, set_argv, session);
        asserteq_str(environ_get(session->environ, "#"), "3");

        char *shift_argv[] = {"shift"};
        int   res          = builtin_shift(1, shift_argv, session);
        asserteq(res, 0);

        asserteq_str(environ_get(session->environ, "1"), "b");
        asserteq_str(environ_get(session->environ, "2"), "c");
        asserteq_str(environ_get(session->environ, "#"), "2");
        asserteq_str(environ_get(session->environ, "@"), "b c");
        asserteq_str(environ_get(session->environ, "*"), "b c");
        asserteq(environ_get(session->environ, "3"), NULL);

        free_session(session);
        free(session);
    }

    it("should shift by n") {
        Session *session = init_session(NULL, NULL);

        char *set_argv[] = {"set", "a", "b", "c", "d", "e"};
        builtin_set(6, set_argv, session);
        asserteq_str(environ_get(session->environ, "#"), "5");

        char *shift_argv[] = {"shift", "2"};
        int   res          = builtin_shift(2, shift_argv, session);
        asserteq(res, 0);

        asserteq_str(environ_get(session->environ, "1"), "c");
        asserteq_str(environ_get(session->environ, "2"), "d");
        asserteq_str(environ_get(session->environ, "3"), "e");
        asserteq_str(environ_get(session->environ, "#"), "3");
        asserteq_str(environ_get(session->environ, "@"), "c d e");
        asserteq(environ_get(session->environ, "4"), NULL);
        asserteq(environ_get(session->environ, "5"), NULL);

        free_session(session);
        free(session);
    }

    it("should shift all parameters") {
        Session *session = init_session(NULL, NULL);

        char *set_argv[] = {"set", "x", "y"};
        builtin_set(3, set_argv, session);
        asserteq_str(environ_get(session->environ, "#"), "2");

        char *shift_argv[] = {"shift", "2"};
        int   res          = builtin_shift(2, shift_argv, session);
        asserteq(res, 0);

        asserteq_str(environ_get(session->environ, "#"), "0");
        asserteq_str(environ_get(session->environ, "@"), "");
        asserteq_str(environ_get(session->environ, "*"), "");
        asserteq(environ_get(session->environ, "1"), NULL);
        asserteq(environ_get(session->environ, "2"), NULL);

        free_session(session);
        free(session);
    }

    it("should do nothing when shifting by 0") {
        Session *session = init_session(NULL, NULL);

        char *set_argv[] = {"set", "foo", "bar"};
        builtin_set(3, set_argv, session);

        char *shift_argv[] = {"shift", "0"};
        int   res          = builtin_shift(2, shift_argv, session);
        asserteq(res, 0);

        asserteq_str(environ_get(session->environ, "1"), "foo");
        asserteq_str(environ_get(session->environ, "2"), "bar");
        asserteq_str(environ_get(session->environ, "#"), "2");

        free_session(session);
        free(session);
    }

    it("should fail when shift count exceeds $#") {
        Session *session = init_session(NULL, NULL);

        char *set_argv[] = {"set", "one", "two"};
        builtin_set(3, set_argv, session);

        char *shift_argv[] = {"shift", "3"};
        int   res          = builtin_shift(2, shift_argv, session);
        asserteq(res, 1);

        // Parameters should be unchanged
        asserteq_str(environ_get(session->environ, "1"), "one");
        asserteq_str(environ_get(session->environ, "2"), "two");
        asserteq_str(environ_get(session->environ, "#"), "2");

        free_session(session);
        free(session);
    }

    it("should fail when shifting with $# == 0") {
        Session *session = init_session(NULL, NULL);

        char *shift_argv[] = {"shift"};
        int   res          = builtin_shift(1, shift_argv, session);
        asserteq(res, 1);

        free_session(session);
        free(session);
    }

    it("should reject invalid arguments") {
        Session *session = init_session(NULL, NULL);

        char *set_argv[] = {"set", "a", "b"};
        builtin_set(3, set_argv, session);

        char *argv_neg[] = {"shift", "-1"};
        asserteq(builtin_shift(2, argv_neg, session), 1);

        char *argv_alpha[] = {"shift", "abc"};
        asserteq(builtin_shift(2, argv_alpha, session), 1);

        char *argv_empty[] = {"shift", ""};
        asserteq(builtin_shift(2, argv_empty, session), 1);

        char *argv_extra[] = {"shift", "1", "2"};
        asserteq(builtin_shift(3, argv_extra, session), 1);

        // Parameters unchanged
        asserteq_str(environ_get(session->environ, "#"), "2");

        free_session(session);
        free(session);
    }

    it("should preserve $0 during shift") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "0", "my_script.sh");

        char *set_argv[] = {"set", "p1", "p2"};
        builtin_set(3, set_argv, session);

        char *shift_argv[] = {"shift"};
        builtin_shift(1, shift_argv, session);

        asserteq_str(environ_get(session->environ, "0"), "my_script.sh");
        asserteq_str(environ_get(session->environ, "1"), "p2");
        asserteq_str(environ_get(session->environ, "#"), "1");

        free_session(session);
        free(session);
    }

    it("should execute shift in shell command strings") {
        Session *session = init_session(NULL, NULL);

        asserteq(execute_string("set a b c d", session), 0);
        asserteq(execute_string("shift", session), 0);
        asserteq_str(environ_get(session->environ, "1"), "b");
        asserteq_str(environ_get(session->environ, "#"), "3");

        asserteq(execute_string("shift 2", session), 0);
        asserteq_str(environ_get(session->environ, "1"), "d");
        asserteq_str(environ_get(session->environ, "#"), "1");

        asserteq(execute_string("shift", session), 0);
        asserteq_str(environ_get(session->environ, "#"), "0");
        asserteq(environ_get(session->environ, "1"), NULL);

        // Next shift should fail
        assertneq(execute_string("shift", session), 0);

        free_session(session);
        free(session);
    }

    it("should work in a while loop shifting parameters") {
        Session *session = init_session(NULL, NULL);

        asserteq(execute_string("set first second third", session), 0);
        asserteq(execute_string("while [ $# -gt 0 ]; do shift; done", session),
                 0);

        asserteq_str(environ_get(session->environ, "#"), "0");
        asserteq(environ_get(session->environ, "1"), NULL);

        free_session(session);
        free(session);
    }
}
