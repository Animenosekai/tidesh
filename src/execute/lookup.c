#include <ctype.h>   /* isspace */
#include <limits.h>  /* PATH_MAX */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* FILE, fopen, fread, fclose, fgets, snprintf */
#include <stdlib.h>  /* malloc, free, realloc, strdup */
#include <string.h>  /* strcmp, strchr, strlen, strncpy, strtok, memcpy */
#include <unistd.h>  /* access, X_OK */

#include "builtin.h"
#include "data/trie.h"
#include "environ.h"
#include "execute/lookup.h"
#include "session.h"

/* Trim leading and trailing whitespace from a string */
char *trim_whitespace(const char *str) {
    if (!str) {
        return NULL;
    }

    const char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (*start == '\0') {
        return strdup("");
    }

    const char *end = start;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }

    size_t len    = end - start;
    char  *result = malloc(len + 1);
    if (result) {
        strncpy(result, start, len);
        result[len] = '\0';
    }
    return result;
}

char *extract_first_word(const char *str) {
    if (!str) {
        return NULL;
    }

    const char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (*start == '\0') {
        return strdup("");
    }

    const char *end = start;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }

    size_t len  = end - start;
    char  *word = malloc(len + 1);
    if (!word) {
        return NULL;
    }
    memcpy(word, start, len);
    word[len] = '\0';
    return word;
}

bool has_shebang(const char *path) {
    if (!path) {
        return false;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }

    char   first_two[2];
    size_t read = fread(first_two, 1, 2, f);
    fclose(f);

    return (read == 2 && first_two[0] == '#' && first_two[1] == '!');
}

bool parse_shebang(const char *path, int *interp_argc, char ***interp_argv) {
    if (!path || !interp_argc || !interp_argv) {
        return false;
    }

    *interp_argc = 0;
    *interp_argv = NULL;

    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }

    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    fclose(f);

    if (line[0] != '#' || line[1] != '!') {
        return false;
    }

    char *p = line + 2;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    if (*p == '\0' || *p == '\n') {
        return false;
    }

    char *newline = strchr(p, '\n');
    if (newline) {
        *newline = '\0';
    }

    char **args = NULL;
    int    argc = 0;

    char *token = strtok(p, " \t");
    while (token) {
        args         = realloc(args, (argc + 1) * sizeof(char *));
        args[argc++] = strdup(token);
        token        = strtok(NULL, " \t");
    }

    if (argc == 0) {
        return false;
    }

    *interp_argc = argc;
    *interp_argv = args;
    return true;
}

char *find_in_path(const char *cmd, Session *session) {
    if (strchr(cmd, '/'))
        return strdup(cmd);

    char       *path_env = environ_get(session->environ, "PATH");
    static char buffer[PATH_MAX];
    if (path_env) {
        char *path_dup = strdup(path_env);
        char *dir      = strtok(path_dup, ":");
        while (dir) {
            snprintf(buffer, sizeof(buffer), "%s/%s", dir, cmd);
            if (access(buffer, X_OK) == 0) {
                free(path_dup);
                return strdup(buffer);
            }
            dir = strtok(NULL, ":");
        }
        free(path_dup);
    }

    const char *defaults[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};
    for (int i = 0; defaults[i]; i++) {
        snprintf(buffer, sizeof(buffer), "%s/%s", defaults[i], cmd);
        if (access(buffer, X_OK) == 0)
            return strdup(buffer);
    }
    return NULL;
}

CommandInfo get_command_info(const char *cmd, Session *session) {
    CommandInfo info = {COMMAND_NOT_FOUND, NULL};

#ifndef TIDESH_DISABLE_ALIASES
    if (session->features.alias_expansion) {
        char *alias_val = trie_get(session->aliases, (char *)cmd);
        if (alias_val) {
            info.type = COMMAND_ALIAS;
            info.path = strdup(alias_val);
            return info;
        }
    }
#endif

    if (is_special_builtin(cmd)) {
        info.type = COMMAND_SPECIAL_BUILTIN;
        info.path = NULL;
        return info;
    }

#ifndef TIDESH_DISABLE_FUNCTIONS
    if (session->features.functions &&
        trie_get(session->functions, (char *)cmd)) {
        info.type = COMMAND_FUNCTION;
        info.path = NULL;
        return info;
    }
#endif

    if (is_builtin(cmd)) {
        info.type = COMMAND_BUILTIN;
        info.path = NULL;
        return info;
    }

    char *path = find_in_path(cmd, session);
    if (path) {
        info.type = COMMAND_EXTERNAL;
        info.path = path;
        return info;
    }

    return info;
}
