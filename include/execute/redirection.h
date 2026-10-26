/** redirection.h
 *
 * I/O redirection handling, file descriptor duplication, and in-process
 * save/restore.
 */

#ifndef EXECUTE_REDIRECTION_H
#define EXECUTE_REDIRECTION_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

#include "ast.h"
#include "session.h"

typedef struct SavedFd {
    int orig_fd;
    int dup_fd;
} SavedFd;

typedef struct RedirectionState {
    SavedFd *saved;
    size_t   count;
    size_t   capacity;
} RedirectionState;

int  handle_redirections(ASTNode *node, Session *session);
void save_fd(RedirectionState *state, int fd);
bool apply_in_process_redirections(ASTNode *node, Session *session,
                                   RedirectionState *state);
void restore_in_process_redirections(RedirectionState *state);

#endif /* EXECUTE_REDIRECTION_H */
