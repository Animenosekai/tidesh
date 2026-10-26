/* Filename (glob) expansion implementation
 *
 * This module handles shell-style glob pattern expansion (wildcards like *, ?,
 * [], and recursive ** globbing) for filenames. It processes user input
 * strings, identifies glob patterns, and expands them to matching filenames in
 * the filesystem.
 */

#include <ctype.h>    /* isspace */
#include <dirent.h>   /* DIR, dirent, opendir, readdir, closedir */
#include <fnmatch.h>  /* fnmatch, FNM_PERIOD */
#include <glob.h>     /* glob, globfree, glob_t, GLOB_* */
#include <limits.h>   /* PATH_MAX */
#include <stdbool.h>  /* bool, true, false */
#include <stdio.h>    /* fprintf, stderr, snprintf */
#include <stdlib.h>   /* malloc, free, realloc */
#include <string.h>   /* strcmp, strdup, strlen, strstr, strchr, strtok_r */
#include <sys/stat.h> /* stat, lstat, struct stat, S_ISDIR */

#include "data/array.h" /* Array, init_array, array_add, array_extend, free_array, array_sort */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, free_dynamic, dynamic_to_string */
#include "environ.h"      /* Environ, environ_get */
#include "expansions/filenames.h" /* filename_expansion */
#include "session.h"              /* Session */

/* Check if string contains glob characters */
static bool has_glob_chars(const char *str) {
    if (!str) {
        return false;
    }
    for (size_t i = 0; str[i]; i++) {
        if (str[i] == '*' || str[i] == '?' || str[i] == '[') {
            return true;
        }
    }
    return false;
}

/* Check if string contains recursive wildcard ** */
static bool has_doublestar(const char *str) {
    return (str && strstr(str, "**") != NULL);
}

/* Check if segment contains glob wildcard characters */
static bool is_wildcard_segment(const char *s) {
    return (strchr(s, '*') != NULL || strchr(s, '?') != NULL ||
            strchr(s, '[') != NULL);
}

/* Remove duplicate adjacent items from a sorted array */
static void array_dedup(Array *array) {
    if (!array || array->count <= 1) {
        return;
    }
    size_t write_idx = 1;
    for (size_t i = 1; i < array->count; i++) {
        if (strcmp(array->items[i], array->items[write_idx - 1]) != 0) {
            array->items[write_idx++] = array->items[i];
        } else {
            free(array->items[i]);
        }
    }
    array->count = write_idx;
}

/* Concatenate directory and filename into an allocated path */
static char *join_path(const char *dir, const char *name) {
    if (!dir || dir[0] == '\0' || strcmp(dir, ".") == 0) {
        return strdup(name);
    }
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    char  *res  = malloc(dlen + nlen + 2);
    if (!res) {
        return NULL;
    }
    if (dir[dlen - 1] == '/') {
        snprintf(res, dlen + nlen + 2, "%s%s", dir, name);
    } else {
        snprintf(res, dlen + nlen + 2, "%s/%s", dir, name);
    }
    return res;
}

/* Expand a glob pattern using the system glob() implementation */
static Array *expand_glob_system(char *pattern) {
    Array *results = init_array(NULL);

    glob_t globbuf;
    int    flags = GLOB_NOSORT | GLOB_MARK;

    // Use glob() for expansion
    int ret = glob(pattern, flags, NULL, &globbuf);

    if (ret == 0) {
        // Success - add all matches
        for (size_t i = 0; i < globbuf.gl_pathc; i++) {
            array_add(results, globbuf.gl_pathv[i]);
        }
        array_sort(results);
    } else {
        // No matches or error - return pattern as-is
        array_add(results, pattern);
    }

    globfree(&globbuf);
    return results;
}

