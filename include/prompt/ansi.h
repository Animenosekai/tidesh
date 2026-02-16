/** ansi.h
 *
 * ANSI escape codes for terminal control
 */

#ifndef PROMPT_ANSI_H
#define PROMPT_ANSI_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* For size_t */

#define ANSI_ESCAPE "\x1b"
#define ANSI_CSI "["
#define ANSI_DCS "P"
#define ANSI_OSC "]"

// Single Character Commands
#define ANSI_CURSOR_MOVE_ONE_UP ANSI_ESCAPE "M"

// Control Sequence Introducer

/// Cursor control
#define ANSI_CURSOR_HOME ANSI_ESCAPE ANSI_CSI "H"
#define ANSI_CURSOR_MOVE(row, col) ANSI_ESCAPE ANSI_CSI #row ";" #col "H"
#define ANSI_CURSOR_MOVE_PATTERN ANSI_ESCAPE ANSI_CSI "%d;%dH"

#define ANSI_CURSOR_MOVE_UP(n) ANSI_ESCAPE ANSI_CSI #n "A"
#define ANSI_CURSOR_MOVE_DOWN(n) ANSI_ESCAPE ANSI_CSI #n "B"
#define ANSI_CURSOR_MOVE_FORWARD(n) ANSI_ESCAPE ANSI_CSI #n "C"
#define ANSI_CURSOR_MOVE_BACKWARD(n) ANSI_ESCAPE ANSI_CSI #n "D"
#define ANSI_CURSOR_MOVE_BEGINNING_LINE_DOWN(n) ANSI_ESCAPE ANSI_CSI #n "E"
#define ANSI_CURSOR_MOVE_BEGINNING_LINE_UP(n) ANSI_ESCAPE ANSI_CSI #n "F"
#define ANSI_CURSOR_MOVE_COLUMN(n) ANSI_ESCAPE ANSI_CSI #n "G"

#define ANSI_CURSOR_MOVE_UP_PATTERN ANSI_ESCAPE ANSI_CSI "%dA"
#define ANSI_CURSOR_MOVE_DOWN_PATTERN ANSI_ESCAPE ANSI_CSI "%dB"
#define ANSI_CURSOR_MOVE_FORWARD_PATTERN ANSI_ESCAPE ANSI_CSI "%dC"
#define ANSI_CURSOR_MOVE_BACKWARD_PATTERN ANSI_ESCAPE ANSI_CSI "%dD"
#define ANSI_CURSOR_MOVE_BEGINNING_LINE_DOWN_PATTERN ANSI_ESCAPE ANSI_CSI "%dE"
#define ANSI_CURSOR_MOVE_BEGINNING_LINE_UP_PATTERN ANSI_ESCAPE ANSI_CSI "%dF"
#define ANSI_CURSOR_MOVE_COLUMN_PATTERN ANSI_ESCAPE ANSI_CSI "%dG"

#define ANSI_CURSOR_SAVE_POSITION ANSI_ESCAPE ANSI_CSI "s"
#define ANSI_CURSOR_RESTORE_POSITION ANSI_ESCAPE ANSI_CSI "u"

/// Erase functions
#define ANSI_ERASE_IN_DISPLAY(mode) ANSI_ESCAPE ANSI_CSI #mode "J"
#define ANSI_ERASE_IN_LINE(mode) ANSI_ESCAPE ANSI_CSI #mode "K"

//// In Display
#define ANSI_ERASE_CURSOR_TO_EOF ANSI_ERASE_IN_DISPLAY(0)
#define ANSI_ERASE_CURSOR_TO_SOF ANSI_ERASE_IN_DISPLAY(1)
#define ANSI_ERASE_ENTIRE_DISPLAY ANSI_ERASE_IN_DISPLAY(2)

//// In Line
#define ANSI_ERASE_CURSOR_TO_EOL ANSI_ERASE_IN_LINE(0)
#define ANSI_ERASE_CURSOR_TO_SOL ANSI_ERASE_IN_LINE(1)
#define ANSI_ERASE_ENTIRE_LINE ANSI_ERASE_IN_LINE(2)

