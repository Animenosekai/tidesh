#include <stdarg.h>  /* va_list, va_start, va_end */
#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */
#include <stdio.h>   /* NULL */
#include <stdlib.h>  /* malloc */
#include <string.h>  /* strlen, strcmp, strstr */

#include "prompt/ansi.h"

#define MAX_APPLY_ANSI_CODES 64

const char *all_fg_colors[] = {
    ANSI_BLACK,         ANSI_RED,          ANSI_GREEN,
    ANSI_YELLOW,        ANSI_BLUE,         ANSI_MAGENTA,
    ANSI_CYAN,          ANSI_WHITE,        ANSI_DEFAULT,
    ANSI_BRIGHT_BLACK,  ANSI_BRIGHT_RED,   ANSI_BRIGHT_GREEN,
    ANSI_BRIGHT_YELLOW, ANSI_BRIGHT_BLUE,  ANSI_BRIGHT_MAGENTA,
    ANSI_BRIGHT_CYAN,   ANSI_BRIGHT_WHITE, NULL};

const char *all_bg_colors[] = {
    ANSI_BG_BLACK,         ANSI_BG_RED,          ANSI_BG_GREEN,
    ANSI_BG_YELLOW,        ANSI_BG_BLUE,         ANSI_BG_MAGENTA,
    ANSI_BG_CYAN,          ANSI_BG_WHITE,        ANSI_BG_DEFAULT,
    ANSI_BG_BRIGHT_BLACK,  ANSI_BG_BRIGHT_RED,   ANSI_BG_BRIGHT_GREEN,
    ANSI_BG_BRIGHT_YELLOW, ANSI_BG_BRIGHT_BLUE,  ANSI_BG_BRIGHT_MAGENTA,
    ANSI_BG_BRIGHT_CYAN,   ANSI_BG_BRIGHT_WHITE, NULL};

/* Extracts the numeric code from an ANSI escape sequence.
   Returns -1 if the sequence is invalid. */
static inline int ansi_extract_code(const char *s) {
    // Expect: \x1b[XYZm
    if (!s || s[0] != ANSI_ESCAPE[0] || s[1] != ANSI_CSI[0])
        return -1;

    int         value = 0;
    const char *p     = s + 2;

    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }

    if (*p != 'm')
        return -1;

    return value;
}

char *ansi_revert(const char *ansi_code) {
    int code = ansi_extract_code(ansi_code);
    if (code < 0)
        return NULL;

    switch (code) {
        // Graphics Mode
        case 1:
            return ANSI_BOLD_OFF;
        case 2:
            return ANSI_DIM_OFF;
        case 4:
            return ANSI_UNDERLINE_OFF;
        case 5:
            return ANSI_BLINK_OFF;
        case 7:
            return ANSI_REVERSE_OFF;
        case 8:
            return ANSI_HIDDEN_OFF;

        // Foreground Colors: revert to default
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 39:
            return ANSI_DEFAULT;

        // Background Colors: revert to default
        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        case 49:
            return ANSI_BG_DEFAULT;

        // Bright Foreground Colors: revert to default
        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
            return ANSI_DEFAULT;

        // Bright Background Colors: revert to default
        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
            return ANSI_BG_DEFAULT;

        default:
            return NULL;
    }
}

bool is_fg_bright_color_code(const char *ansi_code) {
    int code = ansi_extract_code(ansi_code);
    if (code < 0)
        return false;

    return (code >= 90 && code <= 97);
}

bool is_bg_bright_color_code(const char *ansi_code) {
    int code = ansi_extract_code(ansi_code);
    if (code < 0)
        return false;

    return (code >= 100 && code <= 107);
}

bool is_bright_color_code(const char *ansi_code) {
    return is_fg_bright_color_code(ansi_code) ||
           is_bg_bright_color_code(ansi_code);
}

bool is_fg_standard_color_code(const char *ansi_code) {
    int code = ansi_extract_code(ansi_code);
    if (code < 0)
        return false;

    return (code >= 30 && code <= 37);
}

bool is_bg_standard_color_code(const char *ansi_code) {
    int code = ansi_extract_code(ansi_code);
    if (code < 0)
        return false;

    return (code >= 40 && code <= 47);
}

bool is_standard_color_code(const char *ansi_code) {
    return is_fg_standard_color_code(ansi_code) ||
           is_bg_standard_color_code(ansi_code);
}

bool is_fg_color_code(const char *ansi_code) {
    return is_fg_standard_color_code(ansi_code) ||
           is_fg_bright_color_code(ansi_code);
}

