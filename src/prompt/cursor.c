#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */
#include <stdlib.h>  /* malloc */
#include <string.h>  /* strlen, strncpy */

#include "data/dynamic.h"
#include "data/utf8.h"
#include "history.h"
#include "prompt/ansi.h"
#include "prompt/completion.h"
#include "prompt/cursor.h"
#include "prompt/terminal.h"
#include "session.h"

/* Render text from a specific position to the end, injecting continuation
 * prompts after newlines. */
static void cursor_render_tail(Cursor *cursor, size_t start_index) {
    if (!cursor || !cursor->data)
        return;

    if (start_index < cursor->data->length) {
        char *p = cursor->data->value + start_index;

        // We iterate through the tail string
        while (*p) {
            // Find length of segment until next newline
            size_t segment_len = strcspn(p, "\n");

            if (segment_len > 0) {
                terminal_write_sized(p, segment_len);
                p += segment_len;
            }

            // If we hit a newline, print CRLF and the continuation prompt
            if (*p == '\n') {
                terminal_write("\r\n");
                if (cursor->continuation_prompt) {
                    terminal_write(cursor->continuation_prompt);
                }
                p++; // Skip the '\n'
            }
        }
    }

    // Render suggestion if at the end of the data and we have one
    if (cursor->suggestion && cursor->position == 0) {
        terminal_save_cursor();
        char *applied = ansi_apply(cursor->suggestion, cursor->data->value,
                                   ANSI_BRIGHT_BLACK, NULL);
        if (applied) {
            terminal_write(applied);
            free(applied);
        }
        terminal_restore_cursor();
    }
}

size_t visible_length(const char *str) {
    if (!str)
        return 0;
    char *stripped = ansi_strip(str);
    if (!stripped) {
        return 0;
    }
    size_t len = utf8_strlen(stripped);
    free(stripped);
    return len;
}

Cursor *init_cursor(Cursor *cursor, Session *session, const char *prompt,
                    const char *continuation_prompt) {
    bool allocated = false;
    if (!cursor) {
        cursor = malloc(sizeof(Cursor));
        if (!cursor)
            return NULL;
        allocated = true;
    }

    cursor->data = init_dynamic(NULL);
    if (!cursor->data) {
        if (allocated)
            free(cursor);
        return NULL;
    }

    cursor->position            = 0;
    cursor->visible_position    = 0;
    cursor->visible_length      = 0;
    cursor->prompt              = prompt;
    cursor->continuation_prompt = continuation_prompt;
    cursor->session             = session;
    cursor->keep                = NULL;
    cursor->suggestion          = NULL;
    return cursor;
}

CursorPosition cursor_terminal_position(Cursor *cursor) {
    CursorPosition cursor_pos = {0, 0};

    if (!cursor || !cursor->data || !cursor->data->value)
        return cursor_pos;

    size_t total_cols = cursor->session->terminal->cols;
    if (total_cols <= 0)
        total_cols = TERMINAL_DEFAULT_COLS;

    // Start with the prompt position
    size_t prompt_len = visible_length(cursor->prompt);
    size_t cont_len   = visible_length(cursor->continuation_prompt);

    size_t current_col = prompt_len % total_cols;
    size_t current_row = prompt_len / total_cols;

    // Calculate bytes to process
    size_t bytes_to_cursor = 0;
    if (cursor->data->length >= cursor->position) {
        bytes_to_cursor = cursor->data->length - cursor->position;
    }

    char *p   = cursor->data->value;
    char *end = p + bytes_to_cursor;

    while (p < end) {
        if (*p == '\n') {
            current_row++;
            // Reset to continuation prompt size, not original prompt
            current_col = cont_len % total_cols;
            p++;
        } else {
            unsigned char char_len = utf8_charlen(*p);
            // Safety check for malformed UTF8
            if (char_len == 0)
                char_len = 1;

            if (p + char_len > end)
                break;

            p += char_len;
            current_col++;

            if (current_col >= total_cols) {
                current_row++;
                current_col = 0;
            }
        }
    }

    cursor_pos.col = current_col;
    cursor_pos.row = current_row;
    return cursor_pos;
}

