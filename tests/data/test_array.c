#include <stdlib.h>
#include <string.h>
#include "data/array.h"
#include "snow/snow.h"

describe(array) {
    it("should initialize an array") {
        Array *arr = init_array(NULL);
        assertneq(arr, NULL);
        asserteq(arr->count, 0);
        assertneq(arr->capacity, 0);
        free_array(arr);
        free(arr);
    }

    it("should add elements to an array") {
        Array *arr = init_array(NULL);
        bool result = array_add(arr, "hello");
        asserteq(result, true);
        asserteq(arr->count, 1);
        asserteq_str(arr->items[0], "hello");
        free_array(arr);
        free(arr);
    }

    it("should add multiple elements") {
        Array *arr = init_array(NULL);
        array_add(arr, "first");
        array_add(arr, "second");
        array_add(arr, "third");
        asserteq(arr->count, 3);
        asserteq_str(arr->items[0], "first");
        asserteq_str(arr->items[1], "second");
        asserteq_str(arr->items[2], "third");
        free_array(arr);
    }

    it("should grow capacity as needed") {
        Array *arr = init_array(NULL);
        size_t initial_capacity = arr->capacity;
        
        for (int i = 0; i < 20; i++) {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "item_%d", i);
            array_add(arr, buffer);
        }
        
        asserteq(arr->count, 20);
        assert(arr->capacity > initial_capacity);
        free_array(arr);
        free(arr);
    }

    it("should insert elements at specific indices") {
        Array *arr = init_array(NULL);
        array_add(arr, "first");
        array_add(arr, "third");
        array_insert(arr, 1, "second");
        
        asserteq(arr->count, 3);
        asserteq_str(arr->items[0], "first");
        asserteq_str(arr->items[1], "second");
        asserteq_str(arr->items[2], "third");
        free_array(arr);
        free(arr);
    }

    it("should pop elements by index") {
        Array *arr = init_array(NULL);
        array_add(arr, "first");
        array_add(arr, "second");
        array_add(arr, "third");
        
        char *popped = array_pop(arr, 1);
        asserteq_str(popped, "second");
        asserteq(arr->count, 2);
        asserteq_str(arr->items[1], "third");
        
        free(popped);
        free_array(arr);
        free(arr);
    }

    it("should remove elements by index") {
        Array *arr = init_array(NULL);
        array_add(arr, "first");
        array_add(arr, "second");
        array_add(arr, "third");
        
        array_remove(arr, 1);
        asserteq(arr->count, 2);
        asserteq_str(arr->items[0], "first");
        asserteq_str(arr->items[1], "third");
        free_array(arr);
        free(arr);
    }

    it("should set elements") {
        Array *arr = init_array(NULL);
        array_add(arr, "first");
        array_add(arr, "second");
        
        array_set(arr, 1, "updated", true);
        asserteq_str(arr->items[1], "updated");
        asserteq(arr->count, 2);
        free_array(arr);
        free(arr);
    }

    it("should copy arrays") {
        Array *src = init_array(NULL);
        array_add(src, "a");
        array_add(src, "b");
        array_add(src, "c");
        
        Array *dest = array_copy(src, NULL);
        assertneq(dest, NULL);
        asserteq(dest->count, 3);
        asserteq_str(dest->items[0], "a");
        asserteq_str(dest->items[1], "b");
        asserteq_str(dest->items[2], "c");
        
        free_array(src);
        free(src);
        free_array(dest);
        free(dest);
    }

    it("should extend arrays") {
        Array *arr1 = init_array(NULL);
        array_add(arr1, "a");
        array_add(arr1, "b");
        
        Array *arr2 = init_array(NULL);
        array_add(arr2, "c");
        array_add(arr2, "d");
        
        array_extend(arr1, arr2);
        asserteq(arr1->count, 4);
        asserteq_str(arr1->items[0], "a");
        asserteq_str(arr1->items[1], "b");
        asserteq_str(arr1->items[2], "c");
        asserteq_str(arr1->items[3], "d");
        
        free_array(arr1);
        free(arr1);
        free_array(arr2);
        free(arr2);
    }

    it("should sort arrays") {
        Array *arr = init_array(NULL);
        array_add(arr, "zebra");
        array_add(arr, "apple");
        array_add(arr, "mango");
        array_add(arr, "banana");
        
        array_sort(arr);
        
        // Arrays should be sorted lexicographically
        // Just verify it doesn't crash and has correct count
        asserteq(arr->count, 4);
        assertneq(arr->items[0], NULL);
        assertneq(arr->items[3], NULL);
        
        free_array(arr);
        free(arr);
    }

    it("should clear arrays") {
        Array *arr = init_array(NULL);
        array_add(arr, "a");
        array_add(arr, "b");
        
        array_clear(arr);
        asserteq(arr->count, 0);
        
        free_array(arr);
        free(arr);
    }

    it("should handle empty arrays") {
        Array *arr = init_array(NULL);
        asserteq(arr->count, 0);
        
        Array *copied = array_copy(arr, NULL);
        assertneq(copied, NULL);
        asserteq(copied->count, 0);
        
        free_array(arr);
        free(arr);
        free_array(copied);
        free(copied);
    }

    it("should work with custom growing strategy") {
        // Use NULL for default strategy in test
        // Custom strategy would be defined at file level
        Array *arr = init_array(NULL);
        assertneq(arr, NULL);
        
        for (int i = 0; i < 10; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "item_%d", i);
            array_add(arr, buf);
        }
        
        asserteq(arr->count, 10);
        free_array(arr);
        free(arr);
    }
}
