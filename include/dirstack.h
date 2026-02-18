/** dirstack.h
 *
 * This file provides a directory stack implementation.
 * A directory stack is a list of directories that the user
 * can push and pop to change the current working directory.
 *
 * Useful for implementing pushd/popd commands in a shell.
 */

#ifndef DIRSTACK_H
#define DIRSTACK_H

#ifndef TIDESH_DISABLE_DIRSTACK

#include "data/array.h" /* Array */
#include <stdbool.h>    /* bool */
#include <stddef.h>     /* size_t */

/* A directory stack */
typedef struct DirStack {
    Array *stack; /* index 0 = top (most recent previous dir) */
} DirStack;

/**
 * Initialize a DirStack structure
 *
 * @param directory_stack Pointer to existing DirStack or NULL to allocate new
 * @return Pointer to initialized DirStack, or NULL on failure
 */
DirStack *init_dirstack(DirStack *directory_stack);

/**
 * Push the current directory onto the stack and change to the given path
 *
 * @param directory_stack The directory stack
 * @param path The path to change to
 * @return true on success, false on failure
 */
bool dirstack_pushd(DirStack *directory_stack, const char *path);

/**
 * Pop a directory off the stack and change to it
 *
 * @param directory_stack The directory stack
 * @return true on success, false on failure
 */
bool dirstack_popd(DirStack *directory_stack);

/**
 * Swap the current directory with the Nth directory in the stack
 *
 * @param directory_stack The directory stack
 * @param n The index of the directory to swap with
 * @return true on success, false on failure
 */
bool dirstack_swap(DirStack *directory_stack, size_t n); /* pushd +N */

/**
 * Peek at the Nth directory in the stack without modifying it
 *
 * @param directory_stack The directory stack
 * @param n The index of the directory to peek at
 * @return A newly allocated string with the directory path, or NULL if out of
 * bounds
 */
char *dirstack_peek(const DirStack *directory_stack, size_t n);

/**
 * Free all resources used by a DirStack structure
 *
 * @param directory_stack The directory stack to free
 */
void free_dirstack(DirStack *directory_stack);

#endif /* TIDESH_DISABLE_DIRSTACK */

#endif /* DIRSTACK_H */
