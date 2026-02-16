#include <stdbool.h> /* bool */
#include <stdlib.h>  /* malloc, free, realloc, qsort */
#include <string.h>  /* strdup, strcmp, memmove */

#include "data/array.h"

/* The default growing strategy: double the capacity */
static size_t default_growing_strategy(size_t current_capacity,
                                       size_t required_capacity) {
    if (required_capacity < 1)
        return 16;
    size_t new_capacity = current_capacity;
    while (new_capacity < required_capacity) {
        new_capacity *= 2;
    }
    return new_capacity;
}

Array *init_array_with_strategy(Array *array,
                                size_t (*growing_strategy)(size_t, size_t)) {
    if (!array) {
        array = malloc(sizeof(Array));
        if (!array)
            return NULL;
    } else {
        free_array(array);
    }

    array->growing_strategy =
        growing_strategy ? growing_strategy : default_growing_strategy;

    array->capacity = array->growing_strategy(0, 0);
    if (array->capacity < 1)
        array->capacity = 1;

    array->items = malloc(array->capacity * sizeof(char *));
    if (!array->items) {
        free(array);
        return NULL;
    }

    array->count = 0;
    return array;
}

Array *init_array(Array *array) {
    return init_array_with_strategy(array, default_growing_strategy);
}

/* Ensure the array has at least min_capacity */
static void ensure_capacity(Array *array, size_t min_capacity) {
    if (array->capacity >= min_capacity)
        return;

    while (array->capacity < min_capacity) {
        array->capacity =
            array->growing_strategy(array->capacity, min_capacity);
    }
    array->items = realloc(array->items, array->capacity * sizeof(char *));
}

bool array_add(Array *array, char *str) {
    ensure_capacity(array, array->count + 1);
    array->items[array->count] = strdup(str);
    if (!array->items[array->count]) {
        return false;
    }
    array->count++;
    return true;
}

void array_extend(Array *array, Array *other) {
    for (size_t i = 0; i < other->count; i++) {
        array_add(array, other->items[i]);
    }
}

void array_insert(Array *array, size_t index, char *str) {
    if (index > array->count)
        index = array->count;
    ensure_capacity(array, array->count + 1);

    // Shift right items
    memmove(&array->items[index + 1], &array->items[index],
            (array->count - index) * sizeof(char *));

    array->items[index] = strdup(str);
    array->count++;
}

char *array_pop(Array *array, size_t index) {
    if (index >= array->count)
        return NULL;

    char *ret = array->items[index];

    // Shift left items
    memmove(&array->items[index], &array->items[index + 1],
            (array->count - index - 1) * sizeof(char *));

    array->count--;
    return ret;
}

void array_remove(Array *array, size_t index) {
    char *s = array_pop(array, index);
    free(s);
}

void array_set(Array *array, size_t index, char *str, bool free_old) {
    if (index >= array->count)
        return;

    if (free_old)
        free(array->items[index]);

    array->items[index] = strdup(str);
}

void free_array(Array *array) {
    for (size_t i = 0; i < array->count; i++) {
        free(array->items[i]);
        array->items[i] = NULL;
    }
    free(array->items);
    array->items    = NULL;
    array->count    = 0;
    array->capacity = 0;
}

void array_sort(Array *array) {
    if (array->count > 1) {
        qsort(array->items, array->count, sizeof(char *),
              (int (*)(const void *, const void *))strcmp);
    }
}

void array_clear(Array *array) {
    for (size_t i = 0; i < array->count; i++) {
        free(array->items[i]);
        array->items[i] = NULL;
    }
    array->count = 0;
}

Array *array_copy(Array *src, Array *dest) {
    if (!src)
        return NULL;

    dest = init_array_with_strategy(dest, src->growing_strategy);
    if (!dest)
        return NULL;

    ensure_capacity(dest, src->count);

    for (size_t i = 0; i < src->count; i++) {
        dest->items[i] = strdup(src->items[i]);
        if (!dest->items[i]) {
            free_array(dest);
            free(dest);
            return NULL;
        }
    }

    dest->count = src->count;
    return dest;
}
