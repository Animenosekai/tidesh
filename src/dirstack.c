#include "dirstack.h" /* DirStack  */
#include <errno.h>    /* errno */
#include <limits.h>   /* PATH_MAX */
#include <stdio.h>    /* fprintf, stderr, perror */
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* strerror */
#include <unistd.h>   /* getcwd, chdir */

#include "data/array.h" /* init_array, free_array, array_insert, array_pop, array_set */

#ifndef TIDESH_DISABLE_DIRSTACK

#include "dirstack.h" /* DirStack */

DirStack *init_dirstack(DirStack *directory_stack) {
    if (!directory_stack) {
        directory_stack = malloc(sizeof(DirStack));
        if (!directory_stack) {
            return NULL;
        }
    }
    directory_stack->stack = init_array(NULL);
    return directory_stack;
}

void free_dirstack(DirStack *directory_stack) {
    if (directory_stack->stack) {
        free_array(directory_stack->stack);
        free(directory_stack->stack);
    }
}

bool dirstack_pushd(DirStack *directory_stack, const char *path) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) {
        perror("getcwd");
        return false;
    }

    if (chdir(path) != 0) {
        fprintf(stderr, "pushd: chdir(%s): %s\n", path, strerror(errno));
        return false;
    }

    /* Insert previous cwd at index 0 */
    array_insert(directory_stack->stack, 0, cwd);
    return true;
}

bool dirstack_popd(DirStack *directory_stack) {
    if (directory_stack->stack->count == 0) {
        fprintf(stderr, "popd: directory stack empty\n");
        return false;
    }

    char *target = array_pop(directory_stack->stack, 0);

    if (chdir(target) != 0) {
        fprintf(stderr, "popd: chdir(%s): %s\n", target, strerror(errno));
        free(target);
        return false;
    }

    free(target);
    return true;
}

bool dirstack_swap(DirStack *directory_stack, size_t n) {
    if (n == 0)
        return true;

    if (n > directory_stack->stack->count) {
        fprintf(stderr, "pushd: +%zu: no such entry\n", n);
        return false;
    }

    size_t idx = n - 1;

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) {
        perror("getcwd");
        return false;
    }

    const char *target = directory_stack->stack->items[idx];

    if (chdir(target) != 0) {
        fprintf(stderr, "pushd: chdir(%s): %s\n", target, strerror(errno));
        return false;
    }

    array_set(directory_stack->stack, idx, cwd, 1);

    return true;
}

char *dirstack_peek(const DirStack *directory_stack, size_t n) {
    if (n >= directory_stack->stack->count) {
        return NULL;
    }
    return strdup(directory_stack->stack->items[n]);
}

#endif /* TIDESH_DISABLE_DIRSTACK */