/// Text attributes
#define ANSI_COLOR(color_code) ANSI_ESCAPE ANSI_CSI #color_code "m"
#define ANSI_COLOR_RESET ANSI_COLOR(0)

//// Graphics Mode
#define ANSI_BOLD ANSI_COLOR(1)
#define ANSI_DIM ANSI_COLOR(2)
#define ANSI_ITALIC ANSI_COLOR(3)
#define ANSI_UNDERLINE ANSI_COLOR(4)
#define ANSI_BLINK ANSI_COLOR(5)
#define ANSI_REVERSE ANSI_COLOR(7)
#define ANSI_HIDDEN ANSI_COLOR(8)

///// Reset
#define ANSI_BOLD_OFF ANSI_COLOR(22)
#define ANSI_DIM_OFF ANSI_COLOR(22)
#define ANSI_UNDERLINE_OFF ANSI_COLOR(24)
#define ANSI_BLINK_OFF ANSI_COLOR(25)
#define ANSI_REVERSE_OFF ANSI_COLOR(27)
#define ANSI_HIDDEN_OFF ANSI_COLOR(28)

/// Foreground Colors
#define ANSI_BLACK ANSI_COLOR(30)
#define ANSI_RED ANSI_COLOR(31)
#define ANSI_GREEN ANSI_COLOR(32)
#define ANSI_YELLOW ANSI_COLOR(33)
#define ANSI_BLUE ANSI_COLOR(34)
#define ANSI_MAGENTA ANSI_COLOR(35)
#define ANSI_CYAN ANSI_COLOR(36)
#define ANSI_WHITE ANSI_COLOR(37)
#define ANSI_DEFAULT ANSI_COLOR(39)

/// Background Colors
#define ANSI_BG_BLACK ANSI_COLOR(40)
#define ANSI_BG_RED ANSI_COLOR(41)
#define ANSI_BG_GREEN ANSI_COLOR(42)
#define ANSI_BG_YELLOW ANSI_COLOR(43)
#define ANSI_BG_BLUE ANSI_COLOR(44)
#define ANSI_BG_MAGENTA ANSI_COLOR(45)
#define ANSI_BG_CYAN ANSI_COLOR(46)
#define ANSI_BG_WHITE ANSI_COLOR(47)
#define ANSI_BG_DEFAULT ANSI_COLOR(49)

/// Bright Foreground Colors
#define ANSI_BRIGHT_BLACK ANSI_COLOR(90)
#define ANSI_BRIGHT_RED ANSI_COLOR(91)
#define ANSI_BRIGHT_GREEN ANSI_COLOR(92)
#define ANSI_BRIGHT_YELLOW ANSI_COLOR(93)
#define ANSI_BRIGHT_BLUE ANSI_COLOR(94)
#define ANSI_BRIGHT_MAGENTA ANSI_COLOR(95)
#define ANSI_BRIGHT_CYAN ANSI_COLOR(96)
#define ANSI_BRIGHT_WHITE ANSI_COLOR(97)

/// Bright Background Colors
#define ANSI_BG_BRIGHT_BLACK ANSI_COLOR(100)
#define ANSI_BG_BRIGHT_RED ANSI_COLOR(101)
#define ANSI_BG_BRIGHT_GREEN ANSI_COLOR(102)
#define ANSI_BG_BRIGHT_YELLOW ANSI_COLOR(103)
#define ANSI_BG_BRIGHT_BLUE ANSI_COLOR(104)
#define ANSI_BG_BRIGHT_MAGENTA ANSI_COLOR(105)
#define ANSI_BG_BRIGHT_CYAN ANSI_COLOR(106)
#define ANSI_BG_BRIGHT_WHITE ANSI_COLOR(107)