void cursor_insert(Cursor *cursor, char *string) {
    if (!cursor || !cursor->data || !string)
        return;

    size_t str_len = strlen(string);
    if (str_len == 0)
        return;

    size_t visible_insert_len = utf8_strlen(string);

    // If cursor is at the very end (position 0), just append
    if (cursor->position == 0) {
        terminal_clear_to_end();
        dynamic_extend(cursor->data, string);

        cursor_update_suggestion(cursor);

        // We can't just write check newline here if we want continuation
        // prompts on the inserted text itself.
        cursor_render_tail(cursor, cursor->data->length - str_len);

        cursor->visible_length += visible_insert_len;
    } else {
        // Insert in the middle
        size_t insert_pos = cursor->data->length - cursor->position;

        // Bounds check
        if (insert_pos > cursor->data->length) {
            insert_pos = cursor->data->length;
        }

        dynamic_insert(cursor->data, insert_pos, string);
        cursor->visible_length += visible_insert_len;

        cursor_update_suggestion(cursor);

        // Re-render
        terminal_save_cursor();
        // Use helper to render tail with continuation prompts
        cursor_render_tail(cursor, insert_pos);
        terminal_restore_cursor();

        // Move the cursor forward on screen
        for (size_t i = 0; i < visible_insert_len; i++) {
            CursorPosition cursor_pos = cursor_terminal_position(cursor);
            size_t         total_cols = cursor->session->terminal->cols
                                            ? cursor->session->terminal->cols
                                            : TERMINAL_DEFAULT_COLS;

            if (cursor_pos.col >= total_cols - 1) {
                terminal_cursor_down(1);
                // Move to end of continuation prompt
                terminal_cursor_to_column(
                    visible_length(cursor->continuation_prompt));
            } else {
                terminal_cursor_forward(1);
            }
        }
    }
}

bool cursor_delete(Cursor *cursor) {
    if (!cursor || !cursor->data || !cursor->data->length)
        return false;

    // Can't delete if at the start of the line (position == length)
    if (cursor->position >= cursor->data->length)
        return false;

    char *char_at_cursor =
        cursor->data->value + (cursor->data->length - cursor->position);

    // Find char before cursor
    char *deleting_character =
        utf8_prev_char(char_at_cursor, cursor->data->value);

    if (!deleting_character || deleting_character < cursor->data->value)
        return false;

    size_t deleting_length = char_at_cursor - deleting_character;
    size_t delete_pos      = deleting_character - cursor->data->value;

    CursorPosition cursor_pos = cursor_terminal_position(cursor);

    dynamic_remove(cursor->data, delete_pos, deleting_length);

    if (cursor->visible_length > 0) {
        cursor->visible_length -= 1;
    }

    CursorPosition new_pos = cursor_terminal_position(cursor);

    // Move visual cursor back
    if (new_pos.row < cursor_pos.row) {
        terminal_cursor_up(1);
        terminal_cursor_to_column(new_pos.col);
    } else if (new_pos.col < cursor_pos.col) {
        terminal_cursor_backward(1);
    }

    terminal_clear_to_end();

    cursor_update_suggestion(cursor);

    // Re-render from the new position
    terminal_save_cursor();
    // Render tail with continuation prompts
    cursor_render_tail(cursor, delete_pos);
    terminal_restore_cursor();

    return true;
}

bool cursor_delete_word(Cursor *cursor) {
    if (!cursor || !cursor->data || !cursor->data->length)
        return false;
    if (cursor->position == cursor->data->length)
        return false;

    bool deleted_any = false;

    // Skip trailing delimiters
    while (cursor->position < cursor->data->length) {
        char char_at_cursor =
            cursor->data->value[cursor->data->length - cursor->position - 1];
        if (!is_shell_delimiter(char_at_cursor))
            break;
        if (!cursor_delete(cursor))
            break;
        deleted_any = true;
    }

    // Delete until next delimiter
    while (cursor->position < cursor->data->length) {
        char char_at_cursor =
            cursor->data->value[cursor->data->length - cursor->position - 1];
        if (is_shell_delimiter(char_at_cursor))
            break;
        if (!cursor_delete(cursor))
            break;
        deleted_any = true;
    }

    return deleted_any;
}

