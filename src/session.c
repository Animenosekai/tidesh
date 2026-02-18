#include <dirent.h> /* DIR, opendir, readdir, closedir */
#include <limits.h> /* PATH_MAX */
#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* strdup, strcmp */
#include <unistd.h> /* getcwd */

#include "data/array.h"      /* array_add, free_array */
#include "environ.h"         /* environ_get, environ_set, environ_get_default */
#include "prompt/terminal.h" /* Terminal, terminal functions */
#include "session.h"         /* Session, Environ, History, Trie, DirStack */

Session *init_session(Session *session, char *history_path) {
    if (!session) {
        session = calloc(1, sizeof(Session));
        if (!session) {
            return NULL;
        }
    } else {
        memset(session, 0, sizeof(Session));
    }

    session->environ = init_environ(NULL);
    if (!session->environ) {
        free(session);
        return NULL;
    }

    session->history = load_history(NULL, history_path);
    if (!session->history) {
        free_session(session);
        free(session);
        return NULL;
    }

    session->aliases = init_trie(NULL);
    if (!session->aliases) {
        free_session(session);
        free(session);
        return NULL;
    }

    session->dirstack = init_dirstack(NULL);
    if (!session->dirstack) {
        free_session(session);
        free(session);
        return NULL;
    }

    session->path_commands = init_trie(NULL);
    if (!session->path_commands) {
        free_session(session);
        free(session);
        return NULL;
    }

    session->terminal = init_terminal(NULL, session);
    if (!session->terminal) {
        free_session(session);
        free(session);
        return NULL;
    }

    session->jobs = init_jobs();
    if (!session->jobs) {
        free_session(session);
        free(session);
        return NULL;
    }

    session->current_working_dir  = NULL;
    session->previous_working_dir = NULL;
    session->exit_requested       = false;

    // Initialize working directories
    update_working_dir(session);
    // update_path(session); // Pretty slow
    return session;
}

void free_session(Session *session) {
    if (!session)
        return;

    // Free working directories (session owns these)
    free(session->current_working_dir);
    free(session->previous_working_dir);

    if (session->dirstack) {
        free_dirstack(session->dirstack);
        free(session->dirstack);
    };

    if (session->environ) {
        free_environ(session->environ);
        free(session->environ);
    }

    if (session->history) {
        free_history(session->history);
        free(session->history);
    }

    if (session->aliases) {
        free_trie(session->aliases);
        free(session->aliases);
    }

    if (session->path_commands) {
        free_trie(session->path_commands);
        free(session->path_commands);
    }

    if (session->terminal) {
        free_terminal(session->terminal, session);
        free(session->terminal);
    }

    if (session->jobs) {
        free_jobs(session->jobs);
        free(session->jobs);
    }
}

static void init_previous_working_dir(Session *session) {
    // First time setting CWD, set previous to same value
    char *oldpwd = environ_get(session->environ, "OLDPWD");
    if (!oldpwd) {
        session->previous_working_dir = strdup(session->current_working_dir);
        environ_set(session->environ, "OLDPWD", session->previous_working_dir);
    } else {
        session->previous_working_dir = strdup(oldpwd);
    }
}

void update_working_dir(Session *session) {
    const char *current_value = session->current_working_dir;
    char       *cwd           = getcwd(NULL, 0);

    if (cwd) {
        if (current_value && strcmp(current_value, cwd) == 0) {
            free(cwd);
        } else {
            session->current_working_dir = strdup(cwd);
            environ_set(session->environ, "PWD", session->current_working_dir);
            free(cwd);
        }
    } else if (!session->current_working_dir) {
        session->current_working_dir =
            strdup(environ_get_default(session->environ, "PWD", "."));
    } else {
        if (!session->previous_working_dir) {
            init_previous_working_dir(session);
        }
        return;
    }

    if (!session->dirstack->stack->count) {
        // Initialize dirstack with current dir (make a copy)
        array_add(session->dirstack->stack,
                  strdup(session->current_working_dir));
    }

    if (!session->previous_working_dir) {
        init_previous_working_dir(session);
        return;
    }

    // if `session->current_working_dir` is the same as before, we did not
    // change directories, so do not update the previous working dir
    if (current_value && session->current_working_dir &&
        strcmp(current_value, session->current_working_dir) == 0) {
        return;
    }

    // Else update OLDPWD and the previous working dir
    if (session->previous_working_dir) {
        free(session->previous_working_dir);
    }

    session->previous_working_dir = (char *)current_value;
    if (session->previous_working_dir) {
        environ_set(session->environ, "OLDPWD", session->previous_working_dir);
    }
}

void update_path(Session *session) {
    char *original_path = environ_get(session->environ, "PATH");
    if (!original_path) {
        return;
    }

    if (session->path_commands) {
        free_trie(session->path_commands);
        free(session->path_commands);
    }

    session->path_commands = init_trie(NULL);
    if (!session->path_commands) {
        return;
    }

    char *path  = strdup(original_path);
    char *token = strtok(path, ":");
    while (token) {
        // Open directory and read entries
        DIR *dir = opendir(token);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                // Skip . and ..
                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                // Build full path
                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", token,
                         entry->d_name);

                // Check if executable
                if (access(full_path, X_OK) == 0) {
                    trie_set(session->path_commands, entry->d_name, full_path);
                }
            }
            closedir(dir);
        }
        token = strtok(NULL, ":");
    }
    free(path);
}