// Private modes
#define ANSI_PRIVATE_MODE_SET(mode) ANSI_ESCAPE ANSI_CSI "?" #mode "h"
#define ANSI_PRIVATE_MODE_RESET(mode) ANSI_ESCAPE ANSI_CSI "?" #mode "l"
#define ANSI_PRIVATE_CURSOR_INVISIBLE ANSI_PRIVATE_MODE_RESET(25)
#define ANSI_PRIVATE_CURSOR_VISIBLE ANSI_PRIVATE_MODE_SET(25)
#define ANSI_PRIVATE_RESTORE_SCREEN ANSI_PRIVATE_MODE_RESET(47)
#define ANSI_PRIVATE_SAVE_SCREEN ANSI_PRIVATE_MODE_SET(47)
#define ANSI_PRIVATE_ALTERNATIVE_BUFFER ANSI_PRIVATE_MODE_SET(1049)
#define ANSI_PRIVATE_NORMAL_BUFFER ANSI_PRIVATE_MODE_RESET(1049)

/* Arrays of all color codes. NULL terminated */
extern const char *all_fg_colors[];

/* Arrays of all background color codes. NULL terminated */
extern const char *all_bg_colors[];

/**
 * Checks if the given ANSI code is a bright foreground color code.
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a bright foreground color code, false otherwise.
 */
bool is_fg_bright_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a bright background color code.
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a bright background color code, false otherwise.
 */
bool is_bg_bright_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a bright color code (foreground or
 * background).
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a bright color code, false otherwise.
 */
bool is_bright_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a standard foreground color code.
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a standard foreground color code, false otherwise.
 */
bool is_fg_standard_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a standard background color code.
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a standard background color code, false otherwise.
 */
bool is_bg_standard_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a standard color code (foreground or
 * background).
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a standard color code, false otherwise.
 */
bool is_standard_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a foreground color code (standard or
 * bright).
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a foreground color code, false otherwise.
 */
bool is_fg_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a background color code (standard or
 * bright).
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a background color code, false otherwise.
 */
bool is_bg_color_code(const char *ansi_code);

/**
 * Checks if the given ANSI code is a color code (foreground or background,
 * standard or bright).
 *
 * @param ansi_code The ANSI code to check.
 * @return true if it is a color code, false otherwise.
 */
bool is_color_code(const char *ansi_code);

/**
 * The revert code for a given ANSI code.
 *
 * @param ansi_code The ANSI code to revert.
 * @return The ANSI code that reverts the effect of the given code.
 */
char *ansi_revert(const char *ansi_code);

/**
 * Checks if a given ANSI code is active at the end of the given context string.
 *
 * @param context The context string containing ANSI codes.
 * @param code_to_check The ANSI code to check for activity.
 * @return true if the code is active, false otherwise.
 */
bool ansi_is_active(const char *context, const char *code_to_check);

/**
 * Apply ANSI codes to the given text, reverting them at the end if possible.
 *
 * @param text The text to apply ANSI codes to.
 * @param context The context in which to apply the ANSI codes.
 * @param ... The ANSI codes to apply.
 * @return The text with ANSI codes applied.
 */
char *ansi_apply(char *text, char *context, ...);

/**
 * Calculates the visible length of a string, excluding ANSI escape codes.
 *
 * @param s The string to measure.
 * @return The number of visible characters.
 */
size_t ansi_strlen(const char *s);

/**
 * Strips all ANSI escape codes from the given string.
 *
 * @param s The string to strip.
 * @return A new string with all ANSI codes removed. Caller is responsible
 * for freeing the returned string.
 */
char *ansi_strip(const char *s);

/**
 * Returns a pointer to the next visible character in the string,
 * skipping any intermediate ANSI escape codes.
 *
 * @param s Pointer to the current position in the string.
 * @return Pointer to the next visible character, or to the null
 * terminator if at the end.
 */
const char *ansi_next_char(const char *s);

/**
 * Returns a pointer to the previous visible character in the string,
 * starting from `s` and not going past `base`.
 *
 * @param base The start of the string (boundary).
 * @param s Pointer to the current position in the string.
 * @return Pointer to the previous visible character, or `base` if
 * no previous visible character is found and `s` is not `base`.
 */
const char *ansi_prev_char(const char *base, const char *s);

#endif /* PROMPT_ANSI_H */
