#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "builtin.h"
#include "builtins/colon.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

describe(colon) {
    it("should identify : as builtin and special builtin") {
        assert(is_builtin(":"));
        assert(is_special_builtin(":"));
        assertneq(get_builtin(":"), NULL);
    }

    it("should return 0 when called without arguments") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {":"};
        int   res    = builtin_colon(1, argv, session);
        asserteq(res, 0);

        free_session(session);
        free(session);
    }

    it("should return 0 when called with arbitrary arguments") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {":", "foo", "bar", "--flag", "-x", "baz 123"};
        int   res    = builtin_colon(6, argv, session);
        asserteq(res, 0);

        free_session(session);
        free(session);
    }

    it("should execute : in shell strings and set exit status to 0") {
        Session *session = init_session(NULL, NULL);

        environ_set_exit_status(session->environ, 42);
        asserteq_str(environ_get(session->environ, "?"), "42");

        asserteq(execute_string(":", session), 0);
        asserteq_str(environ_get(session->environ, "?"), "0");

        free_session(session);
        free(session);
    }

    it("should expand arguments when executing :") {
        Session *session = init_session(NULL, NULL);

        environ_set(session->environ, "COLON_TEST_VAR", "expanded_val");
        asserteq(execute_string(": $COLON_TEST_VAR ${COLON_TEST_VAR:-fallback}",
                                session),
                 0);
        asserteq_str(environ_get(session->environ, "?"), "0");

        free_session(session);
        free(session);
    }

    it("should handle redirections with :") {
        Session    *session  = init_session(NULL, NULL);
        const char *tmp_file = "/tmp/test_tidesh_colon_redir.txt";

        // Create and write something
        asserteq(
            execute_string("echo 'hello' > /tmp/test_tidesh_colon_redir.txt",
                           session),
            0);

        struct stat st;
        asserteq(stat(tmp_file, &st), 0);
        assert(st.st_size > 0);

        // Truncate using : > file
        asserteq(
            execute_string(": > /tmp/test_tidesh_colon_redir.txt", session), 0);
        asserteq(stat(tmp_file, &st), 0);
        asserteq(st.st_size, 0);

        unlink(tmp_file);
        free_session(session);
        free(session);
    }

    it("should work in loops as a condition") {
        Session *session = init_session(NULL, NULL);

        asserteq(execute_string("while :; do break; done", session), 0);

        free_session(session);
        free(session);
    }
}
