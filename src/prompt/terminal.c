/* Terminal control implementation */

#include <signal.h>    /* sigaction, sigemptyset, sig_atomic_t */
#include <stdarg.h>    /* va_list, va_start, va_end */
#include <stdio.h>     /* vsnprintf */
#include <stdlib.h>    /* malloc, free, atexit */
#include <string.h>    /* strlen, strcmp, strstr */
#include <sys/ioctl.h> /* ioctl, TIOCGWINSZ */
#include <termios.h>   /* termios, tcgetattr, tcsetattr, TCSAFLUSH */
#include <unistd.h>    /* STDOUT_FILENO, STDIN_FILENO, isatty */

#include "environ.h"     /* Environ, environ_get */
#include "prompt/ansi.h" /* ANSI_* */
#include "prompt/terminal.h"
#include "session.h" /* Session */

#define ANSI_CODE_BUFFER_SIZE 32

// Signal flags
static volatile sig_atomic_t g_needs_resize = 0; // Resize needed flag

// Handling multiple sessions
static struct Sessions {
    Session         *session; // The session
    struct Sessions *next;    // Next in list
} g_sessions = {NULL, NULL};

static void register_session(Session *session) {
    struct Sessions *new_entry = malloc(sizeof(struct Sessions));
    if (!new_entry)
        return;
    new_entry->session = session;
    new_entry->next    = g_sessions.next;
    g_sessions.next    = new_entry;
}

static void unregister_session(Session *session) {
    if (!session)
        return;

    struct Sessions *prev = &g_sessions;
    struct Sessions *curr = g_sessions.next;

    while (curr) {
        if (curr->session == session) {
            prev->next = curr->next;
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

static void cleanup_session(Session *session) {
    if (!session)
        return;

    terminal_restore(session);
    unregister_session(session);
}

static void cleanup_sessions(void) {
    struct Sessions *curr = g_sessions.next;
    while (curr) {
        struct Sessions *next = curr->next;
        cleanup_session(curr->session);
        curr = next;
    }
    g_sessions.next = NULL;
    terminal_show_cursor();
}

// Helpers

/* Handle window resize signal */
static void handle_sigwinch(int sig) {
    (void)sig;
    g_needs_resize = 1;
}

/* Update terminal size */
static void update_terminal_size(Session *session) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
        session->terminal->rows = ws.ws_row;
        session->terminal->cols = ws.ws_col;
    }
}

/* Detect if terminal supports colors */
static bool detect_color_support(Environ *env) {
    const char *term = environ_get(env, "TERM");
    if (!term)
        return false;

    // Common terminals that support colors
    const char *color_terms[] = {
        "xterm", "xterm-ghostty", "xterm-kitty", "tmux",
        "rxvt",  "linux",         "cygwin",      NULL};

    for (size_t i = 0; color_terms[i] != NULL; i++) {
        if (strcmp(term, color_terms[i]) == 0)
            return true;
    }

    const char *color_term_instances[] = {"color", "256color", "truecolor",
                                          NULL};

    for (size_t i = 0; color_term_instances[i] != NULL; i++) {
        if (strstr(term, color_term_instances[i]) != NULL)
            return true;
    }

    return false;
}

/* Check if we're running in a visual terminal */
static bool is_visual_terminal(Environ *env) {
    const char *term = environ_get(env, "TERM");
    if (!term)
        return false;

    // Check for common non-visual terminals
    if (strcmp(term, "dumb") == 0)
        return false;

    // Check if stdout is a tty
    if (!isatty(STDOUT_FILENO))
        return false;

    return true;
}

// Initializers

Terminal *init_terminal(Terminal *terminal, Session *session) {
    if (!session) {
        return NULL;
    }

    if (!terminal) {
        terminal = malloc(sizeof(Terminal));
        if (!terminal) {
            return NULL;
        }
    }

    session->terminal = terminal;
    atexit(cleanup_sessions);
    register_session(session);

    terminal->is_raw          = false;
    terminal->is_visual       = is_visual_terminal(session->environ);
    terminal->supports_colors = detect_color_support(session->environ);
    terminal->rows            = TERMINAL_DEFAULT_ROWS; // Default rows
    terminal->cols            = TERMINAL_DEFAULT_COLS; // Default cols

    // Get terminal size
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
        terminal->rows = ws.ws_row;
        terminal->cols = ws.ws_col;
    }

    // Setup signal handler for window resize
    struct sigaction sa;
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    return terminal;
}

// Setups
void terminal_restore(Session *session) {
    if (!session->terminal->is_raw)
        return;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &session->terminal->orig_termios);
    terminal_show_cursor();
    session->terminal->is_raw = false;
}

