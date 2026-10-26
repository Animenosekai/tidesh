#include "data/utf8.h"

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */
#include <stdint.h>  /* uint32_t */

#if defined(__unix__) || defined(__APPLE__)
#define _XOPEN_SOURCE 700 // Modern POSIX compliance
#include <wchar.h>        /* wcwidth, wchar_t */
#endif

/* Get UTF-8 character length from first byte (1-4 bytes).
 * Invalid lead bytes (e.g. overlong C0/C1, out-of-range F5..FF, or continuation
 * bytes) return 1 as a safe fallback. */
unsigned char utf8_charlen(char c) {
    unsigned char u = (unsigned char)c;
    if (u < 0x80) {
        return 1;
    }
    if (u >= 0xC2 && u <= 0xDF) {
        return 2;
    }
    if (u >= 0xE0 && u <= 0xEF) {
        return 3;
    }
    if (u >= 0xF0 && u <= 0xF4) {
        return 4;
    }
    return 1; // Invalid, treat as single byte
}

/**
 * Decode one UTF-8 character at `str` and return its Unicode code point.
 * Also sets `*out_len` to the byte length consumed (1 to 4, or 0 at string
 * end). If the sequence is malformed or truncated, safely sets `*out_len = 1`
 * and returns the replacement character (U+FFFD), avoiding reading past the
 * null terminator.
 */
static uint32_t utf8_decode_char(const char *str, unsigned char *out_len) {
    if (!str || *str == '\0') {
        if (out_len) {
            *out_len = 0;
        }
        return 0;
    }

    unsigned char c0 = (unsigned char)str[0];
    if (c0 < 0x80) {
        if (out_len) {
            *out_len = 1;
        }
        return c0;
    }

    if (str[1] == '\0') {
        if (out_len) {
            *out_len = 1;
        }
        return 0xFFFD;
    }

    unsigned char c1 = (unsigned char)str[1];
    if (c0 >= 0xC2 && c0 <= 0xDF) {
        if ((c1 & 0xC0) == 0x80) {
            if (out_len) {
                *out_len = 2;
            }
            return ((uint32_t)(c0 & 0x1F) << 6) | (c1 & 0x3F);
        }
        if (out_len) {
            *out_len = 1;
        }
        return 0xFFFD;
    }

    if (str[2] == '\0') {
        if (out_len) {
            *out_len = 1;
        }
        return 0xFFFD;
    }

    unsigned char c2 = (unsigned char)str[2];
    if (c0 >= 0xE0 && c0 <= 0xEF) {
        if ((c2 & 0xC0) == 0x80) {
            bool valid = false;
            if (c0 == 0xE0 && c1 >= 0xA0 && c1 <= 0xBF) {
                valid = true;
            } else if (c0 >= 0xE1 && c0 <= 0xEC && (c1 & 0xC0) == 0x80) {
                valid = true;
            } else if (c0 == 0xED && c1 >= 0x80 && c1 <= 0x9F) {
                valid = true;
            } else if (c0 >= 0xEE && c0 <= 0xEF && (c1 & 0xC0) == 0x80) {
                valid = true;
            }

            if (valid) {
                if (out_len) {
                    *out_len = 3;
                }
                return ((uint32_t)(c0 & 0x0F) << 12) |
                       ((uint32_t)(c1 & 0x3F) << 6) | (c2 & 0x3F);
            }
        }
        if (out_len) {
            *out_len = 1;
        }
        return 0xFFFD;
    }

    if (str[3] == '\0') {
        if (out_len) {
            *out_len = 1;
        }
        return 0xFFFD;
    }

    unsigned char c3 = (unsigned char)str[3];
    if (c0 >= 0xF0 && c0 <= 0xF4) {
        if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
            bool valid = false;
            if (c0 == 0xF0 && c1 >= 0x90 && c1 <= 0xBF) {
                valid = true;
            } else if (c0 >= 0xF1 && c0 <= 0xF3 && (c1 & 0xC0) == 0x80) {
                valid = true;
            } else if (c0 == 0xF4 && c1 >= 0x80 && c1 <= 0x8F) {
                valid = true;
            }

            if (valid) {
                if (out_len) {
                    *out_len = 4;
                }
                return ((uint32_t)(c0 & 0x07) << 18) |
                       ((uint32_t)(c1 & 0x3F) << 12) |
                       ((uint32_t)(c2 & 0x3F) << 6) | (c3 & 0x3F);
            }
        }
        if (out_len) {
            *out_len = 1;
        }
        return 0xFFFD;
    }

    if (out_len) {
        *out_len = 1;
    }
    return 0xFFFD;
}

size_t utf8_strlen(const char *str) {
    if (!str) {
        return 0;
    }

    size_t len = 0;
    while (*str) {
        unsigned char char_len = 0;
        utf8_decode_char(str, &char_len);
        if (char_len == 0) {
            break;
        }
        str += char_len;
        len++;
    }
    return len;
}

char *utf8_next_char(char *str) {
    if (!str || *str == '\0') {
        return str;
    }

    unsigned char char_len = 0;
    utf8_decode_char(str, &char_len);
    return str + char_len;
}

