#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "session.h"
#include "snow/snow.h"

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
        Session *session = init_session(NULL, (char *)history_path);
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
        char *cwd = getcwd(NULL, 0);
        
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
        char *cwd1 = session->current_working_dir;
        
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
}
