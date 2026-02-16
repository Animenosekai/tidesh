#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dirstack.h"
#include "snow/snow.h"

describe(dirstack) {
    it("should initialize a dirstack") {
        DirStack *stack = init_dirstack(NULL);
        assertneq(stack, NULL);
        assertneq(stack->stack, NULL);
        asserteq(stack->stack->count, 0);
        free_dirstack(stack);
    }

    it("should push directory onto stack") {
        DirStack *stack = init_dirstack(NULL);
        char *tmpdir = "/tmp";
        
        bool result = dirstack_pushd(stack, tmpdir);
        assert(result);
        asserteq(stack->stack->count, 1);
        
        free_dirstack(stack);
    }

    it("should peek at directory in stack") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        
        char *dir = dirstack_peek(stack, 0);
        assertneq(dir, NULL);
        // On macOS, /tmp is a symlink to /private/tmp, so accept both
        assert(strcmp(dir, "/tmp") == 0 || strcmp(dir, "/private/tmp") == 0);
        
        free(dir);
        free_dirstack(stack);
    }

    it("should pop directory from stack") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        
        char *original_cwd = getcwd(NULL, 0);
        
        bool result = dirstack_popd(stack);
        assert(result);
        asserteq(stack->stack->count, 0);
        
        char *new_cwd = getcwd(NULL, 0);
        asserteq_str(new_cwd, original_cwd);
        
        free(original_cwd);
        free(new_cwd);
        free_dirstack(stack);
    }

    it("should handle push pop sequence") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        dirstack_pushd(stack, "/var");
        
        asserteq(stack->stack->count, 2);
        
        dirstack_popd(stack);
        asserteq(stack->stack->count, 1);
        
        dirstack_popd(stack);
        asserteq(stack->stack->count, 0);
        
        free_dirstack(stack);
    }

    it("should return false for popd on empty stack") {
        DirStack *stack = init_dirstack(NULL);
        bool result = dirstack_popd(stack);
        assert(!result);
        
        free_dirstack(stack);
    }

    it("should peek at multiple levels") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        dirstack_pushd(stack, "/var");
        dirstack_pushd(stack, "/etc");
        
        char *dir0 = dirstack_peek(stack, 0);
        char *dir1 = dirstack_peek(stack, 1);
        char *dir2 = dirstack_peek(stack, 2);
        
        assertneq(dir0, NULL);
        assertneq(dir1, NULL);
        assertneq(dir2, NULL);
        
        free(dir0);
        free(dir1);
        free(dir2);
        free_dirstack(stack);
    }

    it("should return NULL for peek out of bounds") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        
        char *dir = dirstack_peek(stack, 5);
        asserteq(dir, NULL);
        
        free_dirstack(stack);
    }

    it("should swap directories") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        dirstack_pushd(stack, "/var");
        dirstack_pushd(stack, "/etc");
        
        // After pushd with full path, we're at /etc
        // swap(0) should swap /etc with /tmp
        bool result = dirstack_swap(stack, 0);
        assert(result);
        
        free_dirstack(stack);
    }

    it("should handle invalid dirstack swap") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        
        bool result = dirstack_swap(stack, 10);
        assert(!result);
        
        free_dirstack(stack);
    }

    it("should preserve stack after peek") {
        DirStack *stack = init_dirstack(NULL);
        dirstack_pushd(stack, "/tmp");
        
        size_t count_before = stack->stack->count;
        char *dir = dirstack_peek(stack, 0);
        size_t count_after = stack->stack->count;
        
        asserteq(count_before, count_after);
        
        free(dir);
        free_dirstack(stack);
    }

    it("should handle multiple pushd and popd cycles") {
        DirStack *stack = init_dirstack(NULL);
        
        for (int i = 0; i < 5; i++) {
            bool result = dirstack_pushd(stack, "/tmp");
            assert(result);
        }
        
        for (int i = 0; i < 5; i++) {
            bool result = dirstack_popd(stack);
            assert(result);
        }
        
        asserteq(stack->stack->count, 0);
        
        free_dirstack(stack);
    }
}
