#ifndef PROMPT_COMPLETION_H
#define PROMPT_COMPLETION_H

#include <stddef.h>
#include "session.h"
#include "prompt/cursor.h"

/**
 * Perform tab completion at the current cursor position.
 *
 * @param cursor The current cursor state
 * @param session The shell session
 */
void completion_apply(Cursor *cursor, Session *session);

/**
 * Check if a character is a shell delimiter.
 *
 * @param c The character to check.
 * @return True if it's a delimiter, false otherwise.
 */
bool is_shell_delimiter(char c);

#endif /* PROMPT_COMPLETION_H */
