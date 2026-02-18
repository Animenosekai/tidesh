#include <stdbool.h> /* bool */
#include <stdio.h>   /* FILE, fopen, fclose, fprintf, getline */
#include <stdlib.h>  /* malloc, free, realloc, strtol */
#include <string.h>  /* strdup, strlen, strcat, strchr, strncmp */
#include <time.h>    /* time */

#include "history.h"       /* History, HistoryEntry */
#include "prompt/cursor.h" /* visible_length */

#ifndef TIDESH_DISABLE_HISTORY

#define DEFAULT_HISTORY_LIMIT 1000

/* Resets the history navigation pointer to the bottom (NULL) */
void history_reset_state(History *history) {
    if (history) {
        history->current = NULL;
    }
}

/* Frees a single history entry and its contents */
static void free_history_entry(HistoryEntry *entry) {
    if (entry) {
        if (entry->command) {
            free(entry->command);
            entry->command = NULL;
        }
    }
}

/* Convert `\n` to `\\n` for storage */
static char *escape_newlines(const char *command) {
    if (!command)
        return NULL;

    size_t len     = strlen(command);
    size_t new_len = len;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '\n') {
            new_len++;
        }
    }

    char *escaped = malloc(new_len + 1);
    if (!escaped)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '\n') {
            escaped[j++] = '\\';
            escaped[j++] = 'n';
        } else {
            escaped[j++] = command[i];
        }
    }
    escaped[j] = '\0';
    return escaped;
}

