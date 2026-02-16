#include <stdbool.h> /* bool, true, false */
#include <stdlib.h>  /* malloc, free, realloc */
#include <string.h>  /* strdup, strlen, memmove */

#include "data/dynamic.h"

/* The default growing strategy: double the capacity */
static size_t default_growing_strategy(size_t current_capacity,
                                       size_t required_capacity) {
    if (required_capacity < 1)
        return 8;
    size_t new_capacity = current_capacity;
    while (new_capacity < required_capacity) {
        new_capacity *= 2;
    }
    return new_capacity;
}

Dynamic *init_dynamic_with_strategy(Dynamic *value,
                                    size_t (*growing_strategy)(size_t,
                                                               size_t)) {
    bool allocated_struct = false;

    if (!value) {
        value = malloc(sizeof(Dynamic));
        if (!value)
            return NULL;
        allocated_struct = true;
    } else {
        // Safety: ensure it doesn't hold old data if reused
        free_dynamic(value);
    }

    value->growing_strategy =
        growing_strategy ? growing_strategy : default_growing_strategy;
    value->capacity = value->growing_strategy(0, 0);

    if (value->capacity < 1)
        value->capacity = 8;

    value->value = malloc(value->capacity * sizeof(char));
    if (!value->value) {
        if (allocated_struct)
            free(value); // Only free if we allocated it
        return NULL;
    }

    value->length   = 0;
    value->value[0] = '\0';
    return value;
}

Dynamic *init_dynamic(Dynamic *value) {
    return init_dynamic_with_strategy(value, default_growing_strategy);
}

void dynamic_append(Dynamic *value, char character) {
    size_t needed = value->length + 1 + 1;
    if (needed >= value->capacity) {
        size_t new_capacity = value->growing_strategy(value->capacity, needed);
        char  *new_value = realloc(value->value, new_capacity * sizeof(char));
        if (!new_value)
            return; // allocation failed
        value->value    = new_value;
        value->capacity = new_capacity;
    }

    value->value[value->length] = character;
    value->length               = needed - 1;
    value->value[value->length] = '\0';
}

void dynamic_extend(Dynamic *value, char *string) {
    if (string == NULL)
        return;
    size_t length = strlen(string);
    size_t needed = value->length + length + 1;
    if (needed >= value->capacity) {
        size_t new_capacity = value->growing_strategy(value->capacity, needed);
        char  *new_value = realloc(value->value, new_capacity * sizeof(char));
        if (!new_value)
            return; // allocation failed
        value->value    = new_value;
        value->capacity = new_capacity;
    }
    memcpy(value->value + value->length, string, length);
    value->length               = needed - 1;
    value->value[value->length] = '\0';
}

void dynamic_prepend(Dynamic *value, char character) {
    // length + new character + null terminator
    size_t needed = value->length + 2;

    if (needed >= value->capacity) {
        size_t new_capacity = value->growing_strategy(value->capacity, needed);
        char  *new_value = realloc(value->value, new_capacity * sizeof(char));
        if (!new_value)
            return;
        value->value    = new_value;
        value->capacity = new_capacity;
    }

    // Shift existing characters + null terminator
    memmove(value->value + 1, value->value, value->length + 1);
    value->value[0] = character;
    value->length++;
}

void dynamic_delete_last(Dynamic *value) {
    if (value->length <= 0)
        return;

    value->length--;
    value->value[value->length] = '\0';
}

void dynamic_insert(Dynamic *value, size_t position, char *string) {
    if (string == NULL)
        return;

    size_t str_len = strlen(string);
    if (position > value->length)
        position = value->length;

    size_t needed = value->length + str_len + 1;
    if (needed >= value->capacity) {
        size_t new_capacity = value->growing_strategy(value->capacity, needed);
        char  *new_value = realloc(value->value, new_capacity * sizeof(char));
        if (!new_value)
            return; // allocation failed
        value->value    = new_value;
        value->capacity = new_capacity;
    }

    // Shift existing characters to make space
    memmove(value->value + position + str_len, value->value + position,
            value->length - position + 1);

    // Insert new string
    memcpy(value->value + position, string, str_len);
    value->length               = needed - 1;
    value->value[value->length] = '\0';
}

void dynamic_remove(Dynamic *value, size_t position, size_t length) {
    if (position >= value->length || length == 0)
        return;

    if (position + length > value->length) {
        length = value->length - position;
    }

    // Shift characters to remove the specified segment
    memmove(value->value + position, value->value + position + length,
            value->length - (position + length) + 1);

    value->length -= length;
}

void dynamic_clear(Dynamic *value) {
    value->length = 0;
    if (value->value)
        value->value[0] = '\0';
}

void free_dynamic(Dynamic *value) {
    if (!value)
        return;

    if (value->value) {
        free(value->value);
        value->value = NULL;
    }

    value->capacity = 0;
    value->length   = 0;
}

char *dynamic_to_string(Dynamic *value) {
    if (value->length == 0 || !value->value) {
        return strdup("");
    }
    return strdup(value->value);
}

Dynamic *dynamic_copy(Dynamic *src, Dynamic *dest) {
    if (!src)
        return NULL;

    dest = init_dynamic_with_strategy(dest, src->growing_strategy);
    if (!dest)
        return NULL;

    dynamic_extend(dest, src->value);
    return dest;
}
