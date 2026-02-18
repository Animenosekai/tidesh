/** test_features.c
 *
 * Tests for feature flag system
 */

#include <stdlib.h>
#include <string.h>

#include "data/trie.h"
#include "execute.h"
#include "expand.h"
#include "features.h"
#include "session.h"
#include "snow/snow.h"

describe(features) {
    it("should initialize with all features enabled by default") {
        Features features;
        init_features(&features);

        assert(features.variable_expansion);
        assert(features.tilde_expansion);
        assert(features.brace_expansion);
        assert(features.filename_expansion);
        assert(features.alias_expansion);
        assert(features.job_control);
        assert(features.history);
        assert(features.directory_stack);
    }

    it("should initialize minimal with all features disabled") {
        Features features;
        init_features_minimal(&features);

        assert(!features.variable_expansion);
        assert(!features.tilde_expansion);
        assert(!features.brace_expansion);
        assert(!features.filename_expansion);
        assert(!features.alias_expansion);
        assert(!features.job_control);
        assert(!features.history);
        assert(!features.directory_stack);
    }

    it("should enable all expansions") {
        Features features;
        init_features_minimal(&features);

        features_enable_all_expansions(&features);

        assert(features.variable_expansion);
        assert(features.tilde_expansion);
        assert(features.brace_expansion);
        assert(features.filename_expansion);
    }

    it("should disable all expansions") {
        Features features;
        init_features(&features);

        features_disable_all_expansions(&features);

        assert(!features.variable_expansion);
        assert(!features.tilde_expansion);
        assert(!features.brace_expansion);
        assert(!features.filename_expansion);
    }

    it("should integrate with session") {
        Session *session = init_session(NULL, NULL);

        assert(session->features.variable_expansion);
        assert(session->features.brace_expansion);

        session->features.variable_expansion = false;
        assert(!session->features.variable_expansion);

        free_session(session);
        free(session);
    }

    it("should skip variable expansion when disabled") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "TEST_VAR", "expanded");

        // With variable expansion enabled
        session->features.variable_expansion = true;
        Array *result = full_expansion("$TEST_VAR", session);
        assertneq_ptr(result, NULL);
        asserteq(result->count, 1);
        asserteq_str(result->items[0], "expanded");
        free_array(result);
        free(result);

        // With variable expansion disabled
        session->features.variable_expansion = false;
        result = full_expansion("$TEST_VAR", session);
        assertneq_ptr(result, NULL);
        asserteq(result->count, 1);
        asserteq_str(result->items[0], "$TEST_VAR");
        free_array(result);
        free(result);

        free_session(session);
        free(session);
    }

    it("should skip tilde expansion when disabled") {
        Session *session = init_session(NULL, NULL);
        char *home = environ_get(session->environ, "HOME");

        if (home) {
            // With tilde expansion enabled
            session->features.tilde_expansion = true;
            Array *result = full_expansion("~", session);
            assertneq_ptr(result, NULL);
            asserteq(result->count, 1);
            asserteq_str(result->items[0], home);
            free_array(result);
            free(result);

            // With tilde expansion disabled
            session->features.tilde_expansion = false;
            result = full_expansion("~", session);
            assertneq_ptr(result, NULL);
            asserteq(result->count, 1);
            asserteq_str(result->items[0], "~");
            free_array(result);
            free(result);
        }

        free_session(session);
        free(session);
    }

    it("should skip brace expansion when disabled") {
        Session *session = init_session(NULL, NULL);

        // With brace expansion enabled
        session->features.brace_expansion = true;
        Array *result = full_expansion("test{1,2}", session);
        assertneq_ptr(result, NULL);
        asserteq(result->count, 2);
        asserteq_str(result->items[0], "test1");
        asserteq_str(result->items[1], "test2");
        free_array(result);
        free(result);

        // With brace expansion disabled
        session->features.brace_expansion = false;
        result = full_expansion("test{1,2}", session);
        assertneq_ptr(result, NULL);
        asserteq(result->count, 1);
        asserteq_str(result->items[0], "test{1,2}");
        free_array(result);
        free(result);

        free_session(session);
        free(session);
    }

    it("should skip alias expansion when disabled") {
        Session *session = init_session(NULL, NULL);
        trie_set(session->aliases, "ll", "ls -l");

        // With alias expansion enabled
        session->features.alias_expansion = true;
        CommandInfo info = get_command_info("ll", session);
        asserteq(info.type, COMMAND_ALIAS);
        asserteq_str(info.path, "ls -l");
        free(info.path);

        // With alias expansion disabled
        session->features.alias_expansion = false;
        info = get_command_info("ll", session);
        assertneq(info.type, COMMAND_ALIAS);

        free_session(session);
        free(session);
    }

    it("should handle multiple feature flags together") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "VAR", "value");
        trie_set(session->aliases, "myalias", "echo");

        // Disable multiple features
        session->features.variable_expansion = false;
        session->features.brace_expansion    = false;
        session->features.alias_expansion    = false;

        // Variables should not expand
        Array *result = full_expansion("$VAR", session);
        asserteq_str(result->items[0], "$VAR");
        free_array(result);
        free(result);

        // Braces should not expand
        result = full_expansion("{a,b}", session);
        asserteq(result->count, 1);
        asserteq_str(result->items[0], "{a,b}");
        free_array(result);
        free(result);

        // Aliases should not resolve
        CommandInfo info = get_command_info("myalias", session);
        assertneq(info.type, COMMAND_ALIAS);

        free_session(session);
        free(session);
    }

    it("should allow re-enabling features after disabling") {
        Session *session = init_session(NULL, NULL);
        environ_set(session->environ, "TEST", "works");

        // Disable
        session->features.variable_expansion = false;
        Array *result = full_expansion("$TEST", session);
        asserteq_str(result->items[0], "$TEST");
        free_array(result);
        free(result);

        // Re-enable
        session->features.variable_expansion = true;
        result = full_expansion("$TEST", session);
        asserteq_str(result->items[0], "works");
        free_array(result);
        free(result);

        free_session(session);
        free(session);
    }
}
