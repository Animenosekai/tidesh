#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtin.h"
#include "builtins/exec.h"
#include "environ.h"
#include "execute.h"
#include "session.h"
#include "snow/snow.h"

static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, sz, f);
    buf[read_bytes]   = '\0';
    fclose(f);
    return buf;
}

describe(exec_builtin) {
    it("should identify exec as builtin and special builtin") {
        assert(is_builtin("exec"));
        assert(is_special_builtin("exec"));
        assertneq(get_builtin("exec"), NULL);
    }

    it("should set permanent_redirections with no command") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"exec", NULL};
        int   res    = builtin_exec(1, argv, session);
        asserteq(res, 0);
        assert(session->permanent_redirections);

        free_session(session);
        free(session);
    }

    it("should support -- as end of options with no command") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"exec", "--", NULL};
        int   res    = builtin_exec(2, argv, session);
        asserteq(res, 0);
        assert(session->permanent_redirections);

        free_session(session);
        free(session);
    }

    it("should fail on invalid options") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"exec", "-z", NULL};
        int   res    = builtin_exec(2, argv, session);
        asserteq(res, 2);

        char *argv_a[] = {"exec", "-a", NULL};
        res            = builtin_exec(2, argv_a, session);
        asserteq(res, 2);

        free_session(session);
        free(session);
    }

    it("should fail with 127 if command not found") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"exec", "/nonexistent/path/for/exec/cmd", NULL};
        int   res    = builtin_exec(2, argv, session);
        asserteq(res, 127);
        assert(session->exit_requested);

        session->exit_requested = false;
        char *argv_name[]       = {"exec", "nonexistent_command_xyz123", NULL};
        res                     = builtin_exec(2, argv_name, session);
        asserteq(res, 127);

        free_session(session);
        free(session);
    }

    it("should fail with 126 if target is a directory") {
        Session *session = init_session(NULL, NULL);

        char *argv[] = {"exec", "/tmp", NULL};
        int   res    = builtin_exec(2, argv, session);
        asserteq(res, 126);
        assert(session->exit_requested);

        free_session(session);
        free(session);
    }

    it("should fail with 126 if target is not executable") {
        Session    *session  = init_session(NULL, NULL);
        const char *tmp_file = "/tmp/test_exec_noexec.txt";
        FILE       *f        = fopen(tmp_file, "w");
        if (f) {
            fprintf(f, "not executable\n");
            fclose(f);
            chmod(tmp_file, 0644);

            char *argv[] = {"exec", (char *)tmp_file, NULL};
            int   res    = builtin_exec(2, argv, session);
            asserteq(res, 126);
            unlink(tmp_file);
        }

        free_session(session);
        free(session);
    }

    it("should permanently redirect file descriptors via exec") {
        const char *out_path = "/tmp/test_exec_permanent_redir.txt";
        unlink(out_path);

        pid_t pid = fork();
        asserteq(pid >= 0, true);
        if (pid == 0) {
            Session *session = init_session(NULL, NULL);
            char     cmd[256];
            snprintf(cmd, sizeof(cmd), "exec > %s", out_path);
            execute_string(cmd, session);
            execute_string("echo exec permanent line 1", session);
            execute_string("echo exec permanent line 2", session);
            exit(0);
        }

        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status));
        asserteq(WEXITSTATUS(status), 0);

        char *contents = read_file_contents(out_path);
        assertneq(contents, NULL);
        asserteq_str(contents,
                     "exec permanent line 1\nexec permanent line 2\n");
        free(contents);
        unlink(out_path);
    }

    it("should redirect inside subshell without affecting parent") {
        const char *out_path = "/tmp/test_exec_subshell_redir.txt";
        unlink(out_path);

        Session *session = init_session(NULL, NULL);
        char     cmd[256];
        snprintf(cmd, sizeof(cmd), "( exec > %s; echo inside subshell )",
                 out_path);
        int st = execute_string(cmd, session);
        asserteq(st, 0);

        char *contents = read_file_contents(out_path);
        assertneq(contents, NULL);
        asserteq_str(contents, "inside subshell\n");
        free(contents);
        unlink(out_path);

        free_session(session);
        free(session);
    }

    it("should replace process image with an external command") {
        pid_t pid = fork();
        asserteq(pid >= 0, true);
        if (pid == 0) {
            Session *session = init_session(NULL, NULL);
            char    *argv[]  = {"exec", "/bin/echo", "hello", NULL};
            builtin_exec(3, argv, session);
            exit(1); // Should never reach here if exec succeeded
        }

        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status));
        asserteq(WEXITSTATUS(status), 0);
    }

    it("should pass custom argv0 with -a flag") {
        const char *out_path = "/tmp/test_exec_argv0.txt";
        unlink(out_path);

        pid_t pid = fork();
        asserteq(pid >= 0, true);
        if (pid == 0) {
            Session *session = init_session(NULL, NULL);
            char     cmd[256];
            snprintf(cmd, sizeof(cmd), "exec > %s", out_path);
            execute_string(cmd, session);

            char *argv[] = {"exec",    "-a", "custom_sh_name", "/bin/sh", "-c",
                            "echo $0", NULL};
            builtin_exec(6, argv, session);
            exit(1);
        }

        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status));
        asserteq(WEXITSTATUS(status), 0);

        char *contents = read_file_contents(out_path);
        assertneq(contents, NULL);
        asserteq_str(contents, "custom_sh_name\n");
        free(contents);
        unlink(out_path);
    }

    it("should prepend dash to argv0 with -l login shell flag") {
        const char *out_path = "/tmp/test_exec_login.txt";
        unlink(out_path);

        pid_t pid = fork();
        asserteq(pid >= 0, true);
        if (pid == 0) {
            Session *session = init_session(NULL, NULL);
            char     cmd[256];
            snprintf(cmd, sizeof(cmd), "exec > %s", out_path);
            execute_string(cmd, session);

            char *argv[] = {"exec",    "-l", "-a",      "login_sh",
                            "/bin/sh", "-c", "echo $0", NULL};
            builtin_exec(7, argv, session);
            exit(1);
        }

        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status));
        asserteq(WEXITSTATUS(status), 0);

        char *contents = read_file_contents(out_path);
        assertneq(contents, NULL);
        asserteq_str(contents, "-login_sh\n");
        free(contents);
        unlink(out_path);
    }

    it("should execute with clean environment when -c is specified") {
        const char *out_path = "/tmp/test_exec_clean_env.txt";
        unlink(out_path);

        pid_t pid = fork();
        asserteq(pid >= 0, true);
        if (pid == 0) {
            Session *session = init_session(NULL, NULL);
            environ_set(session->environ, "TIDESH_TEST_VAR",
                        "should_be_cleared");

            char cmd[256];
            snprintf(cmd, sizeof(cmd), "exec > %s", out_path);
            execute_string(cmd, session);

            char *argv[] = {"exec", "-c", "/usr/bin/env", NULL};
            builtin_exec(3, argv, session);
            exit(1);
        }

        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status));
        asserteq(WEXITSTATUS(status), 0);

        char *contents = read_file_contents(out_path);
        assertneq(contents, NULL);
        assert(strstr(contents, "TIDESH_TEST_VAR") == NULL);
        free(contents);
        unlink(out_path);
    }

    it("should execute shebang scripts properly via exec") {
        const char *script_path = "/tmp/test_exec_script.sh";
        FILE       *f           = fopen(script_path, "w");
        assertneq(f, NULL);
        fprintf(f, "#!/bin/sh\nexit 37\n");
        fclose(f);
        chmod(script_path, 0755);

        pid_t pid = fork();
        asserteq(pid >= 0, true);
        if (pid == 0) {
            Session *session = init_session(NULL, NULL);
            char    *argv[]  = {"exec", (char *)script_path, NULL};
            builtin_exec(2, argv, session);
            exit(1);
        }

        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status));
        asserteq(WEXITSTATUS(status), 37);

        unlink(script_path);
    }
}
