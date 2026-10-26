/** context.h
 *
 * Declarations for context managers in tidesh.
 * Provides data structures and functions for managing scoped session states
 * using the 'with' statement and 'context-manager' registration command.
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

typedef struct Session Session;
typedef struct ASTNode ASTNode;

/* Represents an argument to a context manager: either a positional string or
 * key=val */
typedef struct ContextArg {
    char *key;   /* NULL if positional argument */
    char *value; /* value string */
} ContextArg;

/* Represents a single context manager invocation: e.g. env(FOO = "bar") */
typedef struct ContextInvocation {
    char                     *name;
    ContextArg               *args;
    size_t                    arg_count;
    struct ContextInvocation *next;
} ContextInvocation;

/* A registered custom context manager */
typedef struct CustomContextManager {
    char                        *name;
    char                        *enter_cmd;
    char                        *exit_cmd;
    struct CustomContextManager *next;
} CustomContextManager;

/* An entered context manager active in the current scope */
typedef struct ActiveContext {
    char                 *name;
    bool                  is_builtin;
    void                 *builtin_state;
    char                 *custom_saved_state;
    CustomContextManager *custom_mgr;
    ContextArg           *saved_args;
    size_t                saved_argc;
    struct ActiveContext *next;
} ActiveContext;

/* A context frame representing a 'with' block scope */
typedef struct ContextFrame {
    ActiveContext       *active_list;
    struct ContextFrame *prev;
} ContextFrame;

/**
 * Initialize context managers for a session.
 *
 * @param session Pointer to Session
 */
void init_session_context_managers(Session *session);

/**
 * Free all custom context managers in a session.
 *
 * @param session Pointer to Session
 */
void free_session_context_managers(Session *session);

/**
 * Register a custom context manager in the session.
 *
 * @param session Pointer to Session
 * @param name Name of context manager
 * @param enter_cmd Command string executed on enter
 * @param exit_cmd Command string executed on exit
 * @return 0 on success, non-zero on error
 */
int register_custom_context_manager(Session *session, const char *name,
                                    const char *enter_cmd,
                                    const char *exit_cmd);

/**
 * Unregister a custom context manager by name.
 *
 * @param session Pointer to Session
 * @param name Name of context manager to remove
 * @return 0 on success, non-zero on error
 */
int unregister_custom_context_manager(Session *session, const char *name);

/**
 * Lookup a registered custom context manager by name.
 *
 * @param session Pointer to Session
 * @param name Name to find
 * @return Pointer to CustomContextManager, or NULL if not found
 */
CustomContextManager *get_custom_context_manager(Session    *session,
                                                 const char *name);

/**
 * Check if a context manager name is a built-in manager (e.g. cwd, env,
 * options, umask).
 *
 * @param name Name to check
 * @return true if built-in, false otherwise
 */
bool is_builtin_context_manager(const char *name);

/**
 * Create a new context invocation.
 *
 * @param name Name of the context manager
 * @return Allocated ContextInvocation
 */
ContextInvocation *init_context_invocation(const char *name);

/**
 * Add an argument to a context invocation.
 *
 * @param inv Pointer to ContextInvocation
 * @param key Optional key (NULL if positional)
 * @param value Argument value
 */
void context_invocation_add_arg(ContextInvocation *inv, const char *key,
                                const char *value);

/**
 * Free a linked list of context invocations.
 *
 * @param inv Pointer to head of list
 */
void free_context_invocations(ContextInvocation *inv);

/**
 * Deep copy a linked list of context invocations.
 *
 * @param inv Pointer to head of list
 * @return Pointer to head of copied list
 */
ContextInvocation *copy_context_invocations(const ContextInvocation *inv);

/**
 * Execute a 'with' block AST node with the given context invocations and body.
 *
 * @param session Pointer to Session
 * @param invocations Linked list of ContextInvocation
 * @param body Body ASTNode to execute
 * @return Exit status of execution
 */
int execute_with_block(Session *session, ContextInvocation *invocations,
                       ASTNode *body);

// Built-in Context Managers
#include "contexts/airgap.h"
#include "contexts/allowed_exit_codes.h"
#include "contexts/chroot.h"
#include "contexts/cwd.h"
#include "contexts/env.h"
#include "contexts/network.h"
#include "contexts/options.h"
#include "contexts/sandbox.h"
#include "contexts/session.h"
#include "contexts/tempdir.h"
#include "contexts/umask.h"
#include "sandbox.h"

#endif /* CONTEXT_H */
