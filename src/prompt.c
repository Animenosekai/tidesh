#include <errno.h>   /* errno, EINTR */
#include <stdbool.h> /* bool, true, false */
#include <stddef.h>  /* size_t, NULL */
#include <stdio.h>   /* NULL */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* strchr, strdup, strlen */
#include <unistd.h>  /* read, STDIN_FILENO */

#include "data/dynamic.h" /* dynamic_extend, dynamic_append, dynamic_to_string */
#include "data/utf8.h"    /* utf8_strlen */
#include "history.h" /* history_reset_state, history_get_previous, history_get_next */
#include "prompt/completion.h" /* completion_apply */
#include "prompt/cursor.h" /* Cursor, CursorPosition, init_cursor, free_cursor, cursor_* functions, visible_length */
#include "prompt/keyboard.h" /* Key, keyboard_parse, KEY_* */
#include "prompt/terminal.h" /* terminal_setup, terminal_restore, terminal_write_check_newline, terminal_write, terminal_newline_checked, terminal_check_resize */
#include "session.h"         /* Session */

#define PROMPT_BUFFER_SIZE 256

/* Handle special key inputs */
static void handle_key(Key key, Cursor *cursor) {
    switch (key.type) {
        case KEY_VALUE:
            if (key.ctrl) {
                if (key.value == 'u') {
                    cursor_delete_line(cursor);
                } else if (key.value == 'a') {
                    CursorPosition pos = cursor_terminal_position(cursor);
                    cursor_backward(cursor,
                                    pos.col - visible_length(cursor->prompt));
                } else if (key.value == 'e') {
                    size_t distance = cursor_eol_distance(cursor);
                    cursor_forward(cursor, distance);
                } else if (key.value == 'l') {
                    // Ctrl+L is usually handled in the loop, but good to have
                    // here
                    cursor_clear_screen(cursor);
                }
            } else if (key.alt || key.meta) {
                if (key.value == 'b') {
                    cursor_move_word_left(cursor);
                } else if (key.value == 'f') {
                    cursor_move_word_right(cursor);
                }
            }
            break;

        case KEY_BACKSPACE:
            if (key.alt)
                cursor_delete_word(cursor);
            else if (key.meta)
                cursor_delete_line(cursor);
            else
                cursor_delete(cursor);
            break;

        case KEY_LEFT:
            if (key.ctrl || key.alt)
                cursor_move_word_left(cursor);
            else
                cursor_backward(cursor, 1);
            break;

        case KEY_RIGHT:
            if (cursor->position == 0 && cursor->suggestion) {
                cursor_apply_suggestion(cursor);
            } else if (key.ctrl || key.alt) {
                cursor_move_word_right(cursor);
            } else {
                cursor_forward(cursor, 1);
            }
            break;

        case KEY_TAB:
            if (cursor->session->features.completion) {
                completion_apply(cursor, cursor->session);
            }
            break;

        case KEY_UP: {
            // Try to move cursor up visually first (multiline handling)
            if (cursor_go_up(cursor)) {
                break;
            }

            // If at the top line, trigger history
#ifndef TIDESH_DISABLE_HISTORY
            char *prev_cmd = history_get_previous(cursor->session->history);
            if (prev_cmd) {
                cursor_set(cursor, prev_cmd, true);
            }
#endif
        } break;

        case KEY_DOWN: {
            // Try to move cursor down visually first
            if (cursor_go_down(cursor)) {
                break;
            }

            // If at the bottom line, trigger history
#ifndef TIDESH_DISABLE_HISTORY
            char *next_cmd = history_get_next(cursor->session->history);
            if (next_cmd) {
                cursor_set(cursor, next_cmd, true);
            } else {
                // Restore the kept buffer (scratchpad)
                if (cursor->keep) {
                    cursor_set(cursor, cursor->keep, false);
                }
            }
#endif
        } break;

        default:
            break;
    }
}

