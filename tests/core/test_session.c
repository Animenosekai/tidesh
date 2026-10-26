#include "session.h"
#include "snow/snow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

describe(session) {
    it("should initialize a session") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session, NULL);
        assertneq_ptr(session->environ, NULL);
        assertneq_ptr(session->history, NULL);
        assertneq_ptr(session->dirstack, NULL);
        assertneq_ptr(session->aliases, NULL);
        free_session(session);
        free(session);
    }

    it("should initialize session with custom history path") {
        const char *history_path = "/tmp/custom_history";
        Session    *session      = init_session(NULL, (char *)history_path);
        assertneq_ptr(session, NULL);
        free_session(session);
        free(session);
    }

    it("should have environment initialized") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session->environ, NULL);
        free_session(session);
        free(session);
    }

    it("should have history initialized") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session->history, NULL);
        free_session(session);
        free(session);
    }

    it("should have aliases initialized") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session->aliases, NULL);
        free_session(session);
        free(session);
    }

    it("should have dirstack initialized") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session->dirstack, NULL);
        free_session(session);
        free(session);
    }

    it("should not request exit initially") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assert(!session->exit_requested);
        free_session(session);
        free(session);
    }

    it("should update working directory") {
        Session *session = init_session(NULL, "/tmp/test_history");
        char    *cwd     = getcwd(NULL, 0);

        update_working_dir(session);
        assertneq_ptr(session->current_working_dir, NULL);

        free(cwd);
        free_session(session);
        free(session);
    }

    it("should initialize working directory on startup") {
        Session *session = init_session(NULL, "/tmp/test_history");
        assertneq_ptr(session->current_working_dir, NULL);
        free_session(session);
        free(session);
    }

    it("should update PATH in session") {
        Session *session = init_session(NULL, "/tmp/test_history");
        update_path(session);
        // PATH should be updated without crashing
        free_session(session);
        free(session);
    }

    it("should handle multiple sessions") {
        Session *session1 = init_session(NULL, "/tmp/test_history1");
        Session *session2 = init_session(NULL, "/tmp/test_history2");

        assertneq_ptr(session1, NULL);
        assertneq_ptr(session2, NULL);
        assertneq_ptr(session1, session2);

        free_session(session1);
        free(session1);
        free_session(session2);
        free(session2);
    }

    it("should maintain separate environments for different sessions") {
        Session *session1 = init_session(NULL, "/tmp/test_history1");
        Session *session2 = init_session(NULL, "/tmp/test_history2");

        environ_set(session1->environ, "TEST_VAR", "session1");
        environ_set(session2->environ, "TEST_VAR", "session2");

        asserteq_str(environ_get(session1->environ, "TEST_VAR"), "session1");
        asserteq_str(environ_get(session2->environ, "TEST_VAR"), "session2");

        free_session(session1);
        free(session1);
        free_session(session2);
        free(session2);
    }

    it("should preserve current working directory after operations") {
        Session *session = init_session(NULL, "/tmp/test_history");
        char    *cwd1    = session->current_working_dir;

        update_working_dir(session);
        char *cwd2 = session->current_working_dir;

        asserteq_str(cwd1, cwd2);
        free_session(session);
        free(session);
    }

    it("should pass multiple init_session calls") {
        for (int i = 0; i < 5; i++) {
            Session *session = init_session(NULL, "/tmp/test_history");
            assertneq_ptr(session, NULL);
            free_session(session);
            free(session);
        }
    }

    it("should not re-execute enter/exit hooks when moving between "
       "subdirectories of a hooked directory") {
        char *orig_cwd = getcwd(NULL, 0);
        mkdir("/tmp/tidesh_test_hooks", 0755);
        mkdir("/tmp/tidesh_test_hooks/.tidesh-hooks", 0755);
        mkdir("/tmp/tidesh_test_hooks/sub1", 0755);
        mkdir("/tmp/tidesh_test_hooks/sub2", 0755);

        FILE *f = fopen("/tmp/tidesh_test_hooks/.tidesh-hooks/enter.sh", "w");
        assertneq_ptr(f, NULL);
        fprintf(f, "echo enter >> /tmp/tidesh_test_hooks/log.txt\n");
        fclose(f);

        f = fopen("/tmp/tidesh_test_hooks/.tidesh-hooks/exit.sh", "w");
        assertneq_ptr(f, NULL);
        fprintf(f, "echo exit >> /tmp/tidesh_test_hooks/log.txt\n");
        fclose(f);

        // Start session in sub1
        chdir("/tmp/tidesh_test_hooks/sub1");
        Session *session = init_session(NULL, "/tmp/test_history");
        run_initial_parent_hooks(session);

        // Clear the log from initial startup enter
        f = fopen("/tmp/tidesh_test_hooks/log.txt", "w");
        if (f)
            fclose(f);

        // Change directory from sub1 to sub2
        chdir("/tmp/tidesh_test_hooks/sub2");
        update_working_dir(session);

        // Log should be empty (no enter or exit on tidesh_test_hooks)
        f               = fopen("/tmp/tidesh_test_hooks/log.txt", "r");
        char   buf[256] = {0};
        size_t bytes    = 0;
        if (f) {
            bytes = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
        }
        asserteq(bytes, 0);

        // Now move out of the hooked directory
        chdir("/tmp");
        update_working_dir(session);

        // Log should now have "exit\n"
        f = fopen("/tmp/tidesh_test_hooks/log.txt", "r");
        assertneq_ptr(f, NULL);
        bytes      = fread(buf, 1, sizeof(buf) - 1, f);
        buf[bytes] = '\0';
        fclose(f);
        asserteq_str(buf, "exit\n");

        free_session(session);
        free(session);

        if (orig_cwd) {
            chdir(orig_cwd);
            free(orig_cwd);
        }

        unlink("/tmp/tidesh_test_hooks/log.txt");
        unlink("/tmp/tidesh_test_hooks/.tidesh-hooks/enter.sh");
        unlink("/tmp/tidesh_test_hooks/.tidesh-hooks/exit.sh");
        rmdir("/tmp/tidesh_test_hooks/.tidesh-hooks");
        rmdir("/tmp/tidesh_test_hooks/sub1");
        rmdir("/tmp/tidesh_test_hooks/sub2");
        rmdir("/tmp/tidesh_test_hooks");
    }

    it("should respect hooks_disabled flag") {
        char *orig_cwd = getcwd(NULL, 0);
        mkdir("/tmp/tidesh_test_hooks_dis", 0755);
        mkdir("/tmp/tidesh_test_hooks_dis/.tidesh-hooks", 0755);

        FILE *f =
            fopen("/tmp/tidesh_test_hooks_dis/.tidesh-hooks/enter.sh", "w");
        assertneq_ptr(f, NULL);
        fprintf(f, "echo enter >> /tmp/tidesh_test_hooks_dis/log.txt\n");
        fclose(f);

        chdir("/tmp/tidesh_test_hooks_dis");
        Session *session        = init_session(NULL, "/tmp/test_history");
        session->hooks_disabled = true;

        run_initial_parent_hooks(session);

        // Log should not exist or be empty
        f = fopen("/tmp/tidesh_test_hooks_dis/log.txt", "r");
        asserteq_ptr(f, NULL);

        free_session(session);
        free(session);

        if (orig_cwd) {
            chdir(orig_cwd);
            free(orig_cwd);
        }

        unlink("/tmp/tidesh_test_hooks_dis/.tidesh-hooks/enter.sh");
        rmdir("/tmp/tidesh_test_hooks_dis/.tidesh-hooks");
        rmdir("/tmp/tidesh_test_hooks_dis");
    }
}
