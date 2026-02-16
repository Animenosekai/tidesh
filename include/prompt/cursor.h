/** cursor.h
 *
 * Cursor structure and related functions
 */

#ifndef PROMPT_CURSOR_H
#define PROMPT_CURSOR_H

#include "session.h"
#include <stddef.h> /* size_t */
#include "data/dynamic.h" /* Dynamic */

typedef struct Cursor {
    char *keep; // Pointer to the previous cursor state for history navigation
    char *suggestion; // Pointer to the current suggestion string
    Dynamic    *data;             // The current input data
    size_t      position;         // Byte position from the end of the data
    size_t      visible_position; // Visible position from the end of the data
    size_t      visible_length;   // Visible length of the data
    const char *prompt;           // Prompt string
    const char *continuation_prompt; // Continuation prompt string
    Session    *session;             // Associated session
} Cursor;

typedef struct CursorPosition {
    size_t row; // The current row of the cursor
    size_t col; // The current column of the cursor
} CursorPosition;

/**
 * Calculates the visible length of a unicode string, ignoring ANSI escape
 * codes.
 *
 * @param str The input string.
 * @return The visible length of the string.
 */
size_t visible_length(const char *str);

/**
 * Initializes a Cursor structure.
 *
 * @param cursor Pointer to an existing Cursor structure, or NULL to allocate
 * a new one.
 * @param session Pointer to the associated Session structure.
 * @param prompt The prompt string.
 * @param continuation_prompt The continuation prompt string.
 * @return Pointer to the initialized Cursor structure, or NULL on failure.
 */
Cursor *init_cursor(Cursor *cursor, Session *session, const char *prompt,
                    const char *continuation_prompt);

/**
 * Calculates the terminal position of the cursor.
 *
 * @param cursor Pointer to the Cursor structure.
 * @return The CursorPosition structure with row and column.
 */
CursorPosition cursor_terminal_position(Cursor *cursor);

/**
 * Inserts a string at the current cursor position.
 *
 * @param cursor Pointer to the Cursor structure.
 * @param string The string to insert.
 */
void cursor_insert(Cursor *cursor, char *string);

/**
 * Deletes a character before the current cursor position.
 *
 * @param cursor Pointer to the Cursor structure.
 * @return true if a character was deleted, false otherwise.
 */
bool cursor_delete(Cursor *cursor);

/**
 * Deletes a word before the current cursor position.
 *
 * @param cursor Pointer to the Cursor structure.
 * @return true if a word was deleted, false otherwise.
 */
bool cursor_delete_word(Cursor *cursor);

/**
 * Deletes the entire line before the current cursor position.
 *
 * @param cursor Pointer to the Cursor structure.
 * @return true if the line was deleted, false otherwise.
 */
bool cursor_delete_line(Cursor *cursor);

/**
 * Clears the entire screen and resets the cursor state.
 *
 * @param cursor Pointer to the Cursor structure.
 */
void cursor_clear_screen(Cursor *cursor);

// /**
//  * Calculates the column at the end of the current cursor line.
//  *
//  * @param cursor Pointer to the Cursor structure.
//  * @return The column position at the end of the line.
//  */
// size_t cursor_eol_column(Cursor *cursor);

/**
 * Calculates the distance from the cursor to the end of the line.
 *
 * @param cursor Pointer to the Cursor structure.
 * @return The distance to the end of the line.
 */
size_t cursor_eol_distance(Cursor *cursor);

/**
 * Appends a character at the end of the current input.
 *
 * @param cursor Pointer to the Cursor structure.
 * @param character The character to append.
 */
void cursor_append(Cursor *cursor, char character);

/**
 * Moves the cursor forward by n positions.
 *
 * @param cursor Pointer to the Cursor structure.
 * @param n Number of positions to move forward.
 */
void cursor_forward(Cursor *cursor, size_t n);

/**
 * Moves the cursor backward by n positions.
 *
 * @param cursor Pointer to the Cursor structure.
 * @param n Number of positions to move backward.
 */
void cursor_backward(Cursor *cursor, size_t n);

/**
 * Moves the cursor forward by one word.
 *
 * @param cursor Pointer to the Cursor structure.
 */
void cursor_move_word_right(Cursor *cursor);

/**
 * Moves the cursor backward by one word.
 *
 * @param cursor Pointer to the Cursor structure.
 */
void cursor_move_word_left(Cursor *cursor);

/**
 * Updates the suggestion based on the current input and history.
 *
 * @param cursor Pointer to the Cursor structure.
 */
void cursor_update_suggestion(Cursor *cursor);

/**
 * Applies the current suggestion to the cursor data.
 *
 * @param cursor Pointer to the Cursor structure.
 */
void cursor_apply_suggestion(Cursor *cursor);

/**
 * Frees resources used by a Cursor structure.
 *
 * @param cursor Pointer to the Cursor structure to free.
 */
void free_cursor(Cursor *cursor);

/**
 * Sets the cursor content to the specified string.
 *
 * @param cursor Pointer to the Cursor structure.
 * @param string The string to set as the cursor content.
 * @param history If true, saves the current content for history navigation.
 */
void cursor_set(Cursor *cursor, char *string, bool history);

/**
 * Moves the cursor up by one visual line, if possible.
 *
 * @param cursor Pointer to the Cursor structure.
 * @return true if the cursor moved up, false if already at the top line.
 */
bool cursor_go_up(Cursor *cursor);

/**
 * Moves the cursor down by one visual line, if possible.
 *
 * @param cursor Pointer to the Cursor structure.
 * @return true if the cursor moved down, false if already at the bottom line.
 */
bool cursor_go_down(Cursor *cursor);

#endif /* PROMPT_CURSOR_H */
