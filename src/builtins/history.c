#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins/history.h"
#include "history.h" /* History, HistoryEntry */
#include "session.h" /* Session */

int builtin_history(int argc, char **argv, Session *session) {
    if (argc > 1) {
        if (strcmp(argv[1], "disable") == 0) {
            session->history->disabled = true;
            return 0;
        } else if (strcmp(argv[1], "enable") == 0) {
            session->history->disabled = false;
            return 0;
        } else if (strcmp(argv[1], "status") == 0) {
            printf("%s\n", session->history->disabled ? "disabled" : "enabled");
            return 0;
        } else if (strcmp(argv[1], "size") == 0) {
            printf("%zu\n", session->history->size);
            return 0;
        } else if (strcmp(argv[1], "clear") == 0) {
            history_clear(session->history);
            return 0;
        } else if (strcmp(argv[1], "limit") == 0) {
            if (argc > 2) {
                size_t new_limit = (size_t)strtoul(argv[2], NULL, 10);
                if (new_limit > 0) {
                    session->history->limit = new_limit;
                    // Prune history if it exceeds new limit
                    if (history_enforce_limit(session->history) > 0) {
                        history_save(session->history);
                    }
                }
            } else {
                printf("%zu\n", session->history->limit);
            }
            return 0;
        } else if (strcmp(argv[1], "file") == 0) {
            if (argc > 2) {
                if (session->history->filepath) {
                    free(session->history->filepath);
                }
                session->history->filepath = strdup(argv[2]);
            } else {
                printf("%s\n", session->history->filepath
                                   ? session->history->filepath
                                   : "");
            }
            return 0;
        } else {
            fprintf(stderr, "history: unknown subcommand: %s\n", argv[1]);
            fprintf(stderr,
                    "Usage: history [disable|enable|status|size|clear|limit "
                    "[num]|file [path]]\n");
            return 1;
        }
    }

    size_t        index = 1;
    HistoryEntry *entry = session->history->head;

    // Compute the maximum index width
    size_t max_index_width = 0;
    size_t size            = session->history->size;
    while (size > 0) {
        max_index_width++;
        size /= 10;
    }
    if (max_index_width == 0)
        max_index_width = 1;

    // Display history entries
    while (entry) {
        printf("%*zu  %s\n", (int)max_index_width, index++, entry->command);
        entry = entry->next;
    }
    return 0;
}