bool cursor_delete_line(Cursor *cursor) {
    if (!cursor || !cursor->data || !cursor->data->length)
        return false;

    bool deleted_any = false;

    while (cursor->position < cursor->data->length) {
        char char_at_cursor =
            cursor->data->value[cursor->data->length - cursor->position - 1];

        // Break if we hit a newline
        if (char_at_cursor == '\n')
            break;

        if (!cursor_delete(cursor))
            break;
        deleted_any = true;
    }
    return deleted_any;
}

void cursor_append(Cursor *cursor, char character) {
    if (!cursor || !cursor->data)
        return;

    if (cursor->suggestion) {
        terminal_clear_to_end();
    }

    // Append always happens at the very end
    dynamic_append(cursor->data, character);

    if (character == '\n') {
        terminal_write("\r\n");
    } else {
        char buf[2] = {character, '\0'};
        terminal_write(buf);
    }

    cursor_update_suggestion(cursor);
    if (cursor->suggestion) {
        terminal_save_cursor();
        cursor_render_tail(cursor, cursor->data->length);
        terminal_restore_cursor();
    }

    cursor->visible_length += 1;
}

void free_cursor(Cursor *cursor) {
    if (!cursor)
        return;

    if (cursor->keep) {
        free(cursor->keep);
    }

    if (cursor->suggestion) {
        free(cursor->suggestion);
    }

    if (cursor->data) {
        free_dynamic(cursor->data);
        free(cursor->data);
    }
}

size_t cursor_eol_distance(Cursor *cursor) {
    if (!cursor || !cursor->data)
        return 0;

    size_t total_cols = cursor->session->terminal->cols;
    if (total_cols == 0)
        total_cols = TERMINAL_DEFAULT_COLS;

    char *p = cursor->data->value + (cursor->data->length - cursor->position);
    CursorPosition cursor_pos  = cursor_terminal_position(cursor);
    size_t         current_col = cursor_pos.col;
    size_t         distance    = 0;

    while (*p != '\0' && *p != '\n') {
        unsigned char char_len = utf8_charlen(*p);
        if (char_len == 0)
            char_len = 1;

        p += char_len;
        current_col++;
        distance++;

        if (current_col >= total_cols)
            break;
    }
    return distance;
}

void cursor_backward(Cursor *cursor, size_t n) {
    if (!cursor || !cursor->data)
        return;

    size_t moved = 0;
    while (moved < n && cursor->position < cursor->data->length) {
        if (cursor->position == 0 && cursor->suggestion) {
            terminal_clear_to_end();
        }

        char *char_at_cursor =
            cursor->data->value + (cursor->data->length - cursor->position);
        char *prev_char = utf8_prev_char(char_at_cursor, cursor->data->value);

        if (!prev_char || prev_char < cursor->data->value)
            break;

        bool   is_newline = (*prev_char == '\n');
        size_t char_len   = char_at_cursor - prev_char;

        CursorPosition cursor_pos = cursor_terminal_position(cursor);

        cursor->position += char_len;
        cursor->visible_position += 1;

        CursorPosition new_pos = cursor_terminal_position(cursor);

        if (is_newline || cursor_pos.col == 0) {
            terminal_cursor_up(1);
            terminal_cursor_to_column(new_pos.col);
        } else {
            terminal_cursor_backward(1);
        }
        moved++;
    }
}