/* Recursively match path segments in directory hierarchy */
static void match_segments_recursive(const char *dir_path,
                                     const char *result_prefix, char **segments,
                                     size_t seg_idx, size_t seg_count,
                                     bool trailing_slash, Array *results,
                                     int depth) {
    if (depth > 128) {
        return;
    }

    if (strcmp(segments[seg_idx], "**") == 0) {
        bool is_last = (seg_idx == seg_count - 1);
        if (is_last) {
            // ** at the end of pattern
            // Zero directories: if result_prefix is non-empty, current dir
            // matches
            if (result_prefix && result_prefix[0] != '\0' &&
                strcmp(result_prefix, "./") != 0 &&
                strcmp(result_prefix, "../") != 0) {
                array_add(results, (char *)result_prefix);
            }

            DIR *dir = opendir(dir_path);
            if (!dir) {
                return;
            }
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                if (entry->d_name[0] == '.') {
                    continue;
                }

                char *full_path = join_path(dir_path, entry->d_name);
                if (!full_path) {
                    continue;
                }
                struct stat st;
                if (lstat(full_path, &st) != 0) {
                    free(full_path);
                    continue;
                }

                if (S_ISDIR(st.st_mode)) {
                    size_t plen =
                        strlen(result_prefix) + strlen(entry->d_name) + 2;
                    char *dir_res = malloc(plen);
                    if (dir_res) {
                        snprintf(dir_res, plen, "%s%s/", result_prefix,
                                 entry->d_name);
                        array_add(results, dir_res);
                        match_segments_recursive(
                            full_path, dir_res, segments, seg_idx, seg_count,
                            trailing_slash, results, depth + 1);
                        free(dir_res);
                    }
                } else if (!trailing_slash) {
                    size_t plen =
                        strlen(result_prefix) + strlen(entry->d_name) + 1;
                    char *file_res = malloc(plen);
                    if (file_res) {
                        snprintf(file_res, plen, "%s%s", result_prefix,
                                 entry->d_name);
                        array_add(results, file_res);
                        free(file_res);
                    }
                }
                free(full_path);
            }
            closedir(dir);
            return;
        }

        // ** followed by more segments
        // Option 1: ** matches 0 directories -> advance to seg_idx + 1 in
        // current directory
        match_segments_recursive(dir_path, result_prefix, segments, seg_idx + 1,
                                 seg_count, trailing_slash, results, depth + 1);

        // Option 2: ** matches 1 or more directories -> recurse into child
        // subdirectories
        DIR *dir = opendir(dir_path);
        if (!dir) {
            return;
        }
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            if (entry->d_name[0] == '.') {
                continue;
            }

            char *full_path = join_path(dir_path, entry->d_name);
            if (!full_path) {
                continue;
            }
            struct stat st;
            if (lstat(full_path, &st) != 0) {
                free(full_path);
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                size_t plen = strlen(result_prefix) + strlen(entry->d_name) + 2;
                char  *new_prefix = malloc(plen);
                if (new_prefix) {
                    snprintf(new_prefix, plen, "%s%s/", result_prefix,
                             entry->d_name);
                    match_segments_recursive(full_path, new_prefix, segments,
                                             seg_idx, seg_count, trailing_slash,
                                             results, depth + 1);
                    free(new_prefix);
                }
            }
            free(full_path);
        }
        closedir(dir);
        return;
    }

    // Normal segment
    bool is_last = (seg_idx == seg_count - 1);
    DIR *dir     = opendir(dir_path);
    if (!dir) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (entry->d_name[0] == '.' && segments[seg_idx][0] != '.') {
            continue;
        }

        if (fnmatch(segments[seg_idx], entry->d_name, FNM_PERIOD) == 0) {
            char *full_path = join_path(dir_path, entry->d_name);
            if (!full_path) {
                continue;
            }
            struct stat st;
            if (stat(full_path, &st) == 0) {
                bool is_dir = S_ISDIR(st.st_mode);
                if (is_last) {
                    if (trailing_slash && !is_dir) {
                        free(full_path);
                        continue;
                    }
                    size_t plen = strlen(result_prefix) +
                                  strlen(entry->d_name) + (is_dir ? 2 : 1);
                    char  *res  = malloc(plen);
                    if (res) {
                        snprintf(res, plen, "%s%s%s", result_prefix,
                                 entry->d_name, is_dir ? "/" : "");
                        array_add(results, res);
                        free(res);
                    }
                } else if (is_dir) {
                    size_t plen =
                        strlen(result_prefix) + strlen(entry->d_name) + 2;
                    char *new_prefix = malloc(plen);
                    if (new_prefix) {
                        snprintf(new_prefix, plen, "%s%s/", result_prefix,
                                 entry->d_name);
                        match_segments_recursive(
                            full_path, new_prefix, segments, seg_idx + 1,
                            seg_count, trailing_slash, results, depth + 1);
                        free(new_prefix);
                    }
                }
            }
            free(full_path);
        }
    }
    closedir(dir);
}

