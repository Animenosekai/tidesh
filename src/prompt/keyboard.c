#include "prompt/keyboard.h"
#include "prompt/ansi.h"

#include <ctype.h>  /* tolower */
#include <stdlib.h> /* NULL */
#include <string.h> /* strncmp, strcmp, strlen */

// Control Chars
#define KEY_CTRL_A 1
#define KEY_CTRL_Z 26
#define KEY_CTRL_OFFSET ('a' - 1)
#define ASCII_TAB '\t'
#define ASCII_NEWLINE '\n'
#define ASCII_CARRIAGE_RETURN '\r'
#define ASCII_BACKSPACE_ASCII '\b'
#define ASCII_BACKSPACE_DEL 127

// ANSI 'O' Sequences
#define ANSI_O_HOME "H"
#define ANSI_O_END "F"

// ANSI CSI Sequences
#define ANSI_CSI_UP "A"
#define ANSI_CSI_DOWN "B"
#define ANSI_CSI_RIGHT "C"
#define ANSI_CSI_LEFT "D"
#define ANSI_CSI_HOME_H "H"
#define ANSI_CSI_END_F "F"
#define ANSI_CSI_SHIFT_TAB "Z"

// ANSI CSI '~' Sequences (xterm/linux)
#define ANSI_CSI_HOME_1 "1~"
#define ANSI_CSI_HOME_7 "7~"
#define ANSI_CSI_INSERT "2~"
#define ANSI_CSI_DELETE "3~"
#define ANSI_CSI_END_4 "4~"
#define ANSI_CSI_END_8 "8~"
#define ANSI_CSI_PAGE_UP "5~"
#define ANSI_CSI_PAGE_DOWN "6~"

// ANSI CSI Modifier combinations
#define ANSI_CSI_SHIFT 2
#define ANSI_CSI_ALT 3
#define ANSI_CSI_CTRL 5
#define ANSI_CSI_CTRL_SHIFT 6
// Note: Meta/Alt are often used interchangeably, but the Alt prefix is handled
// separately in `keyboard_parse`.

#define ANSI_CSI_MODIFIER_ARROW(modifier, arrow) "1;" #modifier arrow

#define ANSI_CSI_SHIFT_UP ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_SHIFT, ANSI_CSI_UP)
#define ANSI_CSI_SHIFT_DOWN                                                    \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_SHIFT, ANSI_CSI_DOWN)
#define ANSI_CSI_SHIFT_RIGHT                                                   \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_SHIFT, ANSI_CSI_RIGHT)
#define ANSI_CSI_SHIFT_LEFT                                                    \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_SHIFT, ANSI_CSI_LEFT)

#define ANSI_CSI_CTRL_UP ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL, ANSI_CSI_UP)
#define ANSI_CSI_CTRL_DOWN ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL, ANSI_CSI_DOWN)
#define ANSI_CSI_CTRL_RIGHT                                                    \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL, ANSI_CSI_RIGHT)
#define ANSI_CSI_CTRL_LEFT ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL, ANSI_CSI_LEFT)

#define ANSI_CSI_ALT_UP ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_ALT, ANSI_CSI_UP)
#define ANSI_CSI_ALT_DOWN ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_ALT, ANSI_CSI_DOWN)
#define ANSI_CSI_ALT_RIGHT ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_ALT, ANSI_CSI_RIGHT)
#define ANSI_CSI_ALT_LEFT ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_ALT, ANSI_CSI_LEFT)

#define ANSI_CSI_CTRL_SHIFT_UP                                                 \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL_SHIFT, ANSI_CSI_UP)
#define ANSI_CSI_CTRL_SHIFT_DOWN                                               \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL_SHIFT, ANSI_CSI_DOWN)
#define ANSI_CSI_CTRL_SHIFT_RIGHT                                              \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL_SHIFT, ANSI_CSI_RIGHT)
#define ANSI_CSI_CTRL_SHIFT_LEFT                                               \
    ANSI_CSI_MODIFIER_ARROW(ANSI_CSI_CTRL_SHIFT, ANSI_CSI_LEFT)

/* Finds the length of a string that is known to contain a key sequence. */
inline static size_t find_sequence_length(const char *input,
                                          const char *sequence) {
    size_t seq_len = strlen(sequence);
    // Check if the input starts with the sequence
    if (strncmp(input, sequence, seq_len) == 0) {
        return seq_len;
    }
    return 0;
}