void cursor_forward(Cursor *cursor, size_t n) {
    if (!cursor || !cursor->data)
        return;

    size_t total_cols = cursor->session->terminal->cols;
    if (total_cols == 0)
        total_cols = TERMINAL_DEFAULT_COLS;

    size_t moved = 0;
    while (moved < n && cursor->position > 0) {
        size_t char_pos = cursor->data->length - cursor->position;
        if (char_pos >= cursor->data->length)
            break;

        char          current_char = cursor->data->value[char_pos];
        bool          is_newline   = (current_char == '\n');
        unsigned char char_len     = utf8_charlen(current_char);
        if (char_len == 0)
            char_len = 1;

        if (char_len > cursor->position)
            char_len = cursor->position;

        CursorPosition cursor_pos = cursor_terminal_position(cursor);

        cursor->position -= char_len;
        if (cursor->visible_position > 0)
            cursor->visible_position -= 1;

        if (is_newline || cursor_pos.col >= total_cols - 1) {
            terminal_cursor_down(1);
            terminal_cursor_to_column(
                visible_length(cursor->continuation_prompt));
        } else {
            terminal_cursor_forward(1);
        }

        if (cursor->position == 0 && cursor->suggestion) {
            terminal_save_cursor();
            cursor_render_tail(cursor, cursor->data->length);
            terminal_restore_cursor();
        }

        moved++;
    }
}

void cursor_move_word_left(Cursor *cursor) {
    if (!cursor || !cursor->data || cursor->position >= cursor->data->length)
        return;

    // Skip leading delimiters
    while (cursor->position < cursor->data->length) {
        char char_at_cursor =
            cursor->data->value[cursor->data->length - cursor->position - 1];
        if (!is_shell_delimiter(char_at_cursor))
            break;
        cursor_backward(cursor, 1);
    }

    // Move until next delimiter
    while (cursor->position < cursor->data->length) {
        char char_at_cursor =
            cursor->data->value[cursor->data->length - cursor->position - 1];
        if (is_shell_delimiter(char_at_cursor))
            break;
        cursor_backward(cursor, 1);
    }
}

void cursor_move_word_right(Cursor *cursor) {
    if (!cursor || !cursor->data || cursor->position == 0)
        return;

    // Skip leading delimiters
    while (cursor->position > 0) {
        char char_at_cursor =
            cursor->data->value[cursor->data->length - cursor->position];
        if (!is_shell_delimiter(char_at_cursor))
            break;
        cursor_forward(cursor, 1);
    }

    // Move until next delimiter
    while (cursor->position > 0) {
        char char_at_cursor =
            cursor->data->value[cursor->data->length - cursor->position];
        if (is_shell_delimiter(char_at_cursor))
            break;
        cursor_forward(cursor, 1);
    }
}

void cursor_update_suggestion(Cursor *cursor) {
    if (!cursor || !cursor->data || !cursor->session ||
        !cursor->session->terminal->supports_colors) {
        return;
    }

    if (cursor->suggestion) {
        free(cursor->suggestion);
        cursor->suggestion = NULL;
    }

    if (cursor->data->length == 0) {
        return;
    }

    char *current_input = dynamic_to_string(cursor->data);
    if (!current_input)
        return;

    char *match = history_last_command_starting_with(cursor->session->history,
                                                     current_input);
    if (match && strlen(match) > cursor->data->length) {
        cursor->suggestion = strdup(match + cursor->data->length);
    }

    free(current_input);
}

void cursor_apply_suggestion(Cursor *cursor) {
    if (!cursor || !cursor->suggestion)
        return;

    cursor_insert(cursor, cursor->suggestion);
}

void cursor_set(Cursor *cursor, char *string, bool history) {
    if (!cursor || !cursor->data || !string)
        return;

    if (history && !cursor->keep) {
        cursor->keep = dynamic_to_string(cursor->data);
    }

    // Clear old line from screen
    CursorPosition cursor_pos = cursor_terminal_position(cursor);
    for (size_t i = 0; i < cursor_pos.row; i++) {
        terminal_cursor_up(1);
    }
    terminal_cursor_to_column(visible_length(cursor->prompt));
    terminal_write(ANSI_ERASE_CURSOR_TO_EOF);

    // Reset data
    dynamic_clear(cursor->data);
    dynamic_extend(cursor->data, string);

    cursor->position         = 0;
    cursor->visible_position = 0;
    cursor->visible_length   = visible_length(cursor->data->value);

    terminal_clear_to_end();
    cursor_update_suggestion(cursor);

    // Write new string with proper continuation prompt handling
    cursor_render_tail(cursor, 0);
}

