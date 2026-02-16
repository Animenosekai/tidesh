/** history.h
 *
 * Declarations for command history management.
 * This module provides functions to initialize, load, save,
 * append to, and navigate through command history.
 */

#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

/* Holds a single command in the history list */
typedef struct HistoryEntry {
    char                *command;   // The command string
    long                 timestamp; // Unix timestamp
    struct HistoryEntry *next;      // Next entry (newer)
    struct HistoryEntry *prev;      // Previous entry (older)
} HistoryEntry;

/* The history state container */
typedef struct History {
    HistoryEntry *head;          // Oldest entry
    HistoryEntry *tail;          // Newest entry
    HistoryEntry *current;       // Navigation pointer (NULL = at live prompt)
    size_t        size;          // Current number of entries
    size_t        limit;         // Max number of entries
    bool          disabled;      // Whether history is disabled
    char         *filepath;      // Filepath for persistence
    bool          owns_filepath; // Whether filepath should be freed
} History;

/**
 * Initialize history structure
 *
 * @param history Pointer to History to initialize, or NULL to create new
 * @return Pointer to initialized History
 */
History *init_history(History *history);

/**
 * Load history from disk
 *
 * @param history Pointer to History to load into, or NULL to create new
 * @param filepath Path to history file
 * @return Pointer to loaded History
 */
History *load_history(History *history, char *filepath);

/**
 * Clear history entries from memory and truncate the history file on disk
 *
 * @param history Pointer to History to clear
 */
void history_clear(History *history);

/**
 * Save current history list to the configured filepath
 *
 * @param history Pointer to History to save
 */
void history_save(History *history);

/**
 * Remove a specific command string from history
 *
 * @param history Pointer to History
 * @param command Command string to remove
 * @param all If true, remove all occurrences; if false, remove only the first
 * occurrence
 * @return true if any entries were removed, false otherwise
 */
bool history_remove(History *history, const char *command, bool all);

/**
 * Add a command to history. Handles deduplication and history limits.
 *
 * @param history Pointer to History
 * @param command Command string to add
 */
void history_append(History *history, const char *command);

/**
 * Enforces the history limit by removing the oldest entries until the size is
 * within the limit.
 *
 * @param history Pointer to History
 * @return Number of entries removed
 */
size_t history_enforce_limit(History *history);

/**
 * Get the next (newer) history entry for navigation (DOWN key)
 *
 * @param history Pointer to History
 * @return Next command string, or NULL if at the bottom
 */
char *history_get_next(History *history);

/**
 * Get the previous (older) history entry for navigation (UP key)
 *
 * @param history Pointer to History
 * @return Previous command string, or stays at the oldest entry
 */
char *history_get_previous(History *history);

/**
 * Resets the navigation pointer to the bottom (NULL), representing the live
 * prompt.
 *
 * @param history Pointer to History
 */
void history_reset_state(History *history);

/** Get the nth last command from history
 *
 * @param history Pointer to History
 * @param n Index from the end (0 = last command)
 * @return Nth last command string
 */
char *history_nth_last_command(History *history, size_t n);

/** Get the nth command from history
 *
 * @param history Pointer to History
 * @param n Index from the beginning (0 = first command)
 * @return Nth command string
 */
char *history_nth_command(History *history, size_t n);

/** Get the last command from history
 *
 * @param history Pointer to History
 * @return Last command string
 */
char *history_last_command(History *history);

/** Get the last command starting with a prefix
 *
 * @param history Pointer to History
 * @param prefix Prefix string
 * @return Last command string starting with prefix
 */
char *history_last_command_starting_with(History *history, char *prefix);

/**
 * Free all memory associated with the history list and filepath
 *
 * @param history Pointer to History to free
 */
void free_history(History *history);

#endif /* HISTORY_H */
