/** session.c
 *
 * Implementation of the 'session' built-in context manager and session
 * snapshots.
 */

#include <limits.h>  /* PATH_MAX */
#include <stdbool.h> /* bool */
#include <stdio.h>   /* snprintf */
#include <stdlib.h>  /* calloc, free */
#include <string.h>  /* strdup, strcmp */
#include <sys/stat.h>/* umask, mode_t */
#include <unistd.h>  /* getcwd, chdir */

#include "ast.h"     /* copy_ast, free_ast */
#include "context.h" /* ContextArg, free_session_context_managers */
#include "contexts/session.h"
#include "data/array.h" /* array_copy, free_array */
#include "data/trie.h" /* trie_copy, free_trie, init_trie, trie_starting_with */
#include "environ.h"   /* environ_copy, free_environ, environ_set */
#include "execute.h"   /* load_function_body, store_function_body */
#include "session.h"   /* Session, update_working_dir */

struct SessionSnapshot {
    char    *saved_cwd;
    char    *saved_oldpwd;
    Environ *saved_environ;
#ifndef TIDESH_DISABLE_ALIASES
    Trie *saved_aliases;
#endif
#ifndef TIDESH_DISABLE_FUNCTIONS
    Trie *saved_functions;
#endif
#ifndef TIDESH_DISABLE_DIRSTACK
    Array *saved_dirstack;
#endif
    Trie        *saved_path_commands;
    ShellOptions saved_options;
    Features     saved_features;
    mode_t       saved_umask;
#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    CustomContextManager *saved_context_managers;
#endif
    char *saved_trap_exit;
    char *saved_trap_signals[TIDESH_MAX_SIGNALS];
    bool  permanent_redirections;
};

SessionSnapshot *session_snapshot_create(Session *session) {
    if (!session)
        return NULL;

    SessionSnapshot *snap = calloc(1, sizeof(SessionSnapshot));
    if (!snap)
        return NULL;

    if (session->current_working_dir) {
        snap->saved_cwd = strdup(session->current_working_dir);
    } else {
        char buf[PATH_MAX];
        if (getcwd(buf, sizeof(buf))) {
            snap->saved_cwd = strdup(buf);
        }
    }
    if (session->previous_working_dir) {
        snap->saved_oldpwd = strdup(session->previous_working_dir);
    }

    if (session->environ) {
        snap->saved_environ = environ_copy(session->environ, NULL);
    }

#ifndef TIDESH_DISABLE_ALIASES
    if (session->aliases) {
        snap->saved_aliases = trie_copy(session->aliases, NULL);
    }
#endif

#ifndef TIDESH_DISABLE_FUNCTIONS
    if (session->functions) {
        snap->saved_functions = init_trie(NULL);
        Array *names          = trie_starting_with(session->functions, "");
        if (names) {
            for (size_t i = 0; i < names->count; i++) {
                char    *fname = names->items[i];
                ASTNode *b     = load_function_body(session->functions, fname);
                if (b) {
                    ASTNode *cp = copy_ast(b);
                    store_function_body(snap->saved_functions, fname, cp);
                }
            }
            free_array(names);
            free(names);
        }
    }
#endif

#ifndef TIDESH_DISABLE_DIRSTACK
    if (session->dirstack && session->dirstack->stack) {
        snap->saved_dirstack = array_copy(session->dirstack->stack, NULL);
    }
#endif

    if (session->path_commands) {
        snap->saved_path_commands = trie_copy(session->path_commands, NULL);
    }

    snap->saved_options  = session->options;
    snap->saved_features = session->features;

    mode_t cur_mask = umask(0);
    umask(cur_mask);
    snap->saved_umask = cur_mask;

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    CustomContextManager  *head = NULL;
    CustomContextManager **tail = &head;
    for (CustomContextManager *m = session->context_managers; m; m = m->next) {
        CustomContextManager *cp = calloc(1, sizeof(CustomContextManager));
        if (cp) {
            cp->name      = m->name ? strdup(m->name) : NULL;
            cp->enter_cmd = m->enter_cmd ? strdup(m->enter_cmd) : NULL;
            cp->exit_cmd  = m->exit_cmd ? strdup(m->exit_cmd) : NULL;
            *tail         = cp;
            tail          = &cp->next;
        }
    }
    snap->saved_context_managers = head;
#endif

    if (session->trap_exit) {
        snap->saved_trap_exit = strdup(session->trap_exit);
    }
    for (int i = 0; i < TIDESH_MAX_SIGNALS; i++) {
        if (session->trap_signals[i]) {
            snap->saved_trap_signals[i] = strdup(session->trap_signals[i]);
        }
    }

    snap->permanent_redirections = session->permanent_redirections;

    return snap;
}