char *utf8_prev_char(char *current, char *start) {
    if (!start || !current || current <= start) {
        return NULL; // Can't move back from the start
    }

    char *p = current - 1;

    // If the preceding byte is not a continuation byte, it's a single byte
    // (ASCII or invalid)
    if (((unsigned char)*p & 0xC0) != 0x80) {
        return p;
    }

    // It's a continuation byte. Step back over continuation bytes (at most 3)
    int count = 0;
    while (p > start && ((unsigned char)*p & 0xC0) == 0x80 && count < 3) {
        p--;
        count++;
    }

    // Check if p is a valid lead byte for a sequence that reaches exactly
    // `current`
    unsigned char seq_len = 0;
    utf8_decode_char(p, &seq_len);
    if (seq_len > 1 && p + seq_len == current) {
        return p;
    }

    // If no valid UTF-8 sequence ended at current (e.g. orphaned continuation
    // byte), safely move back by 1 byte.
    return current - 1;
}

static int utf8_codepoint_width(uint32_t codepoint) {
    if (codepoint == 0) {
        return 0;
    }

    // ASCII control characters and DEL/C1 controls
    if (codepoint < 32 || (codepoint >= 0x7F && codepoint < 0xA0)) {
        return 0;
    }

#if defined(__unix__) || defined(__APPLE__)
    int w = wcwidth((wchar_t)codepoint);
    if (w >= 0) {
        return w;
    }
#endif

    // Fallback if wcwidth is unavailable or locale not initialized:
    // Combining characters and zero-width formatting
    if ((codepoint >= 0x0300 &&
         codepoint <= 0x036F) || // Combining Diacritical Marks
        (codepoint >= 0x1AB0 &&
         codepoint <= 0x1AFF) || // Combining Diacritical Marks Extended
        (codepoint >= 0x1DC0 &&
         codepoint <= 0x1DFF) || // Combining Diacritical Marks Supplement
        (codepoint >= 0x20D0 &&
         codepoint <= 0x20FF) || // Combining Diacritical Marks for Symbols
        (codepoint >= 0xFE20 && codepoint <= 0xFE2F) || // Combining Half Marks
        (codepoint >= 0x200B &&
         codepoint <= 0x200F) || // Zero-width spaces & direction marks
        (codepoint >= 0x2028 &&
         codepoint <= 0x202E) || // Separators & direction overrides
        (codepoint >= 0x2060 && codepoint <= 0x206F) || // Invisible formatting
        (codepoint >= 0xFE00 && codepoint <= 0xFE0F) || // Variation Selectors
        (codepoint >= 0xE0100 &&
         codepoint <= 0xE01EF)) // Variation Selectors Supplement
    {
        return 0;
    }

    // East Asian Wide / Fullwidth characters & Emojis
    if ((codepoint >= 0x1100 && codepoint <= 0x115F) || // Hangul Jamo
        (codepoint >= 0x231A && codepoint <= 0x231B) || // Watch, Hourglass
        (codepoint >= 0x23E9 && codepoint <= 0x23EC) || // Fast-forward, etc.
        (codepoint >= 0x23F0 && codepoint <= 0x23F3) || // Alarm clock, etc.
        (codepoint >= 0x25FD && codepoint <= 0x25FE) || // Medium squares
        (codepoint >= 0x2614 &&
         codepoint <= 0x2615) || // Umbrella, hot beverage
        (codepoint >= 0x2648 && codepoint <= 0x2653) || // Zodiac signs
        (codepoint >= 0x267F && codepoint <= 0x267F) || // Wheelchair symbol
        (codepoint >= 0x2E80 && codepoint <= 0xA4CF &&
         codepoint != 0x303F) ||                        // CJK Radicals..Yi
        (codepoint >= 0xAC00 && codepoint <= 0xD7A3) || // Hangul Syllables
        (codepoint >= 0xF900 &&
         codepoint <= 0xFAFF) || // CJK Compatibility Ideographs
        (codepoint >= 0xFE10 && codepoint <= 0xFE19) || // Vertical Forms
        (codepoint >= 0xFE30 &&
         codepoint <= 0xFE6F) || // CJK Compatibility Forms
        (codepoint >= 0xFF01 && codepoint <= 0xFF60) || // Fullwidth Forms
        (codepoint >= 0xFFE0 && codepoint <= 0xFFE6) || // Fullwidth Signs
        (codepoint >= 0x1F000 &&
         codepoint <= 0x1FBFF) || // Emojis & Pictographs
        (codepoint >= 0x20000 &&
         codepoint <= 0x3FFFD)) // CJK Ideographs Extension
    {
        return 2;
    }

    return 1;
}

int utf8_charwidth(const char *str) {
    if (!str || *str == '\0') {
        return 0;
    }

    unsigned char char_len  = 0;
    uint32_t      codepoint = utf8_decode_char(str, &char_len);
    return utf8_codepoint_width(codepoint);
}

size_t utf8_width(const char *str) {
    if (!str) {
        return 0;
    }

    size_t width = 0;
    while (*str) {
        unsigned char char_len  = 0;
        uint32_t      codepoint = utf8_decode_char(str, &char_len);
        if (char_len == 0) {
            break;
        }
        width += utf8_codepoint_width(codepoint);
        str += char_len;
    }
    return width;
}