void cursor_clear_screen(Cursor *cursor) {
    if (!cursor || !cursor->data)
        return;
    terminal_clear_screen();
    cursor->position         = 0;
    cursor->visible_position = 0;
    cursor->visible_length   = 0;
    dynamic_clear(cursor->data);
}

/* Helper to find the buffer index corresponding to a specific visual row/col */
static size_t cursor_get_index_at_pos(Cursor *cursor, size_t target_row,
                                      size_t target_col) {
    if (!cursor || !cursor->data)
        return 0;

    size_t total_cols = cursor->session->terminal->cols;
    if (total_cols == 0)
        total_cols = TERMINAL_DEFAULT_COLS;

    // Start sizes
    size_t prompt_len = visible_length(cursor->prompt);
    size_t cont_len   = visible_length(cursor->continuation_prompt);

    size_t current_col = prompt_len % total_cols;
    size_t current_row = prompt_len / total_cols;

    char *p          = cursor->data->value;
    char *end        = cursor->data->value + cursor->data->length;
    char *target_ptr = p;

    // Iterate through buffer to find the character at (target_row, target_col)
    while (p < end) {
        // If we are on the target row, check if we reached the target column
        if (current_row == target_row) {
            if (current_col >= target_col) {
                return cursor->data->length - (p - cursor->data->value);
            }
            target_ptr = p; // Keep tracking the closest valid char
        }

        // Stop if we went past the target row
        if (current_row > target_row) {
            // We missed the exact column (line was shorter), return the end of
            // that line
            return cursor->data->length - (target_ptr - cursor->data->value);
        }

        if (*p == '\n') {
            // If we are ending the target row and haven't returned, return this
            // newline
            if (current_row == target_row) {
                return cursor->data->length - (p - cursor->data->value);
            }
            current_row++;
            current_col = cont_len % total_cols;
            p++;
        } else {
            unsigned char char_len = utf8_charlen(*p);
            if (char_len == 0)
                char_len = 1;

            if (p + char_len > end)
                break;

            p += char_len;
            current_col++;

            if (current_col >= total_cols) {
                current_row++;
                current_col = 0;
            }

            // Update target pointer if we are still on the target row
            if (current_row == target_row)
                target_ptr = p;
        }
    }

    return 0;
}

bool cursor_go_up(Cursor *cursor) {
    CursorPosition pos = cursor_terminal_position(cursor);

    // If we are already at the top line, can't go up
    if (pos.row == 0)
        return false;

    // Calculate target visual position
    size_t target_row = pos.row - 1;
    size_t target_col = pos.col;

    // Convert visual target back to buffer position (distance from end)
    size_t new_pos = cursor_get_index_at_pos(cursor, target_row, target_col);

    // Apply change
    cursor->position = new_pos;

    // Move visual cursor
    terminal_cursor_up(1);

    // Re-align column (since lines might have different lengths or wrapping)
    CursorPosition new_visual = cursor_terminal_position(cursor);
    if (new_visual.col != target_col) {
        terminal_cursor_to_column(new_visual.col);
    }

    return true;
}

bool cursor_go_down(Cursor *cursor) {
    CursorPosition pos = cursor_terminal_position(cursor);

    // Try to find index for row + 1
    size_t target_row = pos.row + 1;
    size_t target_col = pos.col;

    // We need to know if target_row actually exists.
    size_t new_pos = cursor_get_index_at_pos(cursor, target_row, target_col);

    // Temporary apply to check row
    size_t old_pos_val       = cursor->position;
    cursor->position         = new_pos;
    CursorPosition check_pos = cursor_terminal_position(cursor);
    cursor->position         = old_pos_val; // Restore

    if (check_pos.row <= pos.row) {
        // We couldn't move down (already at bottom)
        return false;
    }

    cursor->position = new_pos;
    terminal_cursor_down(1);

    // Re-align column
    CursorPosition new_visual = cursor_terminal_position(cursor);
    if (new_visual.col != target_col) {
        terminal_cursor_to_column(new_visual.col);
    }

    return true;
}