bool is_bg_color_code(const char *ansi_code) {
    return is_bg_standard_color_code(ansi_code) ||
           is_bg_bright_color_code(ansi_code);
}

bool is_color_code(const char *ansi_code) {
    return is_fg_color_code(ansi_code) || is_bg_color_code(ansi_code);
}

/**
 * @brief Finds the last occurrence of a substring.
 * @return Pointer to the last occurrence, or NULL if not found.
 */
static const char *find_last_occurrence(const char *context, const char *code) {
    const char *last = NULL;
    const char *p    = context;
    if (!context || !code || !*code)
        return NULL;

    while ((p = strstr(p, code)) != NULL) {
        last = p;
        p++; // Move past the start of the found string
    }
    return last;
}

/**
 * @brief Finds the last active code from a given list in a context string.
 * An active code is one that appears after all relevant "reset" codes.
 *
 * @param context The string to search.
 * @param code_list A NULL-terminated list of codes to check.
 * @param default_code The specific default code for this type (e.g.,
 * ANSI_DEFAULT).
 * @param reset_code The general reset code (e.g., ANSI_COLOR_RESET).
 * @return The last active code, or NULL if none is active or only default is
 * active.
 */
static const char *find_active_code(const char *context, const char **code_list,
                                    const char *default_code,
                                    const char *reset_code) {
    if (!context)
        return NULL;

    const char *active_code     = NULL;
    const char *last_active_pos = NULL;

    // Find the position of the last resets
    const char *last_default_pos = find_last_occurrence(context, default_code);
    const char *last_reset_pos   = find_last_occurrence(context, reset_code);

    for (int i = 0; code_list[i]; i++) {
        const char *code        = code_list[i];
        const char *last_on_pos = find_last_occurrence(context, code);

        if (!last_on_pos)
            continue;

        // Does this code appear after its resets?
        bool is_active = true;
        if (last_default_pos && last_on_pos < last_default_pos)
            is_active = false;
        if (last_reset_pos && last_on_pos < last_reset_pos)
            is_active = false;

        if (!is_active)
            continue;

        // Is it the most recent active code?
        if (last_active_pos == NULL || last_on_pos > last_active_pos) {
            last_active_pos = last_on_pos;
            active_code     = code;
        }
    }

    // If the most recent "active" code is just the default,
    // then no color is active.
    if (active_code && strcmp(active_code, default_code) == 0) {
        return NULL;
    }

    return active_code;
}

bool ansi_is_active(const char *context, const char *code_to_check) {
    if (!context || !code_to_check) {
        return false;
    }

    const char *revert_code = ansi_revert(code_to_check);

    const char *last_on_pos = find_last_occurrence(context, code_to_check);
    if (!last_on_pos)
        return false;

    // Find the last revert specific to this code
    const char *last_revert_pos =
        (revert_code) ? find_last_occurrence(context, revert_code) : NULL;

    // Find the last general reset
    const char *last_reset_pos =
        find_last_occurrence(context, ANSI_COLOR_RESET);

    // If a specific revert was found after the on code
    if (last_revert_pos && last_revert_pos > last_on_pos) {
        return false;
    }

    // If a general reset was found after the on code
    if (last_reset_pos && last_reset_pos > last_on_pos) {
        return false;
    }

    return true;
}