/* Expand a recursive glob pattern supporting ** */
static Array *expand_glob_recursive(const char *pattern) {
    Array *results = init_array(NULL);
    if (!pattern || !*pattern) {
        return results;
    }

    bool   is_absolute    = (pattern[0] == '/');
    size_t pat_len        = strlen(pattern);
    bool   trailing_slash = (pat_len > 0 && pattern[pat_len - 1] == '/');

    // Tokenize segments
    char  *copy      = strdup(pattern);
    char **segments  = malloc(sizeof(char *) * (pat_len + 1));
    size_t seg_count = 0;

    if (!copy || !segments) {
        free(copy);
        free(segments);
        array_add(results, (char *)pattern);
        return results;
    }

    char *saveptr = NULL;
    char *tok     = strtok_r(copy, "/", &saveptr);
    while (tok) {
        if (strcmp(tok, "**") == 0 && seg_count > 0 &&
            strcmp(segments[seg_count - 1], "**") == 0) {
            // Collapse adjacent **
            tok = strtok_r(NULL, "/", &saveptr);
            continue;
        }
        segments[seg_count++] = strdup(tok);
        tok                   = strtok_r(NULL, "/", &saveptr);
    }
    free(copy);

    if (seg_count == 0) {
        free(segments);
        if (is_absolute) {
            array_add(results, "/");
        } else {
            array_add(results, (char *)pattern);
        }
        return results;
    }

    // Determine initial directory and result prefix
    char   initial_dir[PATH_MAX]    = {0};
    char   initial_prefix[PATH_MAX] = {0};
    size_t start_seg                = 0;

    if (is_absolute) {
        strcpy(initial_dir, "/");
        strcpy(initial_prefix, "/");
    }

    while (start_seg < seg_count && !is_wildcard_segment(segments[start_seg])) {
        if (strcmp(segments[start_seg], ".") == 0) {
            if (initial_prefix[0] == '\0') {
                strcpy(initial_dir, ".");
                strcpy(initial_prefix, "./");
            }
        } else if (strcmp(segments[start_seg], "..") == 0) {
            if (initial_prefix[0] == '\0') {
                strcpy(initial_dir, "..");
                strcpy(initial_prefix, "../");
            } else {
                strncat(initial_dir, "/..",
                        sizeof(initial_dir) - strlen(initial_dir) - 1);
                strncat(initial_prefix, "../",
                        sizeof(initial_prefix) - strlen(initial_prefix) - 1);
            }
        } else {
            if (initial_dir[0] == '\0' || strcmp(initial_dir, ".") == 0) {
                strncpy(initial_dir, segments[start_seg],
                        sizeof(initial_dir) - 1);
            } else if (strcmp(initial_dir, "/") == 0) {
                snprintf(initial_dir, sizeof(initial_dir), "/%s",
                         segments[start_seg]);
            } else {
                strncat(initial_dir, "/",
                        sizeof(initial_dir) - strlen(initial_dir) - 1);
                strncat(initial_dir, segments[start_seg],
                        sizeof(initial_dir) - strlen(initial_dir) - 1);
            }
            strncat(initial_prefix, segments[start_seg],
                    sizeof(initial_prefix) - strlen(initial_prefix) - 1);
            strncat(initial_prefix, "/",
                    sizeof(initial_prefix) - strlen(initial_prefix) - 1);
        }
        start_seg++;
    }

    if (initial_dir[0] == '\0') {
        strcpy(initial_dir, ".");
    }

    if (start_seg < seg_count) {
        struct stat st;
        if (stat(initial_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            match_segments_recursive(initial_dir, initial_prefix, segments,
                                     start_seg, seg_count, trailing_slash,
                                     results, 0);
        }
    }

    for (size_t i = 0; i < seg_count; i++) {
        free(segments[i]);
    }
    free(segments);

    if (results->count > 0) {
        array_sort(results);
        array_dedup(results);
    } else {
        array_add(results, (char *)pattern);
    }
    return results;
}

/* Expand a glob pattern using the extended glob implementation (supports **) */
static Array *expand_glob_extended(char *pattern) {
    if (has_doublestar(pattern)) {
        return expand_glob_recursive(pattern);
    }
    return expand_glob_system(pattern);
}

Array *filename_expansion(char *input, Session *session) {
    Array *results = init_array(NULL);

    if (!input) {
        return results;
    }

    if (has_glob_chars(input)) {
        bool use_default_glob =
            (session != NULL && session->options.default_glob);
        Array *expanded = NULL;

        if (use_default_glob) {
            expanded = expand_glob_system(input);
        } else {
            expanded = expand_glob_extended(input);
        }

        array_extend(results, expanded);
        free_array(expanded);
        free(expanded);
    } else {
        // Add the word literally
        array_add(results, input);
    }

    return results;
}
