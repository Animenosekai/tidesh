/** terminal.h
 *
 * Declarations for terminal handling.
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#define TERMINAL_DEFAULT_ROWS 24
#define TERMINAL_DEFAULT_COLS 80

#include <stdbool.h>
#include <stddef.h>
#include <termios.h>

typedef struct Session Session;

typedef struct Terminal {
    size_t         rows;            // Number of terminal rows
    size_t         cols;            // Number of terminal columns
    bool           supports_colors; // Does terminal support colors
    bool           is_visual;       // Is terminal visual
    bool           is_raw;          // Is terminal in raw mode
    struct termios orig_termios;    // Original terminal settings
} Terminal;

/** Initialize terminal with default environment
 *
 * @param terminal Pointer to Terminal to initialize, or NULL to allocate a new
 * one
 * @return Pointer to initialized Terminal, or NULL on failure
 */
Terminal *init_terminal(Terminal *terminal, Session *session);

/** Restore terminal to original settings
 *
 * @param session Pointer to Session containing Terminal to restore
 */
void terminal_restore(Session *session);

/** Setup terminal for raw mode input
 *
 * @param session Pointer to Session containing Terminal to setup
 * @return true on success, false on failure
 */
bool terminal_setup(Session *session);

/**
 * Write data to the terminal
 *
 * @param data The data to write
 */
void terminal_write(const char *data);

/**
 * Write sized data to the terminal
 *
 * @param data The data to write
 * @param size The size of the data to write
 */
void terminal_write_sized(const char *data, size_t size);

/**
 * Converts newlines as needed in data for terminal output.
 *
 * @param data The data to write
 * @return Newly allocated string with newlines converted, or NULL on failure
 */
char *terminal_newline_checked(const char *data);

/**
 * Write data to the terminal, converting newlines as needed
 *
 * @param data The data to write
 */
void terminal_write_check_newline(const char *data);

/**
 * Write formatted data to the terminal
 *
 * @param pattern The format string
 * @param ... The values to format
 */
void terminal_write_pattern(const char *pattern, ...);

/* Clear the terminal screen */
void terminal_clear_screen(void);

/**
 * Check and handle terminal resize
 *
 * @param session Pointer to Session containing Terminal to check
 * @return true if resized, false otherwise
 */
bool terminal_check_resize(Session *session);

/**
 * Move cursor to specified row and column
 *
 * @param row The row to move to (0-based)
 * @param col The column to move to (0-based)
 */
void terminal_move_cursor(int row, int col);

/**
 * Move cursor up by n lines
 *
 * @param n Number of lines to move up
 */
void terminal_cursor_up(int n);

/**
 * Move cursor down by n lines
 *
 * @param n Number of lines to move down
 */
void terminal_cursor_down(int n);

/**
 * Move cursor forward by n columns
 *
 * @param n Number of columns to move forward
 */
void terminal_cursor_forward(int n);

/**
 * Move cursor backward by n columns
 *
 * @param n Number of columns to move backward
 */
void terminal_cursor_backward(int n);

/* Save cursor position */
void terminal_save_cursor(void);

/* Restore cursor position */
void terminal_restore_cursor(void);

/* Hide cursor */
void terminal_hide_cursor(void);

/* Show cursor */
void terminal_show_cursor(void);

/* Clear from cursor to end of screen */
void terminal_clear_to_end(void);

/* Move cursor to start of line */
void terminal_cursor_to_column(int col);

/* Clear current line */
void terminal_clear_line(void);

/**
 * Free resources used by a Terminal structure
 *
 * @param terminal Pointer to Terminal to free
 */
void free_terminal(Terminal *terminal, Session *session);

#endif /* TERMINAL_H */
