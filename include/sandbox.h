/** sandbox.h
 *
 * Declarations for filesystem isolation and sandboxing in tidesh.
 * Provides data structures and functions for controlling filesystem
 * access permissions (read-only, read-write, deny, chroot) across
 * UNIX-like systems and macOS.
 */

#ifndef SANDBOX_H
#define SANDBOX_H

#include <stdbool.h> /* bool */
#include <stddef.h>  /* size_t */

typedef struct Session Session;

/* File access operations */
typedef enum FSSandboxAccessType {
    FS_SANDBOX_READ     = 1 << 0,
    FS_SANDBOX_WRITE    = 1 << 1,
    FS_SANDBOX_EXEC     = 1 << 2,
    FS_SANDBOX_TRAVERSE = 1 << 3,
} FSSandboxAccessType;

/* Linked list of sandboxed path rules */
typedef struct FSSandboxPath {
    char                 *path;
    struct FSSandboxPath *next;
} FSSandboxPath;

/* Filesystem sandbox layer */
typedef struct FSSandbox {
    bool is_active;
    bool readonly_all; /* If true, entire filesystem is RO except rw_paths */
    FSSandboxPath *ro_paths;   /* Explicitly read-only paths */
    FSSandboxPath *rw_paths;   /* Allowed writable paths */
    FSSandboxPath *deny_paths; /* Paths blocked from all access */
    char          *chroot_dir; /* Chroot jail root if configured */
    char          *temp_dir;   /* Created temporary directory if any */
    bool  temp_dir_cleanup;    /* Whether temp_dir should be deleted on pop */
    char *saved_cwd;           /* Saved working directory */
    struct FSSandbox *prev;    /* Previous sandbox in stack */
} FSSandbox;

/**
 * Allocate and initialize a new FSSandbox layer.
 *
 * @return Pointer to allocated FSSandbox, or NULL on allocation failure
 */
FSSandbox *fs_sandbox_create(void);

/**
 * Free a single FSSandbox structure and its path lists.
 *
 * @param sb Pointer to FSSandbox to free
 */
void fs_sandbox_free(FSSandbox *sb);

/**
 * Deep copy a stack of FSSandbox layers.
 *
 * @param stack Pointer to top of stack
 * @return Pointer to head of copied stack
 */
FSSandbox *fs_sandbox_copy_stack(const FSSandbox *stack);

/**
 * Free an entire stack of FSSandbox layers.
 *
 * @param stack Pointer to top of stack
 */
void fs_sandbox_free_stack(FSSandbox *stack);

/**
 * Push an FSSandbox layer onto the session's sandbox stack.
 *
 * @param session Pointer to Session
 * @param sb Pointer to FSSandbox layer to push
 */
void fs_sandbox_push(Session *session, FSSandbox *sb);

/**
 * Pop the top FSSandbox layer from the session's sandbox stack.
 *
 * @param session Pointer to Session
 * @return Pointer to popped FSSandbox layer (caller may free or clean up)
 */
FSSandbox *fs_sandbox_pop(Session *session);

/**
 * Add a path to the read-only paths list.
 *
 * @param sb Pointer to FSSandbox
 * @param path Path to mark read-only
 */
void fs_sandbox_add_ro_path(FSSandbox *sb, const char *path);

/**
 * Add a path to the writable paths list.
 *
 * @param sb Pointer to FSSandbox
 * @param path Path to allow write access
 */
void fs_sandbox_add_rw_path(FSSandbox *sb, const char *path);

/**
 * Add a path to the denied paths list.
 *
 * @param sb Pointer to FSSandbox
 * @param path Path to deny all access
 */
void fs_sandbox_add_deny_path(FSSandbox *sb, const char *path);

/**
 * Add colon-separated or comma-separated paths to the target path list.
 *
 * @param sb Pointer to FSSandbox
 * @param path_list Colon-separated or comma-separated list of paths
 * @param target Type of rule (0: ro, 1: rw, 2: deny)
 */
void fs_sandbox_add_paths(FSSandbox *sb, const char *path_list, int target);

/**
 * Set the chroot jail directory for this sandbox layer.
 *
 * @param sb Pointer to FSSandbox
 * @param path Path to chroot jail
 */
void fs_sandbox_set_chroot(FSSandbox *sb, const char *path);

/**
 * Canonicalize a path lexically, resolving . and .. components.
 *
 * @param path Input path
 * @param cwd Current working directory for relative paths
 * @return Newly allocated canonical path string, or NULL on error
 */
char *fs_sandbox_canonicalize(const char *path, const char *cwd);

/**
 * Resolve a path canonicalizing lexical elements and resolving symlinks where
 * possible.
 *
 * @param path Input path
 * @param cwd Current working directory for relative paths
 * @return Newly allocated resolved path string, or NULL on error
 */
char *fs_sandbox_resolve_path(const char *path, const char *cwd);

/**
 * Check if a target path matches or is contained within a prefix directory.
 *
 * @param target Normalized path to check
 * @param prefix Normalized prefix directory
 * @return true if target matches prefix, false otherwise
 */
bool fs_sandbox_path_matches(const char *target, const char *prefix);

/**
 * Check whether the active sandbox policies in the session permit access.
 *
 * @param session Pointer to Session
 * @param path Target path being accessed
 * @param access Access flags (FS_SANDBOX_READ, FS_SANDBOX_WRITE,
 * FS_SANDBOX_EXEC, FS_SANDBOX_TRAVERSE)
 * @return true if allowed, false if denied
 */
bool fs_sandbox_check_access(Session *session, const char *path,
                             FSSandboxAccessType requested_access);

/**
 * Apply active sandbox restrictions to a forked child process before execve.
 * On macOS: applies seatbelt sandbox profile if possible.
 * On Linux: applies Landlock unprivileged sandbox if supported by kernel.
 * On UNIX: applies chroot jail if configured.
 *
 * @param session Pointer to Session
 */
void fs_sandbox_apply_child(Session *session);

/**
 * Recursively remove a directory and all its contents.
 *
 * @param path Path to directory to remove
 * @return 0 on success, -1 on failure
 */
int fs_sandbox_rmtree(const char *path);

#endif /* SANDBOX_H */
