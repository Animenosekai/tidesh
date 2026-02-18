#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "builtins/test.h"
#include "session.h"

/* Helper function to check if a string is a valid integer */
static bool is_integer(const char *str) {
    if (!str || *str == '\0')
        return false;
    if (*str == '-' || *str == '+')
        str++;
    if (*str == '\0')
        return false;
    while (*str) {
        if (*str < '0' || *str > '9')
            return false;
        str++;
    }
    return true;
}

/* File test operators */
static int test_file(const char *op, const char *path) {
    struct stat st;

    if (strcmp(op, "-e") == 0 || strcmp(op, "-a") == 0) {
        return access(path, F_OK) == 0;
    } else if (strcmp(op, "-f") == 0) {
        return stat(path, &st) == 0 && S_ISREG(st.st_mode);
    } else if (strcmp(op, "-d") == 0) {
        return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
    } else if (strcmp(op, "-r") == 0) {
        return access(path, R_OK) == 0;
    } else if (strcmp(op, "-w") == 0) {
        return access(path, W_OK) == 0;
    } else if (strcmp(op, "-x") == 0) {
        return access(path, X_OK) == 0;
    } else if (strcmp(op, "-s") == 0) {
        return stat(path, &st) == 0 && st.st_size > 0;
    } else if (strcmp(op, "-L") == 0 || strcmp(op, "-h") == 0) {
        return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
    }
    return 0;
}

/* String comparison operators */
static int test_string(const char *s1, const char *op, const char *s2) {
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
        return strcmp(s1, s2) == 0;
    } else if (strcmp(op, "!=") == 0) {
        return strcmp(s1, s2) != 0;
    } else if (strcmp(op, "<") == 0) {
        return strcmp(s1, s2) < 0;
    } else if (strcmp(op, ">") == 0) {
        return strcmp(s1, s2) > 0;
    }
    return 0;
}

/* Numeric comparison operators */
static int test_numeric(const char *s1, const char *op, const char *s2) {
    long n1 = atol(s1);
    long n2 = atol(s2);

    if (strcmp(op, "-eq") == 0) {
        return n1 == n2;
    } else if (strcmp(op, "-ne") == 0) {
        return n1 != n2;
    } else if (strcmp(op, "-lt") == 0) {
        return n1 < n2;
    } else if (strcmp(op, "-le") == 0) {
        return n1 <= n2;
    } else if (strcmp(op, "-gt") == 0) {
        return n1 > n2;
    } else if (strcmp(op, "-ge") == 0) {
        return n1 >= n2;
    }
    return 0;
}

/* Main test evaluation */
static int evaluate_test(int argc, char **argv, int start) {
    int end = argc;

    /* Handle [ ... ] - need to check for closing ] */
    if (start > 0 && strcmp(argv[0], "[") == 0) {
        if (argc < 2 || strcmp(argv[argc - 1], "]") != 0) {
            fprintf(stderr, "[: missing `]'\n");
            return 2;
        }
        end = argc - 1;
    }

    int len = end - start;

    /* No arguments - false */
    if (len == 0)
        return 1;

    /* Single argument: -n string (string is non-empty) */
    if (len == 1) {
        return argv[start][0] != '\0' ? 0 : 1;
    }

    /* Two arguments: unary operators */
    if (len == 2) {
        const char *op  = argv[start];
        const char *arg = argv[start + 1];

        /* String tests */
        if (strcmp(op, "-n") == 0) {
            return arg[0] != '\0' ? 0 : 1;
        } else if (strcmp(op, "-z") == 0) {
            return arg[0] == '\0' ? 0 : 1;
        }
        /* File tests */
        else if (op[0] == '-' && op[2] == '\0') {
            return test_file(op, arg) ? 0 : 1;
        }
        /* Negation */
        else if (strcmp(op, "!") == 0) {
            return argv[start + 1][0] == '\0' ? 0 : 1;
        }
    }

    /* Three arguments: binary operators */
    if (len == 3) {
        const char *arg1 = argv[start];
        const char *op   = argv[start + 1];
        const char *arg2 = argv[start + 2];

        /* Negation */
        if (strcmp(arg1, "!") == 0) {
            int result = evaluate_test(argc, argv, start + 1);
            return result == 0 ? 1 : (result == 1 ? 0 : 2);
        }

        /* Numeric comparisons */
        if (op[0] == '-' &&
            (strcmp(op, "-eq") == 0 || strcmp(op, "-ne") == 0 ||
             strcmp(op, "-lt") == 0 || strcmp(op, "-le") == 0 ||
             strcmp(op, "-gt") == 0 || strcmp(op, "-ge") == 0)) {
            if (!is_integer(arg1) || !is_integer(arg2)) {
                fprintf(stderr, "test: integer expression expected\n");
                return 2;
            }
            return test_numeric(arg1, op, arg2) ? 0 : 1;
        }
        /* String comparisons */
        else {
            return test_string(arg1, op, arg2) ? 0 : 1;
        }
    }

    /* Four arguments: negation with binary operator */
    if (len == 4 && strcmp(argv[start], "!") == 0) {
        int result = evaluate_test(argc, argv, start + 1);
        return result == 0 ? 1 : (result == 1 ? 0 : 2);
    }

    /* For now, anything more complex returns false */
    fprintf(stderr, "test: too many arguments\n");
    return 2;
}

int builtin_test(int argc, char **argv, Session *session) {
    (void)session; /* Unused */

    /* Handle [ command - starts at index 0 */
    if (argc > 0 && strcmp(argv[0], "[") == 0) {
        return evaluate_test(argc, argv, 1);
    }

    /* Regular test command - starts at index 1 */
    return evaluate_test(argc, argv, 1);
}
