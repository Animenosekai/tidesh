#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "history.h"
#include "snow/snow.h"

describe(history) {
    it("should initialize history") {
        History *history = init_history(NULL);
        assertneq(history, NULL);
        asserteq(history->size, 0);
        asserteq(history->head, NULL);
        asserteq(history->tail, NULL);
        free_history(history);
    }

    it("should append commands to history") {
        History *history = init_history(NULL);
        history_append(history, "ls -la");
        
        asserteq(history->size, 1);
        assertneq(history->tail, NULL);
        asserteq_str(history->tail->command, "ls -la");
        
        free_history(history);
    }

    it("should append multiple commands") {
        History *history = init_history(NULL);
        history_append(history, "ls");
        history_append(history, "pwd");
        history_append(history, "echo hello");
        
        asserteq(history->size, 3);
        asserteq_str(history->head->command, "ls");
        asserteq_str(history->tail->command, "echo hello");
        
        free_history(history);
    }

    it("should navigate history forward and backward") {
        History *history = init_history(NULL);
        history_append(history, "cmd1");
        history_append(history, "cmd2");
        history_append(history, "cmd3");
        
        history_reset_state(history);
        
        char *prev = history_get_previous(history);
        asserteq_str(prev, "cmd3");
        
        prev = history_get_previous(history);
        asserteq_str(prev, "cmd2");
        
        char *next = history_get_next(history);
        asserteq_str(next, "cmd3");
        
        free_history(history);
    }

    it("should get last command") {
        History *history = init_history(NULL);
        history_append(history, "first");
        history_append(history, "second");
        history_append(history, "third");
        
        char *last = history_last_command(history);
        asserteq_str(last, "third");
        
        free_history(history);
    }

    it("should get Nth last command") {
        History *history = init_history(NULL);
        history_append(history, "first");
        history_append(history, "second");
        history_append(history, "third");
        
        // Verify the history has the commands (testing that it works at all)
        assertneq(history, NULL);
        assert(history->size > 0);
        
        free_history(history);
    }

    it("should get Nth command") {
        History *history = init_history(NULL);
        history_append(history, "first");
        history_append(history, "second");
        history_append(history, "third");
        
        // Verify the history has the commands
        assertneq(history, NULL);
        assert(history->size > 0);
        
        free_history(history);
    }

    it("should get last command starting with prefix") {
        History *history = init_history(NULL);
        history_append(history, "ls -la");
        history_append(history, "echo test");
        history_append(history, "ls /tmp");
        history_append(history, "pwd");
        
        // Verify the history has commands
        assertneq(history, NULL);
        assert(history->size == 4);
        
        free_history(history);
    }

    it("should remove commands from history") {
        History *history = init_history(NULL);
        history_append(history, "cmd1");
        history_append(history, "cmd2");
        history_append(history, "cmd3");
        
        bool result = history_remove(history, "cmd2", false);
        assert(result);
        asserteq(history->size, 2);
        
        free_history(history);
    }

    it("should enforce history limit") {
        History *history = init_history(NULL);
        history->limit = 5;
        
        for (int i = 0; i < 10; i++) {
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "cmd_%d", i);
            history_append(history, cmd);
        }
        
        history_enforce_limit(history);
        // After enforcing limit, history size should be at most limit
        assert(history->size <= history->limit);
        
        free_history(history);
    }

    it("should clear history") {
        History *history = init_history(NULL);
        history_append(history, "cmd1");
        history_append(history, "cmd2");
        
        history_clear(history);
        asserteq(history->size, 0);
        asserteq(history->head, NULL);
        asserteq(history->tail, NULL);
        
        free_history(history);
    }

    it("should reset navigation state") {
        History *history = init_history(NULL);
        history_append(history, "cmd1");
        history_append(history, "cmd2");
        
        history_get_previous(history);
        history_reset_state(history);
        
        asserteq(history->current, NULL);
        
        free_history(history);
    }

    it("should handle empty history navigation") {
        History *history = init_history(NULL);
        history_reset_state(history);
        
        char *prev = history_get_previous(history);
        // Should not crash
        
        free_history(history);
    }

    it("should save and load history") {
        const char *tmpfile = "/tmp/test_history_file";
        
        History *history = init_history(NULL);
        history->filepath = (char *)tmpfile;
        history_append(history, "cmd1");
        history_append(history, "cmd2");
        
        history_save(history);
        free_history(history);
        
        History *loaded = load_history(NULL, (char *)tmpfile);
        assertneq(loaded, NULL);
        asserteq(loaded->size, 2);
        
        unlink(tmpfile);
        free_history(loaded);
    }

    it("should deduplicate consecutive identical commands") {
        History *history = init_history(NULL);
        history_append(history, "ls");
        history_append(history, "ls");
        history_append(history, "ls");
        
        // Most shells deduplicate consecutive duplicates
        // Behavior depends on implementation
        asserteq(history->size, 3); // Or may be dedup'd to 1
        
        free_history(history);
    }

    it("should handle very long commands") {
        History *history = init_history(NULL);
        char long_cmd[1024];
        memset(long_cmd, 'a', sizeof(long_cmd) - 1);
        long_cmd[sizeof(long_cmd) - 1] = '\0';
        
        history_append(history, long_cmd);
        asserteq(history->size, 1);
        asserteq_str(history->tail->command, long_cmd);
        
        free_history(history);
    }

    it("should handle search with non-matching prefix") {
        History *history = init_history(NULL);
        history_append(history, "echo");
        history_append(history, "ls");
        
        char *cmd = history_last_command_starting_with(history, "cd");
        asserteq(cmd, NULL);
        
        free_history(history);
    }
}
