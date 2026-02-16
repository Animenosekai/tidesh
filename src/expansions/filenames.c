/* Filename (glob) expansion implementation
 *
 * This module handles shell-style glob pattern expansion (wildcards like *, ?,
 * []) for filenames. It processes user input strings, identifies glob patterns,
 * and expands them to matching filenames in the filesystem.
 */

#include <ctype.h>    /* isspace */
#include <dirent.h>   /* DIR, dirent, opendir, readdir, closedir */
#include <glob.h>     /* glob, globfree, glob_t, GLOB_* */
#include <stdbool.h>  /* bool, true, false */
#include <stdio.h>    /* fprintf, stderr */
#include <stdlib.h>   /* malloc, free */
#include <sys/stat.h> /* stat, struct stat */

#include "data/array.h" /* Array, init_array, array_add, array_extend, free_array, array_sort */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_extend, free_dynamic, dynamic_to_string */
#include "environ.h"      /* Environ, environ_get */
#include "expansions/filenames.h" /* filename_expansion */
#include "session.h"              /* Session */

/* Check if string contains glob characters */
static bool has_glob_chars(char *str) {
    for (size_t i = 0; str[i]; i++) {
        if (str[i] == '*' || str[i] == '?' || str[i] == '[') {
            return true;
        }
    }
    return false;
}

/* Expand a glob pattern to matching filenames */
static Array *expand_glob(char *pattern) {
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

Array *filename_expansion(char *input, Session *session) {
    Array *results = init_array(NULL);

    if (!input)
        return results;

    if (has_glob_chars(input)) {
        // Expand glob pattern
        Array *expanded = expand_glob(input);
        array_extend(results, expanded);
        free_array(expanded);
        free(expanded);
    } else {
        // Add the word literally
        array_add(results, input);
    }

    return results;
}