/** sandbox.c
 *
 * Implementation of filesystem isolation and sandboxing in tidesh.
 * Provides path access checks, Landlock integration on Linux, Seatbelt
 * profile application on macOS, and POSIX chroot support.
 */

#if defined(__APPLE__)
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#endif
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "context.h"
#include "sandbox.h"
#include "session.h"

extern int chroot(const char *);

#if defined(__APPLE__)
extern int  sandbox_init(const char *profile, uint64_t flags, char **errorbuf);
extern void sandbox_free_error(char *errorbuf);
#ifndef SANDBOX_NAMED
#define SANDBOX_NAMED 0x0001
#endif
#endif

#if defined(__linux__)
#include <sys/prctl.h>
#include <sys/syscall.h>

/* Landlock definitions if not in system headers */
#ifndef LANDLOCK_CREATE_RULESET_VERSION
struct landlock_ruleset_attr {
    __u64 handled_access_fs;
};

enum landlock_rule_type {
    LANDLOCK_RULE_PATH_BENEATH = 1,
};

struct landlock_path_beneath_attr {
    __u64 allowed_access;
    __s32 parent_fd;
} __attribute__((packed));

#ifndef LANDLOCK_ACCESS_FS_EXECUTE
#define LANDLOCK_ACCESS_FS_EXECUTE (1ULL << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE (1ULL << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE (1ULL << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR (1ULL << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR (1ULL << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE (1ULL << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR (1ULL << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR (1ULL << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG (1ULL << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK (1ULL << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO (1ULL << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK (1ULL << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM (1ULL << 12)
#endif

#ifndef __NR_landlock_create_ruleset
#if defined(__x86_64__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule 445
#define __NR_landlock_restrict_self 446
#elif defined(__aarch64__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule 445
#define __NR_landlock_restrict_self 446
#elif defined(__i386__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule 445
#define __NR_landlock_restrict_self 446
#elif defined(__arm__)
#define __NR_landlock_create_ruleset 444
#define __NR_landlock_add_rule 445
#define __NR_landlock_restrict_self 446
#endif
#endif

#endif /* LANDLOCK_CREATE_RULESET_VERSION */
#endif /* __linux__ */

/* -------------------------------------------------------------------------
 * Memory Allocation and Lifetime Management
 * ------------------------------------------------------------------------- */

FSSandbox *fs_sandbox_create(void) {
    FSSandbox *sb = calloc(1, sizeof(FSSandbox));
    if (!sb)
        return NULL;
    sb->is_active = true;
    return sb;
}

static void free_path_list(FSSandboxPath *head) {
    while (head) {
        FSSandboxPath *next = head->next;
        free(head->path);
        free(head);
        head = next;
    }
}

static FSSandboxPath *copy_path_list(const FSSandboxPath *head) {
    FSSandboxPath *new_head = NULL;
    FSSandboxPath *new_tail = NULL;
    for (const FSSandboxPath *p = head; p; p = p->next) {
        FSSandboxPath *node = calloc(1, sizeof(FSSandboxPath));
        if (!node)
            break;
        node->path = p->path ? strdup(p->path) : NULL;
        if (!new_head) {
            new_head = node;
            new_tail = node;
        } else {
            new_tail->next = node;
            new_tail       = node;
        }
    }
    return new_head;
}

void fs_sandbox_free(FSSandbox *sb) {
    if (!sb)
        return;
    free_path_list(sb->ro_paths);
    free_path_list(sb->rw_paths);
    free_path_list(sb->deny_paths);
    free(sb->chroot_dir);
    free(sb->temp_dir);
    free(sb->saved_cwd);
    free(sb);
}

FSSandbox *fs_sandbox_copy_stack(const FSSandbox *stack) {
    if (!stack)
        return NULL;

    FSSandbox *new_top  = NULL;
    FSSandbox *prev_new = NULL;

    for (const FSSandbox *curr = stack; curr; curr = curr->prev) {
        FSSandbox *node = calloc(1, sizeof(FSSandbox));
        if (!node)
            break;
        node->is_active    = curr->is_active;
        node->readonly_all = curr->readonly_all;
        node->ro_paths     = copy_path_list(curr->ro_paths);
        node->rw_paths     = copy_path_list(curr->rw_paths);
        node->deny_paths   = copy_path_list(curr->deny_paths);
        node->chroot_dir   = curr->chroot_dir ? strdup(curr->chroot_dir) : NULL;
        node->temp_dir     = curr->temp_dir ? strdup(curr->temp_dir) : NULL;
        node->temp_dir_cleanup = curr->temp_dir_cleanup;
        node->saved_cwd = curr->saved_cwd ? strdup(curr->saved_cwd) : NULL;

        if (!new_top) {
            new_top = node;
        } else if (prev_new) {
            prev_new->prev = node;
        }
        prev_new = node;
    }

    return new_top;
}