/* Reads a single entry from file handling multi-line escapes */
static HistoryEntry *read_entry(FILE *file) {
    char   *full_line = NULL;
    char   *line      = NULL;
    size_t  len       = 0;
    size_t  total_len = 0;
    ssize_t read;

    // Read logical line (handling backslash continuation)
    // (should already be escaped but just to be sure)
    while ((read = getline(&line, &len, file)) != -1) {
        // Strip newline from getline result
        if (read > 0 && line[read - 1] == '\n') {
            line[--read] = '\0';
        }

        // Simple concat
        char *temp = realloc(full_line, total_len + read + 1);
        if (!temp) {
            free(full_line);
            free(line);
            return NULL;
        }
        full_line = temp;
        if (total_len == 0)
            full_line[0] = '\0';

        strcat(full_line, line);
        total_len += read;
        break;
    }

    if (line)
        free(line);
    if (!full_line)
        return NULL;

    // Parse CSV: timestamp,command
    char *comma = strchr(full_line, ',');
    if (!comma) {
        free(full_line);
        return NULL;
    }

    *comma            = '\0'; // Split string
    long  timestamp   = strtol(full_line, NULL, 10);
    char *escaped_cmd = comma + 1;

    // Unescape command
    char *command = strdup(escaped_cmd);
    char *src     = command;
    char *dst     = command;
    while (*src) {
        if (src[0] == '\\' && src[1] == 'n') {
            *dst++ = '\n';
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    free(full_line);

    HistoryEntry *entry = malloc(sizeof(HistoryEntry));
    entry->command      = command;
    entry->timestamp    = timestamp;
    entry->next         = NULL;
    entry->prev         = NULL;

    return entry;
}

History *init_history(History *history) {
    if (!history) {
        history = malloc(sizeof(History));
        if (!history)
            return NULL;
    }
    history->head          = NULL;
    history->tail          = NULL;
    history->current       = NULL;
    history->size          = 0;
    history->limit         = DEFAULT_HISTORY_LIMIT;
    history->disabled      = false;
    history->filepath      = NULL;
    history->owns_filepath = false;
    return history;
}

History *load_history(History *history, char *filepath) {
    history = init_history(history);
    if (!history)
        return NULL;

    if (filepath) {
        history->filepath      = strdup(filepath);
        history->owns_filepath = history->filepath != NULL;
    }

    FILE *file = fopen(filepath, "r");
    if (!file)
        return history;

    HistoryEntry *entry;
    while ((entry = read_entry(file)) != NULL) {
        if (history->tail) {
            history->tail->next = entry;
            entry->prev         = history->tail;
            history->tail       = entry;
        } else {
            history->head = entry;
            history->tail = entry;
        }
        history->size++;
    }

    fclose(file);
    history_reset_state(history);
    return history;
}

void history_save(History *history) {
    if (!history || !history->filepath)
        return;

    FILE *file = fopen(history->filepath, "w");
    if (!file)
        return;

    HistoryEntry *curr = history->head;
    while (curr) {
        char *escaped = escape_newlines(curr->command);
        if (escaped) {
            fprintf(file, "%ld,%s\n", curr->timestamp, escaped);
            free(escaped);
        }
        curr = curr->next;
    }
    fclose(file);
}

void free_history(History *history) {
    if (!history)
        return;

    HistoryEntry *curr = history->head;
    while (curr) {
        HistoryEntry *next = curr->next;
        free_history_entry(curr);
        free(curr);
        curr = next;
    }
    history->head    = NULL;
    history->tail    = NULL;
    history->current = NULL;
    history->size    = 0;

    if (history->filepath && history->owns_filepath) {
        free(history->filepath);
        history->filepath = NULL;
    }
    history->owns_filepath = false;
}

void history_clear(History *history) {
    if (!history || history->disabled)
        return;

    // Save state we want to keep
    char  *path     = history->filepath ? strdup(history->filepath) : NULL;
    size_t limit    = history->limit;
    bool   disabled = history->disabled;

    free_history(history); // Clears entries and path

    // Restore state
    history->head          = NULL;
    history->tail          = NULL;
    history->current       = NULL;
    history->size          = 0;
    history->filepath      = path;
    history->owns_filepath = path != NULL;
    history->limit         = limit;
    history->disabled      = disabled;

    // Truncate file
    if (history->filepath) {
        FILE *f = fopen(history->filepath, "w");
        if (f)
            fclose(f);
    }
}

bool history_remove(History *history, const char *command, bool all) {
    if (!history || !command || history->disabled)
        return false;

    HistoryEntry *curr        = history->head;
    bool          removed_any = false;
    while (curr) {
        if (strcmp(curr->command, command) == 0) {
            if (curr->prev)
                curr->prev->next = curr->next;
            else
                history->head = curr->next;

            if (curr->next)
                curr->next->prev = curr->prev;
            else
                history->tail = curr->prev;

            HistoryEntry *to_free = curr;
            curr                  = curr->next;
            free_history_entry(to_free);
            free(to_free);
            history->size--;
            if (!all)
                return true;
            removed_any = true;
        } else {
            curr = curr->next;
        }
    }
    return removed_any;
}

size_t history_enforce_limit(History *history) {
    if (!history)
        return 0;

    size_t removed = 0;
    while (history->size > history->limit && history->head) {
        HistoryEntry *old = history->head;
        history->head     = old->next;
        if (history->head)
            history->head->prev = NULL;
        else
            history->tail = NULL;

        free_history_entry(old);
        free(old);
        history->size--;
        removed++;
    }
    return removed;
}

void history_append(History *history, const char *command) {
    if (!history || history->disabled || !command ||
        visible_length(command) == 0)
        return;

    // Append new
    HistoryEntry *entry = malloc(sizeof(HistoryEntry));
    entry->command      = strdup(command);
    entry->timestamp    = (long)time(NULL);
    entry->next         = NULL;
    entry->prev         = history->tail;

    if (history->tail) {
        history->tail->next = entry;
        history->tail       = entry;
    } else {
        history->head = entry;
        history->tail = entry;
    }
    history->size++;

    // Enforce Limit
    bool pruned = history_enforce_limit(history) > 0;

    // Reset navigation
    history_reset_state(history);

    // Append to file immediately
    if (history->filepath) {
        if (pruned) {
            history_save(history);
            return;
        }
        FILE *file = fopen(history->filepath, "a");
        if (file) {
            char *escaped = escape_newlines(command);
            if (escaped) {
                fprintf(file, "%ld,%s\n", entry->timestamp, escaped);
                free(escaped);
            }
            fclose(file);
        }
    }
}

char *history_get_previous(History *history) {
    if (!history || !history->tail)
        return NULL;

    if (history->current == NULL) {
        // From prompt -> last history entry
        history->current = history->tail;
    } else if (history->current->prev) {
        // Move up
        history->current = history->current->prev;
    }
    // else: already at head

    return history->current ? history->current->command : NULL;
}

char *history_get_next(History *history) {
    if (!history)
        return NULL;

    if (history->current == NULL) {
        // Already at bottom
        return NULL;
    }

    if (history->current->next) {
        history->current = history->current->next;
        return history->current->command;
    } else {
        // Fall off the end -> back to prompt
        history->current = NULL;
        return NULL;
    }
}

char *history_nth_last_command(History *history, size_t n) {
    if (!history || n == 0 || n > history->size) {
        return NULL;
    }

    HistoryEntry *current = history->tail;
    for (size_t i = 1; i < n; i++) {
        if (current->prev) {
            current = current->prev;
        } else {
            return NULL;
        }
    }
    return current->command;
}

char *history_nth_command(History *history, size_t n) {
    if (!history || n == 0 || n > history->size) {
        return NULL;
    }

    HistoryEntry *current = history->head;
    for (size_t i = 1; i < n; i++) {
        if (current->next) {
            current = current->next;
        } else {
            return NULL;
        }
    }
    return current->command;
}

char *history_last_command(History *history) {
    if (!history || !history->tail) {
        return NULL;
    }
    return history->tail->command;
}

char *history_last_command_starting_with(History *history, char *prefix) {
    if (!history || !prefix) {
        return NULL;
    }

    HistoryEntry *current    = history->tail;
    size_t        prefix_len = strlen(prefix);

    while (current) {
        if (strncmp(current->command, prefix, prefix_len) == 0) {
            return current->command;
        }
        current = current->prev;
    }
    return NULL;
}

#endif /* TIDESH_DISABLE_HISTORY */