static bool read_until_enter(Cursor *cursor) {
    char temp[PROMPT_BUFFER_SIZE];

    while (1) {
        terminal_check_resize(cursor->session);
        ssize_t nread = read(STDIN_FILENO, temp, PROMPT_BUFFER_SIZE - 1);

        if (nread < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }

        if (nread == 0) {
            // EOF: if we have data, we should probably process it even if no
            // newline. However, we must ensure we don't loop forever.
            if (cursor->session->exit_requested) {
                return false;
            }
            cursor->session->exit_requested = true;
            return cursor->data->length > 0;
        }

        temp[nread] = '\0';

        // Add any unprocessed input to the buffer
        Key   key         = keyboard_parse(temp);
        char *unprocessed = temp + key.read;

        if (cursor->session->terminal->is_visual) {
            cursor_insert(cursor, unprocessed);
        } else {
            dynamic_extend(cursor->data, unprocessed);
            cursor->visible_length += utf8_strlen(unprocessed);
        }

        if (key.type == KEY_ENTER || strchr(unprocessed, '\n') ||
            strchr(unprocessed, '\r')) {
            return true;
        }

        if (key.ctrl && key.value == 'c') {
            return false;
        }

        if (key.ctrl && key.value == 'd') {
            cursor->session->exit_requested = true;
            return false;
        }

        if (key.type == KEY_VALUE && key.value == 'l' && key.ctrl) {
            cursor_clear_screen(cursor);
            return false;
        }

        handle_key(key, cursor);
    }
    return true;
}

/* Clean up the command by removing trailing whitespace */
static void cleanup_cmd(char *command) {
    if (!command)
        return;

    size_t cmd_len = strlen(command);
    while (cmd_len > 0 &&
           (command[cmd_len - 1] == '\n' || command[cmd_len - 1] == '\r' ||
            command[cmd_len - 1] == ' ' || command[cmd_len - 1] == '\t')) {
        command[cmd_len - 1] = '\0';
        cmd_len--;
    }
}

char *prompt(char *prompt_str, char *continuation, Session *session,
             bool (*should_return)(char *input, Session *session)) {

    // Ensure history state is at the bottom (live prompt)
#ifndef TIDESH_DISABLE_HISTORY
    history_reset_state(session->history);
#endif

    terminal_setup(session);
    Cursor *cursor = init_cursor(NULL, session, prompt_str, continuation);
    if (!cursor) {
        terminal_restore(session);
        return NULL;
    }

    // Read input until should_return is true
    terminal_write_check_newline(prompt_str);

    bool success = read_until_enter(cursor);

    if (!success) {
        // Handle Ctrl+C or read error
        free_cursor(cursor);
        free(cursor);
        terminal_restore(session);
        return NULL;
    }

    if (cursor->session->terminal->is_visual) {
        cursor_append(cursor, '\n');
    } else {
        dynamic_append(cursor->data, '\n');
        cursor->visible_length += 1;
    }
    char *final = dynamic_to_string(cursor->data);

    char *continuation_prompt = terminal_newline_checked(continuation);

    while (!should_return(final, session)) {
        if (cursor->session->terminal->is_visual && continuation_prompt)
            terminal_write(continuation_prompt);

        success = read_until_enter(cursor);
        if (!success) {
            free_cursor(cursor);
            free(cursor);
            if (continuation_prompt)
                free(continuation_prompt);
            free(final);
            terminal_restore(session);
            return NULL;
        }

        if (cursor->session->terminal->is_visual) {
            cursor_append(cursor, '\n');
        } else {
            dynamic_append(cursor->data, '\n');
            cursor->visible_length += 1;
        }

        free(final); // Free old string
        final = dynamic_to_string(cursor->data);
    }

    free_cursor(cursor);
    if (cursor)
        free(cursor);
    if (continuation_prompt)
        free(continuation_prompt);

    terminal_restore(session);
    cleanup_cmd(final);

    // Reset history state again so next prompt starts clean
#ifndef TIDESH_DISABLE_HISTORY
    history_reset_state(session->history);
#endif

    return final;
}