void session_snapshot_restore(Session *session, SessionSnapshot *snapshot) {
    if (!session || !snapshot)
        return;

    if (snapshot->saved_cwd) {
        if (chdir(snapshot->saved_cwd) != 0) {
            // failed chdir
        }
        update_working_dir(session);
    }
    if (snapshot->saved_oldpwd) {
        free(session->previous_working_dir);
        session->previous_working_dir = strdup(snapshot->saved_oldpwd);
        if (session->environ) {
            environ_set(session->environ, "OLDPWD",
                        session->previous_working_dir);
        }
    }

    if (session->environ && snapshot->saved_environ) {
        environ_copy(snapshot->saved_environ, session->environ);
    }

#ifndef TIDESH_DISABLE_ALIASES
    if (session->aliases && snapshot->saved_aliases) {
        free_trie(session->aliases);
        free(session->aliases);
        session->aliases = trie_copy(snapshot->saved_aliases, NULL);
    }
#endif

#ifndef TIDESH_DISABLE_FUNCTIONS
    if (session->functions && snapshot->saved_functions) {
        Array *curr = trie_starting_with(session->functions, "");
        if (curr) {
            for (size_t i = 0; i < curr->count; i++) {
                ASTNode *b =
                    load_function_body(session->functions, curr->items[i]);
                if (b) {
                    free_ast(b);
                    free(b);
                }
            }
            free_array(curr);
            free(curr);
        }
        free_trie(session->functions);
        free(session->functions);

        session->functions = init_trie(NULL);
        Array *names       = trie_starting_with(snapshot->saved_functions, "");
        if (names) {
            for (size_t i = 0; i < names->count; i++) {
                char    *fname = names->items[i];
                ASTNode *b =
                    load_function_body(snapshot->saved_functions, fname);
                if (b) {
                    ASTNode *cp = copy_ast(b);
                    store_function_body(session->functions, fname, cp);
                }
            }
            free_array(names);
            free(names);
        }
    }
#endif

#ifndef TIDESH_DISABLE_DIRSTACK
    if (session->dirstack && snapshot->saved_dirstack) {
        if (session->dirstack->stack) {
            free_array(session->dirstack->stack);
            free(session->dirstack->stack);
        }
        session->dirstack->stack = array_copy(snapshot->saved_dirstack, NULL);
    }
#endif

    if (session->path_commands && snapshot->saved_path_commands) {
        free_trie(session->path_commands);
        free(session->path_commands);
        session->path_commands = trie_copy(snapshot->saved_path_commands, NULL);
    }

    session->options  = snapshot->saved_options;
    session->features = snapshot->saved_features;

    umask(snapshot->saved_umask);

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    free_session_context_managers(session);
    CustomContextManager  *head = NULL;
    CustomContextManager **tail = &head;
    for (CustomContextManager *m = snapshot->saved_context_managers; m;
         m                       = m->next) {
        CustomContextManager *cp = calloc(1, sizeof(CustomContextManager));
        if (cp) {
            cp->name      = m->name ? strdup(m->name) : NULL;
            cp->enter_cmd = m->enter_cmd ? strdup(m->enter_cmd) : NULL;
            cp->exit_cmd  = m->exit_cmd ? strdup(m->exit_cmd) : NULL;
            *tail         = cp;
            tail          = &cp->next;
        }
    }
    session->context_managers = head;
#endif

    free(session->trap_exit);
    session->trap_exit =
        snapshot->saved_trap_exit ? strdup(snapshot->saved_trap_exit) : NULL;
    for (int i = 0; i < TIDESH_MAX_SIGNALS; i++) {
        free(session->trap_signals[i]);
        session->trap_signals[i] = snapshot->saved_trap_signals[i]
                                       ? strdup(snapshot->saved_trap_signals[i])
                                       : NULL;
    }

    session->permanent_redirections = snapshot->permanent_redirections;
}

void session_snapshot_free(SessionSnapshot *snapshot) {
    if (!snapshot)
        return;

    free(snapshot->saved_cwd);
    free(snapshot->saved_oldpwd);

    if (snapshot->saved_environ) {
        free_environ(snapshot->saved_environ);
        free(snapshot->saved_environ);
    }

#ifndef TIDESH_DISABLE_ALIASES
    if (snapshot->saved_aliases) {
        free_trie(snapshot->saved_aliases);
        free(snapshot->saved_aliases);
    }
#endif

#ifndef TIDESH_DISABLE_FUNCTIONS
    if (snapshot->saved_functions) {
        Array *names = trie_starting_with(snapshot->saved_functions, "");
        if (names) {
            for (size_t i = 0; i < names->count; i++) {
                ASTNode *b = load_function_body(snapshot->saved_functions,
                                                names->items[i]);
                if (b) {
                    free_ast(b);
                    free(b);
                }
            }
            free_array(names);
            free(names);
        }
        free_trie(snapshot->saved_functions);
        free(snapshot->saved_functions);
    }
#endif

#ifndef TIDESH_DISABLE_DIRSTACK
    if (snapshot->saved_dirstack) {
        free_array(snapshot->saved_dirstack);
        free(snapshot->saved_dirstack);
    }
#endif

    if (snapshot->saved_path_commands) {
        free_trie(snapshot->saved_path_commands);
        free(snapshot->saved_path_commands);
    }

#ifndef TIDESH_DISABLE_CONTEXT_MANAGERS
    CustomContextManager *m = snapshot->saved_context_managers;
    while (m) {
        CustomContextManager *next = m->next;
        free(m->name);
        free(m->enter_cmd);
        free(m->exit_cmd);
        free(m);
        m = next;
    }
#endif

    free(snapshot->saved_trap_exit);
    for (int i = 0; i < TIDESH_MAX_SIGNALS; i++) {
        free(snapshot->saved_trap_signals[i]);
    }

    free(snapshot);
}

int context_enter_session(Session *session, ContextArg *args, size_t argc,
                          void **state) {
    (void)args;
    (void)argc;
    if (!session)
        return 1;

    SessionSnapshot *snap = session_snapshot_create(session);
    if (!snap)
        return 1;

    *state = snap;
    return 0;
}

int context_exit_session(Session *session, int status, void *state) {
    (void)status;
    SessionSnapshot *snap = (SessionSnapshot *)state;
    if (session && snap) {
        session_snapshot_restore(session, snap);
        session_snapshot_free(snap);
    }
    return 0;
}