/* Initializes a Key struct to default (unknown) values. */
inline static void init_key(Key *key) {
    key->type  = KEY_VALUE; // Default to KEY_VALUE, but with a null char
    key->value = '\0';
    key->read  = 0;
    key->ctrl  = false;
    key->alt   = false;
    key->shift = false;
    key->meta  = false;
}

/* Sets the key to a known special type and sets the 'read' value. */
inline static void set_special_key(Key *key, enum KeyType type, size_t read) {
    key->type  = type;
    key->value = '\0'; // No character value for special keys
    key->read  = read;
}

/* Parses a CSI sequence and returns the number of characters read. */
static size_t parse_csi_sequence(const char *input, Key *key) {
    size_t len = 0;

    // Arrows
    if ((len = find_sequence_length(input, ANSI_CSI_UP)) > 0) {
        key->type = KEY_UP;
    } else if ((len = find_sequence_length(input, ANSI_CSI_DOWN)) > 0) {
        key->type = KEY_DOWN;
    } else if ((len = find_sequence_length(input, ANSI_CSI_RIGHT)) > 0) {
        key->type = KEY_RIGHT;
    } else if ((len = find_sequence_length(input, ANSI_CSI_LEFT)) > 0) {
        key->type = KEY_LEFT;
    }

    // Home, End, Tab, etc.
    else if ((len = find_sequence_length(input, ANSI_CSI_HOME_H)) > 0) {
        key->type = KEY_HOME;
    } else if ((len = find_sequence_length(input, ANSI_CSI_END_F)) > 0) {
        key->type = KEY_END;
    } else if ((len = find_sequence_length(input, ANSI_CSI_SHIFT_TAB)) > 0) {
        key->type  = KEY_BACKTAB;
        key->shift = true;
    }

    // '~' sequences
    else if ((len = find_sequence_length(input, ANSI_CSI_HOME_1)) > 0 ||
             (len = find_sequence_length(input, ANSI_CSI_HOME_7)) > 0) {
        key->type = KEY_HOME;
    } else if ((len = find_sequence_length(input, ANSI_CSI_END_4)) > 0 ||
               (len = find_sequence_length(input, ANSI_CSI_END_8)) > 0) {
        key->type = KEY_END;
    } else if ((len = find_sequence_length(input, ANSI_CSI_INSERT)) > 0) {
        key->type = KEY_INSERT;
    } else if ((len = find_sequence_length(input, ANSI_CSI_DELETE)) > 0) {
        key->type = KEY_DELETE;
    } else if ((len = find_sequence_length(input, ANSI_CSI_PAGE_UP)) > 0) {
        key->type = KEY_PAGE_UP;
    } else if ((len = find_sequence_length(input, ANSI_CSI_PAGE_DOWN)) > 0) {
        key->type = KEY_PAGE_DOWN;
    }

    // Modified Arrows
    // Shift + Arrows
    else if ((len = find_sequence_length(input, ANSI_CSI_SHIFT_UP)) > 0) {
        key->type  = KEY_UP;
        key->shift = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_SHIFT_DOWN)) > 0) {
        key->type  = KEY_DOWN;
        key->shift = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_SHIFT_RIGHT)) > 0) {
        key->type  = KEY_RIGHT;
        key->shift = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_SHIFT_LEFT)) > 0) {
        key->type  = KEY_LEFT;
        key->shift = true;
    }
    // Ctrl + Arrows
    else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_UP)) > 0) {
        key->type = KEY_UP;
        key->ctrl = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_DOWN)) > 0) {
        key->type = KEY_DOWN;
        key->ctrl = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_RIGHT)) > 0) {
        key->type = KEY_RIGHT;
        key->ctrl = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_LEFT)) > 0) {
        key->type = KEY_LEFT;
        key->ctrl = true;
    }
    // Alt + Arrows
    else if ((len = find_sequence_length(input, ANSI_CSI_ALT_UP)) > 0) {
        key->type = KEY_UP;
        key->alt  = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_ALT_DOWN)) > 0) {
        key->type = KEY_DOWN;
        key->alt  = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_ALT_RIGHT)) > 0) {
        key->type = KEY_RIGHT;
        key->alt  = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_ALT_LEFT)) > 0) {
        key->type = KEY_LEFT;
        key->alt  = true;
    }
    // Ctrl + Shift + Arrows
    else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_SHIFT_UP)) > 0) {
        key->type  = KEY_UP;
        key->ctrl  = true;
        key->shift = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_SHIFT_DOWN)) >
               0) {
        key->type  = KEY_DOWN;
        key->ctrl  = true;
        key->shift = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_SHIFT_RIGHT)) >
               0) {
        key->type  = KEY_RIGHT;
        key->ctrl  = true;
        key->shift = true;
    } else if ((len = find_sequence_length(input, ANSI_CSI_CTRL_SHIFT_LEFT)) >
               0) {
        key->type  = KEY_LEFT;
        key->ctrl  = true;
        key->shift = true;
    }
    // If we matched a sequence, set its read length and return it.
    if (len > 0) {
        key->read  = strlen(ANSI_ESCAPE) + strlen(ANSI_CSI) + len;
        key->value = '\0';
        return key->read;
    }

    return 0;
}

