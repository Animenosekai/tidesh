#include <limits.h>  /* PATH_MAX */
#include <pwd.h>     /* getpwuid, getpwnam, struct passwd */
#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* snprintf, fprintf, stderr */
#include <stdlib.h>  /* malloc, free, realloc, strtol */
#include <string.h>  /* strncmp, strlen, strcpy, strncpy, memcpy */
#include <unistd.h>  /* getuid, getcwd, readlink */

#include "data/array.h" /* Array */
#include "environ.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#endif

typedef struct Environ {
    Array *array;
} Environ;

static char *get_executable_path() {
    static char path[PATH_MAX];

#if defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0)
        return NULL;
#else // Linux/Unix
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1)
        return NULL;
    path[len] = '\0';
#endif

    // Resolve the path to an absolute path
    char *resolved_path = realpath(path, NULL);
    if (resolved_path) {
        strncpy(path, resolved_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        free(resolved_path);
    }
    return path;
}

/* This is the original environment variables */
extern char **environ;

bool environ_contains(Environ *env, char *key) {
    if (!env || !env->array) // Check array
        return false;
    size_t keylen = strlen(key);
    // Loop to env->array->count
    for (size_t i = 0; i < env->array->count; i++) {
        // Access via env->array->items[i]
        if (!strncmp(env->array->items[i], key, keylen) &&
            env->array->items[i][keylen] == '=') {
            return true;
        }
    }
    return false;
}

char *environ_get(Environ *env, char *key) {
    size_t keylen = strlen(key);
    if (!env || !env->array || !env->array->items) // Check array and items
        return NULL;

    // Loop to env->array->count
    for (size_t i = 0; i < env->array->count; i++) {
        // Access via env->array->items[i]
        if (!strncmp(env->array->items[i], key, keylen) &&
            env->array->items[i][keylen] == '=') {
            return env->array->items[i] + keylen + 1; // return pointer to value
        }
    }
    return NULL; // not found
}

char *environ_get_default(Environ *env, char *key, char *default_value) {
    char *value = environ_get(env, key);
    return value ? value : default_value;
}

void environ_set(Environ *env, char *key, char *value) {
    if (!env || !env->array)
        return;

    size_t keylen = strlen(key);
    char  *newvar = malloc(keylen + 1 + strlen(value) + 1); // key=value\0
    if (!newvar) {
        fprintf(stderr, "environ_set: malloc failed\n");
        return;
    }

    // Build newvar string
    memcpy(newvar, key, keylen);
    newvar[keylen] = '=';
    strcpy(newvar + keylen + 1, value);

    // Key exists: replace
    for (size_t i = 0; i < env->array->count; i++) {
        if (!strncmp(env->array->items[i], key, keylen) &&
            env->array->items[i][keylen] == '=') {
            array_set(env->array, i, newvar, true); // true = free old
            free(newvar);
            return;
        }
    }

    // Not found: append using array_add
    array_add(env->array, newvar);
    free(newvar);
}

bool environ_remove(Environ *env, char *key) {
    if (!env || !env->array)
        return false;

    size_t keylen = strlen(key);
    for (size_t i = 0; i < env->array->count; i++) {
        if (!strncmp(env->array->items[i], key, keylen) &&
            env->array->items[i][keylen] == '=') {
            array_remove(env->array, i);
            return true;
        }
    }
    return false;
}

void environ_set_exit_status(Environ *env, int status) {
    char status_str[12];
    snprintf(status_str, sizeof(status_str), "%d", status);
    environ_set(env, "?", status_str);
}

void environ_set_background_pid(Environ *env, pid_t pid) {
    char pid_str[20];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    environ_set(env, "!", pid_str);
}

void environ_set_last_arg(Environ *env, const char *arg) {
    if (arg) {
        environ_set(env, "_", (char *)arg);
    } else {
        environ_set(env, "_", "");
    }
}

