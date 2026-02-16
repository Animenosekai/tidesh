#include <stdlib.h>
#include <string.h>
#include "data/trie.h"
#include "snow/snow.h"

describe(trie) {
    it("should initialize a trie") {
        Trie *trie = init_trie(NULL);
        assertneq_ptr(trie, NULL);
        free_trie(trie);
        free(trie);
    }

    it("should set and get values") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "hello", "world");
        
        char *value = trie_get(trie, "hello");
        assertneq(value, NULL);
        asserteq_str(value, "world");
        
        free_trie(trie);
        free(trie);
    }

    it("should add keys without values") {
        Trie *trie = init_trie(NULL);
        trie_add(trie, "key1");
        trie_add(trie, "key2");
        
        assert(trie_contains(trie, "key1"));
        assert(trie_contains(trie, "key2"));
        
        free_trie(trie);
        free(trie);
    }

    it("should check if trie contains key") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "existing", "value");
        
        assert(trie_contains(trie, "existing"));
        assert(!trie_contains(trie, "nonexistent"));
        
        free_trie(trie);
        free(trie);
    }

    it("should return NULL for nonexistent keys") {
        Trie *trie = init_trie(NULL);
        char *value = trie_get(trie, "nonexistent");
        asserteq(value, NULL);
        
        free_trie(trie);
        free(trie);
    }

    it("should handle prefix matching") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "apple", "fruit");
        trie_set(trie, "application", "software");
        trie_set(trie, "apply", "verb");
        
        assert(trie_starts_with(trie, "app"));
        assert(trie_starts_with(trie, "appl"));
        assert(!trie_starts_with(trie, "banana"));
        
        free_trie(trie);
        free(trie);
    }

    it("should get all keys starting with prefix") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "cat", "animal");
        trie_set(trie, "car", "vehicle");
        trie_set(trie, "card", "object");
        trie_set(trie, "dog", "animal");
        
        Array *matches = trie_starting_with(trie, "ca");
        assertneq(matches, NULL);
        asserteq(matches->count, 3);
        
        free_array(matches);
        free(matches);
        free_trie(trie);
        free(trie);
    }

    it("should delete keys") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "key1", "value1");
        trie_set(trie, "key2", "value2");
        
        bool result = trie_delete_key(trie, "key1");
        assert(result);
        assert(!trie_contains(trie, "key1"));
        assert(trie_contains(trie, "key2"));
        
        free_trie(trie);
        free(trie);
    }

    it("should return false when deleting nonexistent key") {
        Trie *trie = init_trie(NULL);
        bool result = trie_delete_key(trie, "nonexistent");
        assert(!result);
        
        free_trie(trie);
        free(trie);
    }

    it("should copy trie") {
        Trie *src = init_trie(NULL);
        trie_set(src, "key1", "value1");
        trie_set(src, "key2", "value2");
        trie_set(src, "key3", "value3");
        
        Trie *dest = trie_copy(src, NULL);
        assertneq_ptr(dest, NULL);
        
        asserteq_str(trie_get(dest, "key1"), "value1");
        asserteq_str(trie_get(dest, "key2"), "value2");
        asserteq_str(trie_get(dest, "key3"), "value3");
        
        free_trie(src);
        free(src);
        free_trie(dest);
        free(dest);
    }

    it("should handle empty strings") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "", "empty");
        
        char *value = trie_get(trie, "");
        asserteq_str(value, "empty");
        
        free_trie(trie);
        free(trie);
    }

    it("should handle single character keys") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "a", "first");
        trie_set(trie, "b", "second");
        
        asserteq_str(trie_get(trie, "a"), "first");
        asserteq_str(trie_get(trie, "b"), "second");
        
        free_trie(trie);
        free(trie);
    }

    it("should handle keys with common prefixes") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "test", "full");
        trie_set(trie, "tes", "partial");
        trie_set(trie, "te", "shorter");
        trie_set(trie, "t", "short");
        
        asserteq_str(trie_get(trie, "test"), "full");
        asserteq_str(trie_get(trie, "tes"), "partial");
        asserteq_str(trie_get(trie, "te"), "shorter");
        asserteq_str(trie_get(trie, "t"), "short");
        
        free_trie(trie);
        free(trie);
    }

    it("should handle many keys") {
        Trie *trie = init_trie(NULL);
        
        for (int i = 0; i < 100; i++) {
            char key[32], value[32];
            snprintf(key, sizeof(key), "key_%d", i);
            snprintf(value, sizeof(value), "value_%d", i);
            trie_set(trie, key, value);
        }
        
        for (int i = 0; i < 100; i++) {
            char key[32], expected[32];
            snprintf(key, sizeof(key), "key_%d", i);
            snprintf(expected, sizeof(expected), "value_%d", i);
            char *value = trie_get(trie, key);
            asserteq_str(value, expected);
        }
        
        free_trie(trie);
        free(trie);
    }

    it("should handle special characters in keys") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "key-with-dashes", "value1");
        trie_set(trie, "key_with_underscores", "value2");
        trie_set(trie, "key.with.dots", "value3");
        trie_set(trie, "key/with/slashes", "value4");
        
        asserteq_str(trie_get(trie, "key-with-dashes"), "value1");
        asserteq_str(trie_get(trie, "key_with_underscores"), "value2");
        asserteq_str(trie_get(trie, "key.with.dots"), "value3");
        asserteq_str(trie_get(trie, "key/with/slashes"), "value4");
        
        free_trie(trie);
        free(trie);
    }

    it("should get all starting with when no matches") {
        Trie *trie = init_trie(NULL);
        trie_set(trie, "apple", "fruit");
        
        Array *matches = trie_starting_with(trie, "zoo");
        assertneq(matches, NULL);
        asserteq(matches->count, 0);
        
        free_array(matches);
        free(matches);
        free_trie(trie);
        free(trie);
    }
}
