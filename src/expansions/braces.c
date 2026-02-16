/* Brace expansion implementation */

#include <ctype.h>   /* isspace, isalpha */
#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* snprintf */
#include <stdlib.h>  /* malloc, free, strtol */
#include <string.h>  /* strdup, strndup, strlen */

#include "data/array.h"   /* Array, init_array, array_add, free_array */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, free_dynamic, dynamic_to_string */
#include "expansions/braces.h" /* brace_expansion */
#include "session.h"           /* Session */

/* Find the matching closing brace, handling nested braces */
static int find_closing_brace(char *str, int start) {
    int depth = 1;
    int i     = start;

    while (str[i] && depth > 0) {
        if (str[i] == '{')
            depth++;
        else if (str[i] == '}')
            depth--;
        i++;
    }

    return (depth == 0) ? i - 1 : -1;
}

/* Check if string is a valid range {a..b} */
static bool is_range(char *str, int start, int end) {
    if (end - start < 4)
        return false; // Need at least "x..y"

    for (int i = start; i < end - 1; i++) {
        if (str[i] == '.' && str[i + 1] == '.') {
            return true;
        }
    }
    return false;
}

/* Check if string contains a top-level comma */
static bool has_comma(char *str, int start, int end) {
    int depth = 0;
    for (int i = start; i < end; i++) {
        if (str[i] == '{')
            depth++;
        else if (str[i] == '}')
            depth--;
        else if (str[i] == ',' && depth == 0)
            return true;
    }
    return false;
}

/* Parse and expand a range {start..end} */
static Array *expand_range(char *str, int start, int end) {
    Array *result = init_array(NULL);

    // Find the ".."
    int dots = -1;
    for (int i = start; i < end - 1; i++) {
        if (str[i] == '.' && str[i + 1] == '.') {
            dots = i;
            break;
        }
    }

    if (dots == -1)
        return result;

    // Extract start and end values
    char *start_str = strndup(str + start, dots - start);
    char *end_str   = strndup(str + dots + 2, end - (dots + 2));

    // Trim whitespace
    while (*start_str && isspace(*start_str))
        start_str++;
    while (*end_str && isspace(*end_str))
        end_str++;

    // Check if numeric range
    bool  is_numeric = true;
    char *endptr;
    long  start_num = strtol(start_str, &endptr, 10);
    if (*endptr != '\0')
        is_numeric = false;

    long end_num = strtol(end_str, &endptr, 10);
    if (*endptr != '\0')
        is_numeric = false;

    if (is_numeric) {
        int len_of_start = strlen(start_str);
        int len_of_end   = strlen(end_str);
        int len_of_result =
            len_of_start < len_of_end ? len_of_end : len_of_start;

        // Numeric range
        int step = (start_num <= end_num) ? 1 : -1;
        for (long i = start_num; step > 0 ? i <= end_num : i >= end_num;
             i += step) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%0*ld", len_of_result, i);
            array_add(result, buf);
        }
    } else if (strlen(start_str) == 1 && strlen(end_str) == 1 &&
               isalpha(start_str[0]) && isalpha(end_str[0])) {
        // Character range
        char start_char = start_str[0];
        char end_char   = end_str[0];
        int  step       = (start_char <= end_char) ? 1 : -1;

        for (char c = start_char; step > 0 ? c <= end_char : c >= end_char;
             c += step) {
            char buf[2] = {c, '\0'};
            array_add(result, buf);
        }
    }

    free(start_str);
    free(end_str);
    return result;
}

/* Split brace content by commas at the current depth level */
static Array *split_by_comma(char *str, int start, int end) {
    Array *parts = init_array(NULL);

    Dynamic buffer = {0};
    init_dynamic(&buffer);
    int depth = 0;

    for (int i = start; i <= end; i++) {
        if (str[i] == '{') {
            depth++;
            dynamic_append(&buffer, str[i]);
        } else if (str[i] == '}') {
            depth--;
            dynamic_append(&buffer, str[i]);
        } else if (str[i] == ',' && depth == 0) {
            char *part = dynamic_to_string(&buffer);
            array_add(parts, part);
            free(part);
            init_dynamic(&buffer);
        } else {
            dynamic_append(&buffer, str[i]);
        }
    }

    char *part = dynamic_to_string(&buffer);
    array_add(parts, part);
    free(part);
    free_dynamic(&buffer);

    return parts;
}

/* Concatenate three strings */
static char *concat_strings(char *prefix, char *middle, char *suffix) {
    Dynamic result = {0};
    init_dynamic(&result);

    dynamic_extend(&result, (char *)prefix);
    dynamic_extend(&result, (char *)middle);
    dynamic_extend(&result, (char *)suffix);

    char *str = dynamic_to_string(&result);
    free_dynamic(&result);
    return str;
}

/* Recursive brace expansion */
static Array *expand_braces_recursive(char *str) {
    Array *results = init_array(NULL);

    int brace_start = -1;
    int brace_end   = -1;
    int search_pos  = 0;

    // Search for the first *expandable* brace pair
    while (str[search_pos]) {
        // Find next '{'
        char *open_ptr = strchr(str + search_pos, '{');
        if (!open_ptr)
            break;
        int current_start = open_ptr - str;

        // Find matching '}'
        int current_end = find_closing_brace(str, current_start + 1);
        if (current_end == -1)
            break; // No closing brace

        // Check if expandable
        if (is_range(str, current_start + 1, current_end) ||
            has_comma(str, current_start + 1, current_end)) {
            // Found one!
            brace_start = current_start;
            brace_end   = current_end;
            break;
        }

        // Not expandable, continue search after this block
        if (current_end > search_pos) {
            search_pos = current_end + 1;
        } else {
            // Should not happen if find_closing_brace is correct and advances
            search_pos++;
        }
    }

    if (brace_start == -1) {
        array_add(results, str);
        return results;
    }

    // Extract prefix and suffix
    char *prefix = strndup(str, brace_start);
    char *suffix = strdup(str + brace_end + 1);

    Array *alternatives;

    // Check if it's a range
    if (is_range(str, brace_start + 1, brace_end)) {
        alternatives = expand_range(str, brace_start + 1, brace_end);
    } else {
        // Split brace content by commas
        alternatives = split_by_comma(str, brace_start + 1, brace_end - 1);
    }

    // For each alternative, recursively expand
    for (size_t i = 0; i < alternatives->count; i++) {
        char *combined = concat_strings(prefix, alternatives->items[i], suffix);

        // Recursively expand the combined string
        Array *sub_results = expand_braces_recursive(combined);

        for (size_t j = 0; j < sub_results->count; j++) {
            array_add(results, sub_results->items[j]);
        }

        free_array(sub_results);
        free(sub_results);
        free(combined);
    }

    free_array(alternatives);
    free(alternatives);
    free(prefix);
    free(suffix);

    return results;
}

/* Main brace expansion function */
Array *brace_expansion(char *input, Session *session) {
    return expand_braces_recursive(input);
}