bool terminal_setup(Session *session) {
    if (session->terminal->is_raw)
        return true;

    // Check if we have a terminal
    if (!isatty(STDIN_FILENO))
        return false;

    // Get original terminal attributes
    if (tcgetattr(STDIN_FILENO, &session->terminal->orig_termios) == -1) {
        return false;
    }

    // Enable raw mode
    struct termios raw = session->terminal->orig_termios;

    // Input flags: disable various preprocessing
    raw.c_iflag &=
        ~(BRKINT   // Disable SIGINT on break condition
          | ICRNL  // Disable CR-to-NL translation (Enter gives \r, not \n)
          | INPCK  // Disable parity checking
          | ISTRIP // Disable stripping of 8th bit
          | IXON); // Disable XON/XOFF software flow control (^S / ^Q)

    // Output flags
    raw.c_oflag &=
        ~(OPOST); // Disable all output processing (no automatic \n -> \r\n)

    // Control flags
    raw.c_cflag |= (CS8); // Set character size to 8 bits per byte

    // Local flags: disable canonical mode and signals
    raw.c_lflag &=
        ~(ECHO     // Disable echoing of typed characters
          | ICANON // Disable canonical mode (read input byte-by-byte)
          | IEXTEN // Disable implementation-defined input processing
          | ISIG); // Disable signal-generating chars (Ctrl-C, Ctrl-Z, etc.)

    // Control chars
    raw.c_cc[VMIN]  = 1; // Read returns when at least 1 byte is available
    raw.c_cc[VTIME] = 0; // No timeout (blocking read)

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return false;
    }

    session->terminal->is_raw = true;
    return true;
}

// Operations

inline void terminal_write(const char *data) {
    write(STDOUT_FILENO, data, strlen(data));
    // Ensure the data is flushed immediately
    fflush(stdout);
}

inline void terminal_write_sized(const char *data, size_t size) {
    write(STDOUT_FILENO, data, size);
    // Ensure the data is flushed immediately
    fflush(stdout);
}

char *terminal_newline_checked(const char *data) {
    if (!data)
        return NULL;

    size_t len      = strlen(data);
    size_t newlines = 0;

    // Count newlines
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            newlines++;
        }
    }

    // If no newlines, simply duplicate the string
    if (newlines == 0) {
        return strdup(data);
    }

    // original len + 1 extra byte per newline + null
    // terminator
    char *buffer = malloc(len + newlines + 1);
    if (!buffer)
        return NULL;

    // Fill the new buffer
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            buffer[j++] = '\r';
            buffer[j++] = '\n';
        } else {
            buffer[j++] = data[i];
        }
    }
    buffer[j] = '\0'; // Ensure null termination

    return buffer;
}

inline void terminal_write_check_newline(const char *data) {
    char *buffer = terminal_newline_checked(data);
    if (!buffer)
        return;

    terminal_write(buffer);
    free(buffer);
}

inline void terminal_write_pattern(const char *pattern, ...) {
    char buffer[ANSI_CODE_BUFFER_SIZE];

    va_list args;
    va_start(args, pattern);
    vsnprintf(buffer, sizeof(buffer), pattern, args);
    va_end(args);

    terminal_write_check_newline(buffer);
}

inline void terminal_clear_screen(void) {
    terminal_write(ANSI_ERASE_ENTIRE_DISPLAY);
    terminal_write(ANSI_CURSOR_HOME);
}

inline bool terminal_check_resize(Session *session) {
    if (g_needs_resize) {
        g_needs_resize = 0;
        update_terminal_size(session);
        return true;
    }
    return false;
}

void terminal_move_cursor(int row, int col) {
    if (row < 0 || col < 0)
        return;
    terminal_write_pattern(ANSI_CURSOR_MOVE_PATTERN, row + 1, col + 1);
}

void terminal_cursor_up(int n) {
    if (n == 0)
        return;
    if (n < 0)
        return terminal_cursor_down(-n);
    if (n == 1) {
        // Handles scrolling automatically
        terminal_write(ANSI_CURSOR_MOVE_ONE_UP);
        return;
    }
    terminal_write_pattern(ANSI_CURSOR_MOVE_UP_PATTERN, n);
}

void terminal_cursor_down(int n) {
    if (n == 0)
        return;
    if (n < 0)
        return terminal_cursor_up(-n);
    terminal_write_pattern(ANSI_CURSOR_MOVE_DOWN_PATTERN, n);
}

void terminal_cursor_forward(int n) {
    if (n == 0)
        return;
    if (n < 0)
        return terminal_cursor_backward(-n);
    terminal_write_pattern(ANSI_CURSOR_MOVE_FORWARD_PATTERN, n);
}

void terminal_cursor_backward(int n) {
    if (n == 0)
        return;
    if (n < 0)
        return terminal_cursor_forward(-n);
    terminal_write_pattern(ANSI_CURSOR_MOVE_BACKWARD_PATTERN, n);
}

inline void terminal_save_cursor(void) {
    terminal_write(ANSI_CURSOR_SAVE_POSITION);
}

inline void terminal_restore_cursor(void) {
    terminal_write(ANSI_CURSOR_RESTORE_POSITION);
}

inline void terminal_hide_cursor(void) {
    terminal_write(ANSI_PRIVATE_CURSOR_INVISIBLE);
}

inline void terminal_show_cursor(void) {
    terminal_write(ANSI_PRIVATE_CURSOR_VISIBLE);
}

inline void terminal_clear_to_end(void) {
    terminal_write(ANSI_ERASE_CURSOR_TO_EOF);
}

void terminal_cursor_to_column(int col) {
    if (col < 0)
        return;
    terminal_write_pattern(ANSI_CURSOR_MOVE_COLUMN_PATTERN, col + 1);
}

inline void terminal_clear_line(void) {
    terminal_write(ANSI_ERASE_ENTIRE_LINE);
}

void free_terminal(Terminal *terminal, Session *session) {
    cleanup_session(session);

    if (!terminal)
        return;

    // Currently no dynamic resources in Terminal
}