/** prompt.h
 *
 * Prompt display and user input handling
 */

#ifndef PROMPT_H
#define PROMPT_H

#include <stdbool.h> /* bool */

#include "session.h" /* Session */

/**
 * Displays a prompt and reads user input until the provided should_return
 * function returns true.
 *
 * @param prompt The main prompt string to display.
 * @param continuation_prompt The continuation prompt string to display for
 *                           additional input lines.
 * @param session The current session context.
 * @param should_return A function that determines whether the input is
 *                      complete and ready to be returned. Called at each line
 *                      break with the current input and session.
 * @return The complete user input as a string.
 */
char *prompt(const char *prompt, const char *continuation_prompt,
             Session *session,
             bool (*should_return)(char *input, Session *session));

#endif