char *ansi_apply(char *text, char *context, ...) {
    va_list     ap;
    const char *code;

    const char *codes_to_apply[MAX_APPLY_ANSI_CODES];
    const char *codes_to_revert[MAX_APPLY_ANSI_CODES];
    int         num_codes_to_apply  = 0;
    int         num_codes_to_revert = 0;

    size_t apply_len  = 0;
    size_t revert_len = 0;
    size_t text_len   = strlen(text);

    // Find the active colors in the context before applying new ones.
    const char *context_fg = find_active_code(context, all_fg_colors,
                                              ANSI_DEFAULT, ANSI_COLOR_RESET);
    const char *context_bg = find_active_code(
        context, all_bg_colors, ANSI_BG_DEFAULT, ANSI_COLOR_RESET);

    // Track if we apply a new color *in this function call*.
    int applied_new_fg = 0;
    int applied_new_bg = 0;

    // Collect codes and calculate lengths
    va_start(ap, context);
    while ((code = va_arg(ap, const char *)) != NULL) {
        if (num_codes_to_apply >= MAX_APPLY_ANSI_CODES)
            break;

        codes_to_apply[num_codes_to_apply++] = code;
        apply_len += strlen(code);

        // Track if this code is a new color
        if (is_fg_color_code(code) && strcmp(code, ANSI_DEFAULT) != 0) {
            applied_new_fg = 1;
        }
        if (is_bg_color_code(code) && strcmp(code, ANSI_BG_DEFAULT) != 0) {
            applied_new_bg = 1;
        }

        // Check if we are responsible for reverting this code
        if (!ansi_is_active(context, code)) {
            const char *revert_code = ansi_revert(code);
            if (revert_code && !is_color_code(revert_code)) {
                codes_to_revert[num_codes_to_revert++] = revert_code;
                revert_len += strlen(revert_code);
            }
        }
    }
    va_end(ap);

    // Now, add the correct color reverts.
    if (applied_new_fg) {
        // We applied a new FG color. Revert to the context's FG, or default.
        const char *revert_code = context_fg ? context_fg : ANSI_DEFAULT;
        codes_to_revert[num_codes_to_revert++] = revert_code;
        revert_len += strlen(revert_code);
    }
    if (applied_new_bg) {
        // We applied a new BG color. Revert to the context's BG, or default.
        const char *revert_code = context_bg ? context_bg : ANSI_BG_DEFAULT;
        codes_to_revert[num_codes_to_revert++] = revert_code;
        revert_len += strlen(revert_code);
    }

    // Collect codes and calculate lengths
    size_t total_len = apply_len + text_len + revert_len;
    char  *result    = malloc(total_len + 1); // +1 for the null terminator
    if (!result)
        return NULL;

    char *ptr = result;

    // Copy all "apply" codes
    for (int i = 0; i < num_codes_to_apply; i++) {
        size_t len = strlen(codes_to_apply[i]);
        memcpy(ptr, codes_to_apply[i], len);
        ptr += len;
    }

    // Copy the main text
    memcpy(ptr, text, text_len);
    ptr += text_len;

    // Copy all "revert" codes in reverse order
    for (int i = num_codes_to_revert - 1; i >= 0; i--) {
        size_t len = strlen(codes_to_revert[i]);
        memcpy(ptr, codes_to_revert[i], len);
        ptr += len;
    }

    // Add the null terminator
    *ptr = '\0';

    return result;
}

/* Skips over a single ANSI escape sequence if `s` points to one. */
static const char *ansi_skip_sequence(const char *s) {
    if (s[0] != ANSI_ESCAPE[0])
        return s;

    if (s[1] == ANSI_CSI[0]) {
        const char *p = s + 2;
        while ((*p >= '0' && *p <= '9') || *p == ';')
            p++;

        if (*p >= '@' && *p <= '~') // Final byte of CSI
            return p + 1;
        else
            return s;         // Invalid sequence
    } else if (s[1] == 'M') { // Single char command
        return s + 2;
    }
    return s;
}

size_t ansi_strlen(const char *s) {
    size_t      len = 0;
    const char *p   = s;

    if (!s)
        return 0;

    while (*p != '\0') {
        const char *next_p = ansi_skip_sequence(p);
        if (next_p > p) {
            p = next_p; // Skip ANSI sequence
        } else {
            len++;
            p++; // Move to next character
        }
    }
    return len;
}

char *ansi_strip(const char *s) {
    if (!s)
        return NULL;

    size_t      len = 0;
    const char *p   = s;

    size_t stripped_length = ansi_strlen(s);

    // Allocate memory for stripped string
    char *stripped = malloc(stripped_length + 1); // +1 for null terminator
    if (!stripped)
        return NULL;

    // Copy
    p          = s;
    char *dest = stripped;
    while (*p != '\0') {
        const char *next_p = ansi_skip_sequence(p);
        if (next_p > p) {
            p = next_p; // Skip ANSI sequence
        } else {
            *dest++ = *p++; // Copy visible character
        }
    }
    *dest = '\0';

    return stripped;
}

const char *ansi_next_char(const char *s) {
    if (!s || *s == '\0') {
        return s;
    }

    const char *p = s + 1; // Move past the current char
    while (1) {
        const char *next_p = ansi_skip_sequence(p);
        if (next_p > p) {
            p = next_p;
        } else {
            break;
        }
    }
    return p;
}

const char *ansi_prev_char(const char *base, const char *s) {
    if (!base || !s || s <= base) {
        return base;
    }

    const char *prev_visible = NULL;
    const char *p            = base;

    while (*p != '\0' && p < s) {
        const char *skipped_p = ansi_skip_sequence(p);

        if (skipped_p > p) {
            p = skipped_p;
        } else {
            prev_visible = p; // Record it
            p++;
        }
    }

    return (prev_visible) ? prev_visible : base;
}
