#include <stdarg.h> /* va_list, va_start, va_end */
#include <stdio.h>  /* vsnprintf */

#include "prompt/ansi.h" /* ANSI_* */
#include "prompt/debug.h"
#include "prompt/terminal.h" /* terminal_write, terminal_write_pattern */

#define DEBUG_BUFFER_SIZE 128

void prompt_debug(const char *message, ...) {
    terminal_write(ANSI_CURSOR_SAVE_POSITION);
    terminal_write(ANSI_CURSOR_HOME);
    terminal_write(ANSI_ERASE_ENTIRE_LINE);
    char buffer[DEBUG_BUFFER_SIZE];

    va_list args;
    va_start(args, message);
    vsnprintf(buffer, sizeof(buffer), message, args);
    va_end(args);

    terminal_write_check_newline(buffer);
    terminal_write(ANSI_CURSOR_RESTORE_POSITION);
}