Environ *init_environ(Environ *env) {
    if (!env) {
        env = malloc(sizeof(Environ));
        if (!env) {
            return NULL;
        }
    }

    // Initialize the array
    env->array = init_array(NULL);
    if (!env->array) {
        free(env);
        return NULL;
    }

    // Copy variables from global environ
    for (size_t i = 0; environ[i]; i++) {
        if (!array_add(env->array, environ[i])) {
            // On failure, free everything allocated so far
            free_array(env->array);
            free(env->array);
            free(env);
            return NULL;
        }
    }

    // Set SHELL
    char *shell = get_executable_path();
    if (shell) {
        environ_set(env, "SHELL", shell);
    }

#ifdef PROJECT_NAME
    // Set SHELL_NAME
    environ_set(env, "SHELL_NAME", PROJECT_NAME);
#endif

    // Update SHLVL (shell level)
    char *endptr = NULL;
    // Default to 0, parse, then increment
    long shlvl = strtol(environ_get_default(env, "SHLVL", "0"), &endptr, 10);
    if (*endptr != '\0') {
        shlvl = 0; // If parse fails, treat as 0
    }
    shlvl++; // Increment shell level

    // Convert back to string
    int   length = snprintf(NULL, 0, "%ld", shlvl);
    char *str    = malloc(length + 1);
    if (str) { // Check malloc
        snprintf(str, length + 1, "%ld", shlvl);
        environ_set(env, "SHLVL", str); // set new SHLVL
        free(str);
    }

    // Set HOME if not set
    if (!environ_contains(env, "HOME")) {
        struct passwd *pw   = getpwuid(getuid());
        char          *home = pw ? pw->pw_dir : "/";
        environ_set(env, "HOME", home);
    }

    // Set PWD if not set
    if (!environ_contains(env, "PWD")) {
        char cwd[PATH_MAX]; // Use PATH_MAX
        if (getcwd(cwd, sizeof(cwd))) {
            environ_set(env, "PWD", cwd);
        }
    }

    // Set OLDPWD if not set
    if (!environ_contains(env, "OLDPWD")) {
        environ_set(env, "OLDPWD", environ_get_default(env, "PWD", "/"));
    }

#ifdef PROJECT_NAME
    // Set TIDESH_NAME
    environ_set(env, "TIDESH_NAME", PROJECT_NAME);
#endif

#ifdef VERSION
    // Set TIDESH_VERSION
    environ_set(env, "TIDESH_VERSION", VERSION);
#endif

#ifdef RAW_VERSION
    // Set TIDESH_RAW_VERSION
    environ_set(env, "TIDESH_RAW_VERSION", RAW_VERSION);
#endif

#ifdef GIT_VERSION
    // Set TIDESH_GIT_VERSION
    environ_set(env, "TIDESH_GIT_VERSION", GIT_VERSION);
#endif

#ifdef BUILD_DATE
    // Set TIDESH_BUILD_DATE
    environ_set(env, "TIDESH_BUILD_DATE", BUILD_DATE);
#endif

#ifdef PLATFORM
    // Set TIDESH_PLATFORM
    environ_set(env, "TIDESH_PLATFORM", PLATFORM);
#endif

#ifdef __VERSION__
    // Set TIDESH_COMPILER
    environ_set(env, "TIDESH_COMPILER", __VERSION__);
#endif

#ifdef NDEBUG
    // Set TIDESH_BUILD_TYPE
    environ_set(env, "TIDESH_BUILD_TYPE", "release");
#else
    environ_set(env, "TIDESH_BUILD_TYPE", "debug");
#endif

    // Set TIDESH_PID
    char pid_str[20];
    snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    environ_set(env, "TIDESH_PID", pid_str);
    environ_set(env, "$", pid_str);

    // Set exit status
    environ_set_exit_status(env, 0);

    // Set background PID
    environ_set(env, "!", "");

    // Set last argument
    environ_set_last_arg(env, shell);

    // Set TIDESH_PPID
    char ppid_str[20];
    snprintf(ppid_str, sizeof(ppid_str), "%d", getppid());
    environ_set(env, "TIDESH_PPID", ppid_str);

    // Set TIDESH_EXECUTABLE
    if (shell) {
        environ_set(env, "TIDESH_EXECUTABLE", shell);
    }

    return env;
}

Environ *environ_copy(Environ *src, Environ *dest) {
    if (!src)
        return NULL;

    if (!dest) {
        dest = malloc(sizeof(Environ));
        if (!dest) {
            return NULL;
        }
    } else {
        free_environ(dest);
    }
    if (!dest) {
        return NULL;
    }

    // Initialize its array
    dest->array = array_copy(src->array, NULL);
    if (!dest->array) {
        free(dest);
        return NULL;
    }

    return dest;
}

void free_environ(Environ *env) {
    if (!env)
        return;

    // free_array should handle freeing items[] and all items[i] strings
    if (env->array) {
        free_array(env->array);
        free(env->array);
        env->array = NULL;
    }
}

Array *environ_to_array(Environ *env) {
    if (!env)
        return NULL;
    return array_copy(env->array, NULL);
}
