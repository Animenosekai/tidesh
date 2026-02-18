#include <stdlib.h> /* malloc, realloc, free, size_t */
#include <string.h> /* memcpy */

#include "data/files.h" /* read_all, FILE */

char *read_all(FILE *f) {
    size_t capacity = 1024;
    size_t size     = 0;
    char  *content  = malloc(capacity);
    if (!content)
        return NULL;

    char   buffer[1024];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (size + n + 1 > capacity) {
            capacity *= 2;
            char *new_content = realloc(content, capacity);
            if (!new_content) {
                free(content);
                return NULL;
            }
            content = new_content;
        }
        memcpy(content + size, buffer, n);
        size += n;
    }
    content[size] = '\0';
    return content;
}
