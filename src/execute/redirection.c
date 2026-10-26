#include <errno.h> /* errno */
#include <fcntl.h> /* open, O_WRONLY, O_CREAT, O_APPEND, O_TRUNC, O_RDONLY, fcntl, F_SETFD, FD_CLOEXEC */
#include <signal.h>  /* signal, SIGINT, SIGQUIT, SIG_DFL */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* fprintf, stderr, fflush, stdout */
#include <stdlib.h>  /* realloc, free, exit, atoi */
#include <string.h>  /* strcmp, strerror, strlen */
#include <unistd.h> /* close, dup, dup2, pipe, write, fork, access, F_OK, STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO */

#include "ast.h"
#include "execute.h"
#include "execute/redirection.h"
#include "sandbox.h"
#include "session.h"

#define RW_R__R__ 0644

/* Handle combined output redirection and process substitution */
int handle_redirections(ASTNode *node, Session *session) {
#ifndef TIDESH_DISABLE_REDIRECTIONS
    if (!session->features.redirections) {
        if (node->redirects) {
            fprintf(stderr, "tidesh: redirections are disabled\n");
            return -1;
        }
    }
    Redirection *redirect = node->redirects;
    while (redirect) {
        if (redirect->type == TOKEN_FD_DUPLICATION) {
            if (strcmp(redirect->target, "-") == 0) {
                close(redirect->fd);
            } else {
                int src_fd = atoi(redirect->target);
                if (dup2(src_fd, redirect->fd) < 0) {
                    fprintf(stderr, "tidesh: %d: %s\n", src_fd,
                            strerror(errno));
                    return -1;
                }
            }
            redirect = redirect->next;
            continue;
        }

        int fd_file = -1;
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
        if (redirect->type == TOKEN_HEREDOC ||
            redirect->type == TOKEN_HERESTRING) {
            int pipe_fds[2];
            if (pipe(pipe_fds) == 0) {
                if (fork() == 0) {
                    signal(SIGINT, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    close(pipe_fds[0]);
                    write(pipe_fds[1], redirect->target,
                          strlen(redirect->target));
                    close(pipe_fds[1]);
                    exit(0);
                }
                close(pipe_fds[1]);
                fd_file = pipe_fds[0];
            }
        } else if (redirect->is_process_substitution) {
            // Process substitution
            bool  is_in = (redirect->type == TOKEN_REDIRECT_IN ||
                           redirect->type == TOKEN_PROCESS_SUBSTITUTION_IN);
            char *cmd   = redirect->target;

            int pipe_fds[2];
            if (pipe(pipe_fds) == 0) {
                if (fork() == 0) {
                    signal(SIGINT, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    if (is_in) {
                        close(pipe_fds[0]);
                        dup2(pipe_fds[1], STDOUT_FILENO);
                        close(pipe_fds[1]);
                    } else {
                        close(pipe_fds[1]);
                        dup2(pipe_fds[0], STDIN_FILENO);
                        close(pipe_fds[0]);
                    }
                    exit(execute_string(cmd, session));
                }
                if (is_in) {
                    close(pipe_fds[1]);
                    fd_file = pipe_fds[0];
                } else {
                    close(pipe_fds[0]);
                    fd_file = pipe_fds[1];
                }
            }
        } else {
#endif
            int flags = O_WRONLY | O_CREAT;
            if (redirect->type == TOKEN_REDIRECT_APPEND ||
                redirect->type == TOKEN_REDIRECT_APPEND_ERR)
                flags |= O_APPEND;
            else if (redirect->type == TOKEN_REDIRECT_OUT ||
                     redirect->type == TOKEN_REDIRECT_OUT_ERR) {
                if (session->options.noclobber &&
                    access(redirect->target, F_OK) == 0) {
                    fprintf(stderr,
                            "tidesh: %s: cannot overwrite existing file\n",
                            redirect->target);
                    return -1;
                }
                flags |= O_TRUNC;
            } else if (redirect->type == TOKEN_REDIRECT_IN)
                flags = O_RDONLY;

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
            int access_type = (redirect->type == TOKEN_REDIRECT_IN)
                                  ? FS_SANDBOX_READ
                                  : FS_SANDBOX_WRITE;
            if (!fs_sandbox_check_access(session, redirect->target,
                                         access_type)) {
                fprintf(stderr, "tidesh: %s: %s\n", redirect->target,
                        (access_type == FS_SANDBOX_WRITE)
                            ? "write denied by sandbox"
                            : "access denied by sandbox");
                return -1;
            }
#endif

            fd_file = open(redirect->target, flags, RW_R__R__);
#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
        }
#endif

        if (fd_file < 0) {
            fprintf(stderr, "Error opening redirection target: %s\n",
                    strerror(errno));
            return -1;
        }

        dup2(fd_file, redirect->fd);
        if (redirect->type == TOKEN_REDIRECT_OUT_ERR ||
            redirect->type == TOKEN_REDIRECT_APPEND_ERR) {
            dup2(fd_file, STDERR_FILENO);
        }
        close(fd_file);
        redirect = redirect->next;
    }
#endif /* TIDESH_DISABLE_REDIRECTIONS */
    return 0;
}

#ifndef TIDESH_DISABLE_REDIRECTIONS
void save_fd(RedirectionState *state, int fd) {
    if (fd < 0) {
        return;
    }
    for (size_t i = 0; i < state->count; i++) {
        if (state->saved[i].orig_fd == fd) {
            return;
        }
    }

    int dup_fd = dup(fd);
    if (dup_fd >= 0) {
        fcntl(dup_fd, F_SETFD, FD_CLOEXEC);
        if (state->count >= state->capacity) {
            size_t   new_cap = state->capacity == 0 ? 4 : state->capacity * 2;
            SavedFd *new_saved =
                realloc(state->saved, new_cap * sizeof(SavedFd));
            if (!new_saved) {
                close(dup_fd);
                return;
            }
            state->saved    = new_saved;
            state->capacity = new_cap;
        }
        state->saved[state->count].orig_fd = fd;
        state->saved[state->count].dup_fd  = dup_fd;
        state->count++;
    }
}
#endif

bool apply_in_process_redirections(ASTNode *node, Session *session,
                                   RedirectionState *state) {
    state->saved    = NULL;
    state->count    = 0;
    state->capacity = 0;

#ifndef TIDESH_DISABLE_REDIRECTIONS
    if (!node || !node->redirects) {
        return true;
    }

    Redirection *redirect = node->redirects;
    while (redirect) {
        save_fd(state, redirect->fd);
        if (redirect->type == TOKEN_REDIRECT_OUT_ERR ||
            redirect->type == TOKEN_REDIRECT_APPEND_ERR) {
            save_fd(state, STDERR_FILENO);
        }
        redirect = redirect->next;
    }

    fflush(stdout);
    fflush(stderr);

    if (handle_redirections(node, session) < 0) {
        return false;
    }
#else
    (void)node;
    (void)session;
#endif
    return true;
}

void restore_in_process_redirections(RedirectionState *state) {
    if (!state || !state->saved) {
        return;
    }

    fflush(stdout);
    fflush(stderr);

    for (size_t i = 0; i < state->count; i++) {
        if (state->saved[i].dup_fd >= 0) {
            dup2(state->saved[i].dup_fd, state->saved[i].orig_fd);
            close(state->saved[i].dup_fd);
        }
    }

    free(state->saved);
    state->saved    = NULL;
    state->count    = 0;
    state->capacity = 0;
}
