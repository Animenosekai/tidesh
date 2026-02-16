#include <stdlib.h> /* malloc, free, realloc */
#include <string.h> /* strdup */

#include "data/array.h" /* Array, array_add, array_append, array_create, free_array */
#include "data/dynamic.h" /* Dynamic, init_dynamic, dynamic_append, dynamic_to_string, free_dynamic */
#include "data/trie.h"

typedef struct Trie {
    struct Trie *children[ALPHABET_SIZE];
    char        *value; // NULL if no value stored here
} Trie;

Trie *init_trie(Trie *node) {
    if (!node) {
        node = malloc(sizeof(Trie));
        if (!node)
            return NULL;
    } else {
        free_trie(node);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++)
        node->children[i] = NULL;

    node->value = NULL;
    return node;
}

bool trie_set(Trie *root, char *key, char *value) {
    Trie *cur = root;

    for (char *p = key; *p; p++) {
        unsigned char idx = (unsigned char)*p;

        if (!cur->children[idx]) {
            cur->children[idx] = init_trie(NULL);
            if (!cur->children[idx]) {
                return false; // malloc failed
            }
        }

        cur = cur->children[idx];
    }

    /* Replace prior value */
    free(cur->value);
    cur->value = value ? strdup(value) : NULL;
    return true;
}

bool trie_add(Trie *root, char *key) { return trie_set(root, key, ""); }

bool trie_contains(Trie *root, char *key) {
    Trie *cur = root;

    for (char *p = key; *p; p++) {
        unsigned char idx = (unsigned char)*p;

        if (!cur->children[idx])
            return false;

        cur = cur->children[idx];
    }

    return cur->value != NULL;
}

char *trie_get(Trie *root, char *key) {
    Trie *cur = root;

    for (char *p = key; *p; p++) {
        unsigned char idx = (unsigned char)*p;

        if (!cur->children[idx])
            return NULL;

        cur = cur->children[idx];
    }

    return cur->value; // may be NULL
}

bool trie_starts_with(Trie *root, char *prefix) {
    Trie *cur = root;

    for (char *p = prefix; *p; p++) {
        unsigned char idx = (unsigned char)*p;

        if (!cur->children[idx])
            return false;

        cur = cur->children[idx];
    }

    return true;
}

static void traverse(Trie *node, Dynamic *key, Array *results) {
    if (node->value) {
        char *k = dynamic_to_string(key);
        array_add(results, k);
        free(k);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            dynamic_append(key, (char)i);
            traverse(node->children[i], key, results);
            key->length--; // backtrack
        }
    }
}

Array *trie_starting_with(Trie *root, char *prefix) {
    Array *results = init_array(NULL);
    Trie  *cur     = root;

    for (char *p = prefix; *p; p++) {
        unsigned char idx = (unsigned char)*p;

        if (!cur->children[idx]) {
            return results; // empty
        }

        cur = cur->children[idx];
    }

    /* Now traverse from cur and collect all keys */
    Dynamic current_key = {0};
    init_dynamic(&current_key);
    dynamic_extend(&current_key, prefix);

    traverse(cur, &current_key, results);
    free_dynamic(&current_key);

    return results;
}

/* Helper: recursively delete nodes if empty */
static bool trie_delete_rec(Trie *node, char *key, bool *should_prune) {
    if (*key == '\0') {
        if (!node->value) {
            *should_prune = false;
            return false; // key not found
        }

        free(node->value);
        node->value = NULL;

        // If no children, this node can be freed
        for (int i = 0; i < ALPHABET_SIZE; i++) {
            if (node->children[i]) {
                *should_prune = false;
                return true;
            }
        }

        *should_prune = true;
        return true;
    }

    unsigned char idx = (unsigned char)*key;
    if (!node->children[idx]) {
        *should_prune = false;
        return false;
    }

    bool child_prune = false;
    bool deleted = trie_delete_rec(node->children[idx], key + 1, &child_prune);
    if (!deleted) {
        *should_prune = false;
        return false;
    }

    if (child_prune) {
        free_trie(node->children[idx]);
        free(node->children[idx]);
        node->children[idx] = NULL;
    }

    if (node->value) {
        *should_prune = false;
        return true;
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            *should_prune = false;
            return true;
        }
    }

    *should_prune = true;
    return true;
}

bool trie_delete_key(Trie *root, char *key) {
    bool should_prune = false;
    return trie_delete_rec(root, key, &should_prune);
}

void free_trie(Trie *root) {
    if (!root)
        return;

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i]) {
            free_trie(root->children[i]);
            free(root->children[i]);
            root->children[i] = NULL;
        }
    }

    if (root->value) {
        free(root->value);
        root->value = NULL;
    }
}

Trie *trie_copy(Trie *src, Trie *dest) {
    if (!src)
        return NULL;

    dest = init_trie(dest);
    if (!dest)
        return NULL;

    if (src->value) {
        dest->value = strdup(src->value);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (src->children[i]) {
            dest->children[i] = trie_copy(src->children[i], NULL);
        }
    }

    return dest;
}
