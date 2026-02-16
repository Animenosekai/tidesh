#include "data/utf8.h"

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

/* Get UTF-8 character length from first byte */
unsigned char utf8_charlen(char c) {
    if ((c & 0b10000000) == 0)
        return 1;
    if ((c & 0b11100000) == 0b11000000)
        return 2;
    if ((c & 0b11110000) == 0b11100000)
        return 3;
    if ((c & 0b11111000) == 0b11110000)
        return 4;
    return 1; // Invalid, treat as single byte
}

size_t utf8_strlen(char *str) {
    if (!str)
        return 0;

    size_t len = 0;
    while (*str) {
        unsigned char char_len = utf8_charlen((char)*str);
        str += char_len;
        len++;
    }
    return len;
}

char *utf8_next_char(char *str) {
    if (!str || *str == '\0')
        return str;

    unsigned char char_len = utf8_charlen((char)*str);
    return str + char_len;
}

char *utf8_prev_char(char *current, char *start) {
    if (!start || !current || current <= start)
        return NULL; // Can't move back from the start

    char *p = current - 1;

    // Skip backwards over UTF-8 continuation bytes (10xxxxxx)
    while (p > start && ((unsigned char)(*p) & 0xC0) == 0x80) {
        p--;
    }

    return p;
}