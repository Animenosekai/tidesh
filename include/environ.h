/** environ.h
 *
 * This module provides functions to manipulate and query the environment
 * variables for the shell.
 */

#ifndef ENVIRON_H
#define ENVIRON_H

#include <stdbool.h>   /* bool */
#include <sys/types.h> /* pid_t */

#include "data/array.h" /* Array */

// Declare Environ as an opaque struct
typedef struct Environ Environ;

/**
 * Initialize the environment variables
 *
 * @param env Pointer to existing Environ or NULL to allocate new
 * @return Pointer to initialized Environ, or NULL on failure
 */
Environ *init_environ(Environ *env);

/**
 * Check if a variable `key` exists in the environment
 *
 * @param env Pointer to Environ
 * @param key Variable name to check
 * @return true if variable exists, false otherwise
 */
bool environ_contains(Environ *env, char *key);

/**
 * Get the value of a variable `key` in the environment
 *
 * @param env Pointer to Environ
 * @param key Variable name to get
 * @return Pointer to value if found, NULL otherwise
 */
char *environ_get(Environ *env, char *key);

/**
 * Get the value of a variable `key` in the environment, or return
 * `default_value` if not found
 *
 * @param env Pointer to Environ
 * @param key Variable name to get
 * @param default_value Value to return if key is not found
 * @return Pointer to value if found, `default_value` otherwise
 */
char *environ_get_default(Environ *env, char *key, char *default_value);

/**
 * Sets a variable `key` to value `value` in the environment
 *
 * @param env Pointer to Environ
 * @param key Variable name to set
 * @param value Value to set
 */
void environ_set(Environ *env, char *key, char *value);

/**
 * Sets the exit status variable $? in the environment
 *
 * @param env Pointer to Environ
 * @param status Exit status value
 */
void environ_set_exit_status(Environ *env, int status);

/**
 * Sets the background PID variable $! in the environment
 *
 * @param env Pointer to Environ
 * @param pid Background PID
 */
void environ_set_background_pid(Environ *env, pid_t pid);

/**
 * Sets the last argument variable $_ in the environment
 *
 * @param env Pointer to Environ
 * @param arg Last argument string
 */
void environ_set_last_arg(Environ *env, const char *arg);

/**
 * Creates a copy of the given environment
 *
 * @param src_env Pointer to Environ to copy from
 * @param dest_env Pointer to Environ to copy to
 * @return Pointer to new Environ copy, or NULL on failure
 */
Environ *environ_copy(Environ *src, Environ *dest);

/**
 * Frees the environment
 *
 * @param env Pointer to Environ to free
 */
void free_environ(Environ *env);

/**
 * Converts the environment to an Array of strings
 *
 * @param env Pointer to Environ
 * @return Pointer to Array of strings, or NULL on failure
 */
Array *environ_to_array(Environ *env);

#endif /* ENVIRON_H */