void fs_sandbox_free_stack(FSSandbox *stack) {
    while (stack) {
        FSSandbox *next = stack->prev;
        fs_sandbox_free(stack);
        stack = next;
    }
}

void fs_sandbox_push(Session *session, FSSandbox *sb) {
    if (!session || !sb)
        return;
    sb->prev            = session->fs_sandbox;
    session->fs_sandbox = sb;
}

FSSandbox *fs_sandbox_pop(Session *session) {
    if (!session || !session->fs_sandbox)
        return NULL;
    FSSandbox *top      = session->fs_sandbox;
    session->fs_sandbox = top->prev;
    top->prev           = NULL;
    return top;
}

/* -------------------------------------------------------------------------
 * Path Configuration
 * ------------------------------------------------------------------------- */

static void add_to_path_list(FSSandboxPath **head, const char *path) {
    if (!head || !path || path[0] == '\0')
        return;

    char *resolved = fs_sandbox_resolve_path(path, NULL);
    char *canon    = fs_sandbox_canonicalize(path, NULL);

    const char *paths_to_add[2] = {resolved, canon};
    for (int i = 0; i < 2; i++) {
        const char *p_add = paths_to_add[i];
        if (!p_add)
            continue;

        bool exists = false;
        for (FSSandboxPath *p = *head; p; p = p->next) {
            if (p->path && strcmp(p->path, p_add) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            FSSandboxPath *node = calloc(1, sizeof(FSSandboxPath));
            if (node) {
                node->path = strdup(p_add);
                node->next = *head;
                *head      = node;
            }
        }
    }
    free(resolved);
    free(canon);
}

void fs_sandbox_add_ro_path(FSSandbox *sb, const char *path) {
    if (!sb)
        return;
    add_to_path_list(&sb->ro_paths, path);
}

void fs_sandbox_add_rw_path(FSSandbox *sb, const char *path) {
    if (!sb)
        return;
    add_to_path_list(&sb->rw_paths, path);
}

void fs_sandbox_add_deny_path(FSSandbox *sb, const char *path) {
    if (!sb)
        return;
    add_to_path_list(&sb->deny_paths, path);
}

void fs_sandbox_add_paths(FSSandbox *sb, const char *path_list, int target) {
    if (!sb || !path_list || path_list[0] == '\0')
        return;

    char *copy = strdup(path_list);
    if (!copy)
        return;

    char *saveptr = NULL;
    char *token   = strtok_r(copy, ":,", &saveptr);
    while (token) {
        while (*token == ' ' || *token == '\t')
            token++;
        size_t tlen = strlen(token);
        while (tlen > 0 &&
               (token[tlen - 1] == ' ' || token[tlen - 1] == '\t')) {
            token[tlen - 1] = '\0';
            tlen--;
        }

        if (token[0] != '\0') {
            if (target == 0) {
                fs_sandbox_add_ro_path(sb, token);
            } else if (target == 1) {
                fs_sandbox_add_rw_path(sb, token);
            } else if (target == 2) {
                fs_sandbox_add_deny_path(sb, token);
            }
        }
        token = strtok_r(NULL, ":,", &saveptr);
    }

    free(copy);
}

void fs_sandbox_set_chroot(FSSandbox *sb, const char *path) {
    if (!sb)
        return;
    free(sb->chroot_dir);
    sb->chroot_dir = path ? fs_sandbox_resolve_path(path, NULL) : NULL;
}

/* -------------------------------------------------------------------------
 * Canonicalization & Path Matching
 * ------------------------------------------------------------------------- */

char *fs_sandbox_canonicalize(const char *path, const char *cwd) {
    if (!path || path[0] == '\0')
        return NULL;

    char combined[PATH_MAX * 2];
    if (path[0] == '/') {
        snprintf(combined, sizeof(combined), "%s", path);
    } else {
        const char *base = cwd;
        char        buf[PATH_MAX];
        if (!base) {
            if (getcwd(buf, sizeof(buf))) {
                base = buf;
            } else {
                base = "/";
            }
        }
        snprintf(combined, sizeof(combined), "%s/%s", base, path);
    }

    char  *segments[512];
    size_t seg_count = 0;

    char buf[PATH_MAX * 2];
    strncpy(buf, combined, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token   = strtok_r(buf, "/", &saveptr);
    while (token) {
        if (strcmp(token, ".") == 0 || token[0] == '\0') {
            // skip current dir
        } else if (strcmp(token, "..") == 0) {
            if (seg_count > 0) {
                seg_count--;
            }
        } else {
            if (seg_count < (sizeof(segments) / sizeof(segments[0]))) {
                segments[seg_count++] = token;
            }
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    if (seg_count == 0) {
        return strdup("/");
    }

    size_t total_len = 1;
    for (size_t i = 0; i < seg_count; i++) {
        total_len += strlen(segments[i]) + 1;
    }

    char *result = malloc(total_len + 1);
    if (!result)
        return NULL;

    result[0] = '\0';
    for (size_t i = 0; i < seg_count; i++) {
        strcat(result, "/");
        strcat(result, segments[i]);
    }

    return result;
}

char *fs_sandbox_resolve_path(const char *path, const char *cwd) {
    char *canon = fs_sandbox_canonicalize(path, cwd);
    if (!canon)
        return NULL;

    char real_buf[PATH_MAX];
    if (realpath(canon, real_buf)) {
        free(canon);
        return strdup(real_buf);
    }

    // If path does not exist, resolve parent directory
    char *last_slash = strrchr(canon, '/');
    if (last_slash && last_slash != canon) {
        *last_slash          = '\0';
        const char *filename = last_slash + 1;
        if (realpath(canon, real_buf)) {
            char combined[PATH_MAX];
            snprintf(combined, sizeof(combined), "%s/%s", real_buf, filename);
            free(canon);
            return strdup(combined);
        }
        *last_slash = '/';
    }

    return canon;
}

bool fs_sandbox_path_matches(const char *target, const char *prefix) {
    if (!target || !prefix)
        return false;
    if (strcmp(prefix, "/") == 0)
        return true;
    size_t plen = strlen(prefix);
    if (strcmp(target, prefix) == 0)
        return true;
    if (strncmp(target, prefix, plen) == 0 && target[plen] == '/')
        return true;
    return false;
}

/* -------------------------------------------------------------------------
 * Access Checking
 * ------------------------------------------------------------------------- */

bool fs_sandbox_check_access(Session *session, const char *path,
                             FSSandboxAccessType requested_access) {
    if (!session || !session->fs_sandbox || !path || path[0] == '\0')
        return true;

    char *resolved =
        fs_sandbox_resolve_path(path, session->current_working_dir);
    if (!resolved)
        return false;

    bool allowed = true;

    for (FSSandbox *sb = session->fs_sandbox; sb; sb = sb->prev) {
        if (!sb->is_active)
            continue;

        // 1. Chroot confinement
        if (sb->chroot_dir) {
            if (!fs_sandbox_path_matches(resolved, sb->chroot_dir)) {
                if (requested_access & FS_SANDBOX_EXEC) {
                    char jail_bin[PATH_MAX];
                    snprintf(jail_bin, sizeof(jail_bin), "%s/bin",
                             sb->chroot_dir);
                    if (access(jail_bin, F_OK) != 0) {
                        // Jail has no /bin of its own; allow running host
                        // binaries
                    } else {
                        allowed = false;
                        break;
                    }
                } else {
                    allowed = false;
                    break;
                }
            }
        }

        // 2. Deny paths
        bool denied = false;
        for (FSSandboxPath *dp = sb->deny_paths; dp; dp = dp->next) {
            if (fs_sandbox_path_matches(resolved, dp->path)) {
                denied = true;
                break;
            }
        }
        if (denied) {
            allowed = false;
            break;
        }

        // 3. Write access check
        if (requested_access & FS_SANDBOX_WRITE) {
            bool is_ro = false;
            for (FSSandboxPath *rp = sb->ro_paths; rp; rp = rp->next) {
                if (fs_sandbox_path_matches(resolved, rp->path)) {
                    is_ro = true;
                    break;
                }
            }

            bool is_rw = false;
            for (FSSandboxPath *wp = sb->rw_paths; wp; wp = wp->next) {
                if (fs_sandbox_path_matches(resolved, wp->path)) {
                    is_rw = true;
                    break;
                }
            }

            if (is_rw) {
                // Allowed explicitly
            } else if (is_ro || sb->readonly_all) {
                allowed = false;
                break;
            } else if (sb->rw_paths != NULL) {
                // If explicit rw_paths were specified, treat other paths as
                // non-writable
                allowed = false;
                break;
            }
        }
    }

    free(resolved);
    return allowed;
}

/* -------------------------------------------------------------------------
 * Child Process Sandboxing (macOS Seatbelt & Linux Landlock & Chroot)
 * ------------------------------------------------------------------------- */

void fs_sandbox_apply_child(Session *session) {
    if (!session || !session->fs_sandbox)
        return;

    // 1. Chroot if configured
    for (FSSandbox *sb = session->fs_sandbox; sb; sb = sb->prev) {
        if (sb->is_active && sb->chroot_dir) {
            char jail_bin[PATH_MAX];
            snprintf(jail_bin, sizeof(jail_bin), "%s/bin", sb->chroot_dir);
            if (access(jail_bin, F_OK) == 0) {
                if (chroot(sb->chroot_dir) == 0) {
                    if (chdir("/") != 0) {
                        // ignore
                    }
                }
            }
            break;
        }
    }

#if defined(__linux__) && defined(__NR_landlock_create_ruleset)
    // Gather rules and apply Landlock ruleset
    __u64 handled_access =
        LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE |
        LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_WRITE_FILE |
        LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |
        LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR |
        LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK |
        LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
        LANDLOCK_ACCESS_FS_MAKE_SYM;

    struct landlock_ruleset_attr attr = {
        .handled_access_fs = handled_access,
    };

    int ruleset_fd =
        syscall(__NR_landlock_create_ruleset, &attr, sizeof(attr), 0);
    if (ruleset_fd >= 0) {
        __u64 ro_access = LANDLOCK_ACCESS_FS_EXECUTE |
                          LANDLOCK_ACCESS_FS_READ_FILE |
                          LANDLOCK_ACCESS_FS_READ_DIR;
        __u64 rw_access = handled_access;

        for (FSSandbox *sb = session->fs_sandbox; sb; sb = sb->prev) {
            if (!sb->is_active)
                continue;

            if (sb->readonly_all) {
                int root_fd = open("/", O_PATH | O_CLOEXEC);
                if (root_fd >= 0) {
                    struct landlock_path_beneath_attr path_attr = {
                        .allowed_access = ro_access,
                        .parent_fd      = root_fd,
                    };
                    syscall(__NR_landlock_add_rule, ruleset_fd,
                            LANDLOCK_RULE_PATH_BENEATH, &path_attr, 0);
                    close(root_fd);
                }
            }

            for (FSSandboxPath *p = sb->ro_paths; p; p = p->next) {
                int fd = open(p->path, O_PATH | O_CLOEXEC);
                if (fd >= 0) {
                    struct landlock_path_beneath_attr path_attr = {
                        .allowed_access = ro_access,
                        .parent_fd      = fd,
                    };
                    syscall(__NR_landlock_add_rule, ruleset_fd,
                            LANDLOCK_RULE_PATH_BENEATH, &path_attr, 0);
                    close(fd);
                }
            }

            for (FSSandboxPath *p = sb->rw_paths; p; p = p->next) {
                int fd = open(p->path, O_PATH | O_CLOEXEC);
                if (fd >= 0) {
                    struct landlock_path_beneath_attr path_attr = {
                        .allowed_access = rw_access,
                        .parent_fd      = fd,
                    };
                    syscall(__NR_landlock_add_rule, ruleset_fd,
                            LANDLOCK_RULE_PATH_BENEATH, &path_attr, 0);
                    close(fd);
                }
            }
        }

        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0) {
            syscall(__NR_landlock_restrict_self, ruleset_fd, 0);
        }
        close(ruleset_fd);
    }
#endif

#if defined(__APPLE__)
    // Attempt Apple Seatbelt sandbox_init if appropriate
    // If entire filesystem is read-only or specific RW paths are set:
    bool needs_sb = false;
    for (FSSandbox *sb = session->fs_sandbox; sb; sb = sb->prev) {
        if (sb->is_active &&
            (sb->readonly_all || sb->ro_paths || sb->deny_paths)) {
            needs_sb = true;
            break;
        }
    }

    if (needs_sb) {
        char   profile[4096];
        size_t offset = 0;
        offset += snprintf(profile + offset, sizeof(profile) - offset,
                           "(version 1)\n"
                           "(deny default)\n"
                           "(allow process-exec)\n"
                           "(allow process-fork)\n"
                           "(allow sysctl-read)\n"
                           "(allow file-read*)\n");

        for (FSSandbox *sb = session->fs_sandbox; sb; sb = sb->prev) {
            if (!sb->is_active)
                continue;
            for (FSSandboxPath *p = sb->rw_paths; p; p = p->next) {
                if (offset < sizeof(profile) - 100) {
                    offset += snprintf(
                        profile + offset, sizeof(profile) - offset,
                        "(allow file-write* (subpath \"%s\"))\n", p->path);
                }
            }
            for (FSSandboxPath *p = sb->deny_paths; p; p = p->next) {
                if (offset < sizeof(profile) - 100) {
                    offset +=
                        snprintf(profile + offset, sizeof(profile) - offset,
                                 "(deny file* (subpath \"%s\"))\n", p->path);
                }
            }
        }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        char *err    = NULL;
        int   sb_ret = sandbox_init(profile, 0, &err);
        (void)sb_ret;
        if (err) {
            sandbox_free_error(err);
        }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    }
#endif
}

/* -------------------------------------------------------------------------
 * Recursive Directory Deletion
 * ------------------------------------------------------------------------- */

int fs_sandbox_rmtree(const char *path) {
    if (!path || path[0] == '\0')
        return -1;

    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char child_path[PATH_MAX];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(child_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                fs_sandbox_rmtree(child_path);
            } else {
                unlink(child_path);
            }
        }
    }

    closedir(d);
    return rmdir(path);
}
