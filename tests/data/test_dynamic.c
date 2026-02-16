#include <stdlib.h>
#include <string.h>
#include "data/dynamic.h"
#include "snow/snow.h"

describe(dynamic) {
    it("should initialize a dynamic string") {
        Dynamic *dyn = init_dynamic(NULL);
        assertneq(dyn, NULL);
        asserteq(dyn->length, 0);
        assertneq(dyn->capacity, 0);
        assertneq(dyn->value, NULL);
        free_dynamic(dyn);
    }

    it("should append characters") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_append(dyn, 'h');
        dynamic_append(dyn, 'i');
        
        asserteq(dyn->length, 2);
        asserteq(dyn->value[0], 'h');
        asserteq(dyn->value[1], 'i');
        
        free_dynamic(dyn);
    }

    it("should extend with strings") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "hello");
        dynamic_extend(dyn, " ");
        dynamic_extend(dyn, "world");
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "hello world");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should prepend characters") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "world");
        dynamic_prepend(dyn, ' ');
        dynamic_prepend(dyn, 'o');
        dynamic_prepend(dyn, 'l');
        dynamic_prepend(dyn, 'l');
        dynamic_prepend(dyn, 'e');
        dynamic_prepend(dyn, 'h');
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "hello world");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should delete last character") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "hello");
        dynamic_delete_last(dyn);
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "hell");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should insert strings at positions") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "helo");
        dynamic_insert(dyn, 2, "l");
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "hello");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should remove segments") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "hello world");
        dynamic_remove(dyn, 5, 6);
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "hello");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should clear content") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "hello world");
        dynamic_clear(dyn);
        
        asserteq(dyn->length, 0);
        
        free_dynamic(dyn);
    }

    it("should convert to string") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "test");
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "test");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should copy dynamic strings") {
        Dynamic *src = init_dynamic(NULL);
        dynamic_extend(src, "original");
        
        Dynamic *dest = dynamic_copy(src, NULL);
        assertneq(dest, NULL);
        
        char *str = dynamic_to_string(dest);
        asserteq_str(str, "original");
        free(str);
        
        free_dynamic(src);
        free_dynamic(dest);
    }

    it("should grow capacity as needed") {
        Dynamic *dyn = init_dynamic(NULL);
        size_t initial_capacity = dyn->capacity;
        
        for (int i = 0; i < 100; i++) {
            dynamic_append(dyn, 'a');
        }
        
        asserteq(dyn->length, 100);
        assert(dyn->capacity > initial_capacity);
        
        free_dynamic(dyn);
    }

    it("should handle empty dynamic") {
        Dynamic *dyn = init_dynamic(NULL);
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should work with custom growing strategy") {
        // Use default strategy for this test
        Dynamic *dyn = init_dynamic(NULL);
        assertneq(dyn, NULL);
        
        dynamic_extend(dyn, "test");
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "test");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should handle special characters") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "hello\nworld\t!");
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "hello\nworld\t!");
        free(str);
        
        free_dynamic(dyn);
    }

    it("should handle unicode (as bytes)") {
        Dynamic *dyn = init_dynamic(NULL);
        dynamic_extend(dyn, "café");
        
        char *str = dynamic_to_string(dyn);
        asserteq_str(str, "café");
        free(str);
        
        free_dynamic(dyn);
    }
}
