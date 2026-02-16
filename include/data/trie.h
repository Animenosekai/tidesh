/* trie.h

This module provides a Trie (prefix tree) data structure for storing
strings with efficient prefix-based operations.
*/

#ifndef DATA_TRIE_H
#define DATA_TRIE_H

#include <stdbool.h>
#include <stddef.h>

#include "data/array.h"

#define ALPHABET_SIZE 256 // Support extended ASCII so UTF-8 works

typedef struct Trie Trie;

/** Initialize a Trie
 *
 * @param node The node to initialize. If NULL, a new node will be allocated
 */
Trie *init_trie(Trie *node);

/** Free all resources used by a Trie
 *
 * @param trie The trie to free
 */
void free_trie(Trie *trie);

/** Insert key with associated value (a string)
 *
 * @param trie The trie to insert into
 * @param key The key to insert
 * @param value The value to associate with the key. Can be NULL.
 */
bool trie_set(Trie *trie, char *key, char *value);

/** Add a key with no associated value
 *
 * @param trie The trie to insert into
 * @param key The key to insert
 */
bool trie_add(Trie *trie, char *key);

/** Return whether the trie contains the given key
 *
 * @param trie The trie to search
 * @param key The key to search for
 * @return true if the key exists, false otherwise
 */
bool trie_contains(Trie *trie, char *key);

/** Search for a key and return stored value (or NULL)
 *
 * @param trie The trie to search
 * @param key The key to search for
 * @return The value associated with the key, or NULL if not found
 */
char *trie_get(Trie *trie, char *key);

/** Check if any key starts with this prefix
 *
 * @param trie The trie to search
 * @param prefix The prefix to search for
 * @return true if any key starts with the prefix, false otherwise
 */
bool trie_starts_with(Trie *trie, char *prefix);

/** Get all nodes' keys starting with the given prefix
 *
 * @param trie The trie to search
 * @param prefix The prefix to search for
 * @return An array of keys starting with the prefix
 */
Array *trie_starting_with(Trie *trie, char *prefix);

/** Remove a key and its value
 *
 * @param trie The trie to modify
 * @param key The key to remove
 * @return true if the key was found and removed, false otherwise
 */
bool trie_delete_key(Trie *trie, char *key);

/** Copy a Trie
 *
 * @param src The source Trie to copy from
 * @param dest The destination Trie to copy to, or NULL to allocate a new one
 * @return Pointer to the copied Trie, or NULL on failure
 */
Trie *trie_copy(Trie *src, Trie *dest);

#endif /* DATA_TRIE_H */
