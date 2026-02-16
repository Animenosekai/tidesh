#include "builtins/clear.h"
#include "prompt/terminal.h" /* terminal_clear_screen */
#include "session.h"         /* Session */

int builtin_clear(int argc, char **argv, Session *session) {
    terminal_clear_screen();
    return 0;
}
