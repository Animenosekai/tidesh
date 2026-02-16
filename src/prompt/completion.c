#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "builtin.h"
#include "data/array.h"
#include "data/dynamic.h"
#include "data/trie.h"
#include "environ.h"
#include "prompt/completion.h"
#include "prompt/cursor.h"

/* Check if a character is a word delimiter in shell */
bool is_shell_delimiter(char c) {
    return isspace(c) || c == '|' || c == '&' || c == ';' || c == '<' ||
           c == '>' || c == '(' || c == ')' || c == '\0';
}

/* Find the start of the word at the current position */
static size_t find_word_start(const char *data, size_t pos) {
    if (pos == 0)
        return 0;
    size_t i = pos;
    while (i > 0 && !is_shell_delimiter(data[i - 1])) {
        i--;
    }
    return i;
}

/* Find the common prefix of all strings in an array */
static char *find_common_prefix(Array *matches) {
    if (!matches || matches->count == 0)
        return NULL;
    if (matches->count == 1)
        return strdup(matches->items[0]);

    char  *first = matches->items[0];
    size_t len   = strlen(first);

    for (size_t i = 1; i < matches->count; i++) {
        char  *current = matches->items[i];
        size_t j       = 0;
        while (j < len && current[j] && first[j] == current[j]) {
            j++;
        }
        len = j;
    }

    char *prefix = malloc(len + 1);
    if (prefix) {
        strncpy(prefix, first, len);
        prefix[len] = '\0';
    }
    return prefix;
}

/* Match builtins */
static void match_builtins(const char *prefix, Array *matches) {
    size_t len = strlen(prefix);
    for (int i = 0; builtins[i]; i++) {
        if (strncmp(builtins[i], prefix, len) == 0) {
            array_add(matches, (char *)builtins[i]);
        }
    }
}

/* Match commands from history */
static void match_history(const char *prefix, Session *session,
                          Array *matches) {
    if (!session || !session->history || !prefix)
        return;

    HistoryEntry *curr       = session->history->tail;
    size_t        prefix_len = strlen(prefix);

    while (curr) {
        if (strncmp(curr->command, prefix, prefix_len) == 0) {
            // Check if already in matches to avoid duplicates
            bool exists = false;
            for (size_t i = 0; i < matches->count; i++) {
                if (strcmp(matches->items[i], curr->command) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                array_add(matches, curr->command);
            }
        }
        curr = curr->prev;
    }
}

/* Match aliases */
static void match_aliases(const char *prefix, Session *session,
                          Array *matches) {
    Array *alias_matches = trie_starting_with(session->aliases, (char *)prefix);
    if (alias_matches) {
        array_extend(matches, alias_matches);
        free_array(alias_matches);
        free(alias_matches);
    }
}

/* Match executables in PATH */
static void match_path(const char *prefix, Session *session, Array *matches) {
    if (session->path_commands) {
        // Do not perform the path update more than once in this function
        static bool path_updated = false;
        if (!path_updated) {
            update_path(session);
            path_updated = true;
        }

        Array *path_matches =
            trie_starting_with(session->path_commands, (char *)prefix);
        if (path_matches) {
            array_extend(matches, path_matches);
            free_array(path_matches);
            free(path_matches);
        }
    }
}

/* Match files and directories */
static void match_files(const char *prefix, Array *matches) {
    char *dir_path    = ".";
    char *file_prefix = (char *)prefix;
    char *last_slash  = strrchr(prefix, '/');

    char *temp_dir = NULL;
    if (last_slash) {
        temp_dir = strndup(prefix, last_slash - prefix);
        if (strlen(temp_dir) == 0)
            dir_path = "/";
        else
            dir_path = temp_dir;
        file_prefix = last_slash + 1;
    }

    DIR *dir = opendir(dir_path);
    if (dir) {
        size_t         prefix_len = strlen(file_prefix);
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip . and .. always unless prefix starts with .
            if (entry->d_name[0] == '.' && file_prefix[0] != '.') {
                continue;
            }
            if (strncmp(entry->d_name, file_prefix, prefix_len) != 0) {
                continue;
            }

            char *match_str;
            if (last_slash) {
                size_t d_len = strlen(dir_path);
                size_t e_len = strlen(entry->d_name);
                match_str    = malloc(d_len + 1 + e_len + 1);
                if (match_str) {
                    if (strcmp(dir_path, "/") == 0)
                        snprintf(match_str, d_len + 1 + e_len + 1, "/%s",
                                 entry->d_name);
                    else
                        snprintf(match_str, d_len + 1 + e_len + 1, "%s/%s",
                                 dir_path, entry->d_name);
                }
            } else {
                match_str = strdup(entry->d_name);
            }

            if (!match_str)
                continue;

            // Check if it's a directory to append /
            struct stat st;
            char        full_path[PATH_MAX];
            if (last_slash) {
                strcpy(full_path, match_str);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir_path,
                         entry->d_name);
            }

            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                size_t m_len = strlen(match_str);
                char  *temp  = realloc(match_str, m_len + 2);
                if (temp) {
                    match_str = temp;
                    strcat(match_str, "/");
                }
            }
            array_add(matches, match_str);
            free(match_str);
        }
        closedir(dir);
    }
    if (temp_dir)
        free(temp_dir);
}

void completion_apply(Cursor *cursor, Session *session) {
    char  *data = dynamic_to_string(cursor->data);
    size_t pos  = cursor->data->length - cursor->position;

    size_t start  = find_word_start(data, pos);
    char  *prefix = strndup(data + start, pos - start);

    Array *matches = init_array(NULL);

    // Determine if we are completing a command or a file
    bool   is_command = true;
    size_t i          = start;
    while (i > 0) {
        i--;
        if (!isspace(data[i])) {
            if (data[i] == '|' || data[i] == '&' || data[i] == ';' ||
                data[i] == '(') {
                is_command = true;
            } else {
                is_command = false;
            }
            break;
        }
    }
    if (start == 0)
        is_command = true;

    if (is_command && strchr(prefix, '/') == NULL) {
        if (strlen(prefix) > 0) {
            match_builtins(prefix, matches);
            match_aliases(prefix, session, matches);
            match_path(prefix, session, matches);
        }
    } else {
        match_files(prefix, matches);
    }

    if (matches->count == 0) {
        // Fallback to history completion if no matches were found
        free(prefix);
        start  = 0; // Match entire line up to cursor
        prefix = strndup(data, pos);
        match_history(prefix, session, matches);
    }

    if (matches->count > 0) {
        char *common = find_common_prefix(matches);
        if (common && strlen(common) > strlen(prefix)) {
            // Extend the current word
            cursor_insert(cursor, common + strlen(prefix));
        } else if (matches->count > 1) {
            // Multiple matches, show them?
            // Ring a bell also
            printf("\a");
            fflush(stdout);
        } else if (matches->count == 1) {
            // One match, if it's not a directory, maybe add a space
            char *match = matches->items[0];
            if (match[strlen(match) - 1] != '/') {
                cursor_insert(cursor, " ");
            }
        }
        if (common)
            free(common);
    } else {
        // Ring a bell
        printf("\a");
        fflush(stdout);
    }

    free_array(matches);
    free(matches);
    free(prefix);
    free(data);
}
