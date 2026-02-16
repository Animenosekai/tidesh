/** keyboard.h
 *
 * Keyboard input parsing and representation
 */

#ifndef PROMPT_KEYBOARD_H
#define PROMPT_KEYBOARD_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

enum KeyType {
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_DELETE,
    KEY_INSERT,
    KEY_TAB,
    KEY_BACKTAB,
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_BACKSPACE,
    KEY_VALUE,
};

typedef struct Key {
    // Main key
    enum KeyType type;
    char         value; // The character after the type
    size_t       read;  // Number of non-printable bytes read
    // Modifier keys (simultaneous with main key)
    bool ctrl;  // Ctrl key
    bool alt;   // Alt key
    bool shift; // Shift key
    bool meta;  // Meta key
} Key;

/**
 * Parses a key from the given input string.
 *
 * @param input The input string containing the key sequence.
 * @return The parsed Key structure.
 */
Key keyboard_parse(const char *input);

#endif /* PROMPT_KEYBOARD_H */
