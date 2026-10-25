#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "builtins/read.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

describe(read_builtin) {
    it("should identify read as builtin and special builtin") {
        assert(is_builtin("read"));
        assert(is_special_builtin("read"));
        assertneq(get_builtin("read"), NULL);
    }

    it("should reject invalid identifiers") {
        Session *session = init_session(NULL, NULL);
        char    *argv[]  = {"read", "123invalid"};
        int      st      = builtin_read(2, argv, session);
        asserteq(st, 1);

        char *argv2[] = {"read", "valid_name", "not-valid"};
        st            = builtin_read(3, argv2, session);
        asserteq(st, 1);

        free_session(session);
        free(session);
    }

    it("should read line into REPLY by default") {
        Session *session = init_session(NULL, NULL);

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "   hello world   \n";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        char *argv[] = {"read"};
        int   st     = builtin_read(1, argv, session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        char *val = environ_get(session->environ, "REPLY");
        assertneq(val, NULL);
        asserteq_str(val, "   hello world   ");

        free_session(session);
        free(session);
    }

    it("should split fields with default IFS") {
        Session *session = init_session(NULL, NULL);

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "  one   two   three four five  \n";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        char *argv[] = {"read", "first", "second", "rest"};
        int   st     = builtin_read(4, argv, session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        asserteq_str(environ_get(session->environ, "first"), "one");
        asserteq_str(environ_get(session->environ, "second"), "two");
        asserteq_str(environ_get(session->environ, "rest"), "three four five");

        free_session(session);
        free(session);
    }

    it("should assign empty string to surplus variables") {
        Session *session = init_session(NULL, NULL);

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "single\n";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        char *argv[] = {"read", "a", "b", "c"};
        int   st     = builtin_read(4, argv, session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        asserteq_str(environ_get(session->environ, "a"), "single");
        asserteq_str(environ_get(session->environ, "b"), "");
        asserteq_str(environ_get(session->environ, "c"), "");

        free_session(session);
        free(session);
    }

    it("should split using custom IFS delimiter") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "IFS", ":");

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "root:x:0:0:root:/root:/bin/sh\n";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        char *argv[] = {"read", "user", "pass", "uid", "gid", "rest"};
        int   st     = builtin_read(6, argv, session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        asserteq_str(environ_get(session->environ, "user"), "root");
        asserteq_str(environ_get(session->environ, "pass"), "x");
        asserteq_str(environ_get(session->environ, "uid"), "0");
        asserteq_str(environ_get(session->environ, "gid"), "0");
        asserteq_str(environ_get(session->environ, "rest"),
                     "root:/root:/bin/sh");

        free_session(session);
        free(session);
    }

    it("should preserve whitespace when IFS is empty") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "IFS", "");

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "   spaced line   \n";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        char *argv[] = {"read", "line"};
        int   st     = builtin_read(2, argv, session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        asserteq_str(environ_get(session->environ, "line"),
                     "   spaced line   ");

        free_session(session);
        free(session);
    }

    it("should handle custom delimiter -d") {
        Session *session = init_session(NULL, NULL);

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "first:second";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        char *argv[] = {"read", "-d", ":", "val"};
        int   st     = builtin_read(4, argv, session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        asserteq_str(environ_get(session->environ, "val"), "first");

        free_session(session);
        free(session);
    }

    it("should handle -n max chars option") {
        Session *session = init_session(NULL, NULL);

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "123456789";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        char *argv[] = {"read", "-n", "4", "num"};
        int   st     = builtin_read(4, argv, session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        asserteq_str(environ_get(session->environ, "num"), "1234");

        free_session(session);
        free(session);
    }

    it("should work in execute_string with while loop and here-doc") {
        Session *session = init_session(NULL, NULL);

        const char *script = "OUT=\"\"\n"
                             "while IFS= read -r line; do\n"
                             "    OUT=\"${OUT}[$line]\"\n"
                             "done <<EOF\n"
                             "alpha\n"
                             "beta gamma\n"
                             "EOF\n";

        int st = execute_string(script, session);
        asserteq(st, 0);

        char *out = environ_get(session->environ, "OUT");
        assertneq(out, NULL);
        asserteq_str(out, "[alpha][beta gamma]");

        free_session(session);
        free(session);
    }

    it("should restore prefix assignments like IFS= after command completes") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "IFS", "initial");

        int p[2];
        assertneq(pipe(p), -1);
        const char *data = "  padded  \n";
        write(p[1], data, strlen(data));
        close(p[1]);

        int saved_stdin = dup(STDIN_FILENO);
        dup2(p[0], STDIN_FILENO);
        close(p[0]);

        int st = execute_string("IFS= read -r text", session);
        asserteq(st, 0);

        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);

        asserteq_str(environ_get(session->environ, "text"), "  padded  ");
        asserteq_str(environ_get(session->environ, "IFS"), "initial");

        free_session(session);
        free(session);
    }
}