/* Parses an 'O' sequence */
static size_t parse_o_sequence(const char *input, Key *key) {
    size_t len = 0;
    if ((len = find_sequence_length(input, ANSI_O_HOME)) > 0) {
        key->type = KEY_HOME;
    } else if ((len = find_sequence_length(input, ANSI_O_END)) > 0) {
        key->type = KEY_END;
    }

    if (len > 0) {
        // ESC (1) + 'O' (1) + sequence_length
        key->read  = strlen(ANSI_ESCAPE) + 1 + len;
        key->value = '\0';
        return key->read;
    }
    return 0;
}

Key keyboard_parse(const char *input) {
    Key key;
    init_key(&key);

    if (input == NULL || input[0] == '\0') {
        key.type = KEY_VALUE; // Still KEY_VALUE, but read is 0
        return key;
    }

    // Multi-byte sequences starting with ESC
    if (input[0] == ANSI_ESCAPE[0]) {
        // Check for ESC alone
        if (input[1] == '\0') {
            set_special_key(&key, KEY_ESCAPE, 1);
            return key;
        }

        // CSI sequence (e.g., ESC [ A for Up)
        if (strncmp(&input[1], ANSI_CSI, strlen(ANSI_CSI)) == 0) {
            size_t seq_len =
                parse_csi_sequence(&input[1 + strlen(ANSI_CSI)], &key);
            if (seq_len > 0) {
                return key;
            }
        }

        // 'O' sequence (e.g., ESC O H for Home)
        if (input[1] == 'O') {
            size_t seq_len = parse_o_sequence(&input[2], &key);
            if (seq_len > 0) {
                return key;
            }
        }

        // Alt-modified key (e.g., ESC followed by 'a')
        // Recursively parse the rest of the string as a normal key and set
        // alt=true We assume the Alt sequence is always 1 (ESC) + whatever
        // follows.
        Key alt_key = keyboard_parse(&input[1]);

        // If the recursive call returned something valid, then the Alt sequence
        // is valid.
        if (alt_key.type == KEY_VALUE || alt_key.read > 0) {
            alt_key.alt = true;
            // The read length is 1 (for ESC) + the read length of the rest.
            // If the recursive call returned a plain value (read=0), it still
            // consumed 1 byte.
            alt_key.read = (alt_key.read > 0 ? alt_key.read : 1) + 1;
            return alt_key;
        }

        // If it was just ESC followed by something we didn't recognize, treat
        // it as ESC
        set_special_key(&key, KEY_ESCAPE, 1);
        return key;
    }

    // Single control/special bytes
    switch (input[0]) {
        case ASCII_NEWLINE:
        case ASCII_CARRIAGE_RETURN:
            set_special_key(&key, KEY_ENTER, 1);
            return key;
        case ASCII_TAB:
            set_special_key(&key, KEY_TAB, 1);
            return key;
        case ASCII_BACKSPACE_ASCII:
        case ASCII_BACKSPACE_DEL:
            set_special_key(&key, KEY_BACKSPACE, 1);
            return key;
    }

    // Ctrl + printable character
    if (input[0] >= KEY_CTRL_A && input[0] <= KEY_CTRL_Z) {
        key.type  = KEY_VALUE;
        key.value = tolower(input[0] + KEY_CTRL_OFFSET);
        key.ctrl  = true;
        key.read  = 1;
        // NOTE: key.shift is false because terminals typically only report the
        // Ctrl modifier
        return key;
    }

    // Other printable characters
    key.type  = KEY_VALUE;
    key.value = input[0];
    key.read  = 0;
    return key;
}