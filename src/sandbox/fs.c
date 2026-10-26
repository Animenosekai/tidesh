/** fs.c
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

static FSSandboxPath *copy_path_list(const FSSandboxPath *head) {
    if (!head)
        return NULL;
    FSSandboxPath  *new_head = NULL;
    FSSandboxPath **tail     = &new_head;
    for (const FSSandboxPath *curr = head; curr; curr = curr->next) {
        FSSandboxPath *node = calloc(1, sizeof(FSSandboxPath));
        if (!node)
            break;
        node->path = curr->path ? strdup(curr->path) : NULL;
        node->next = NULL;
        *tail      = node;
        tail       = &node->next;
    }
    return new_head;
}

FSSandbox *fs_sandbox_copy_stack(const FSSandbox *stack) {
    if (!stack)
        return NULL;

    FSSandbox  *head = NULL;
    FSSandbox **tail = &head;

    for (const FSSandbox *curr = stack; curr; curr = curr->prev) {
        FSSandbox *copy = fs_sandbox_create();
        if (!copy)
            break;

        copy->is_active    = curr->is_active;
        copy->readonly_all = curr->readonly_all;
        copy->ro_paths     = copy_path_list(curr->ro_paths);
        copy->rw_paths     = copy_path_list(curr->rw_paths);
        copy->deny_paths   = copy_path_list(curr->deny_paths);
        copy->chroot_dir   = curr->chroot_dir ? strdup(curr->chroot_dir) : NULL;
        copy->temp_dir     = curr->temp_dir ? strdup(curr->temp_dir) : NULL;
        copy->temp_dir_cleanup = curr->temp_dir_cleanup;
        copy->saved_cwd = curr->saved_cwd ? strdup(curr->saved_cwd) : NULL;

        *tail = copy;
        tail  = &copy->prev;
    }

    return head;
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

    char *buf = strdup(path_list);
    if (!buf)
        return;

    char *saveptr = NULL;
    char *token   = strtok_r(buf, ":,", &saveptr);
    while (token) {
        // Trim leading and trailing whitespace
        while (*token == ' ' || *token == '\t')
            token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
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
    free(buf);
}

void fs_sandbox_set_chroot(FSSandbox *sb, const char *path) {
    if (!sb)
        return;
    free(sb->chroot_dir);
    if (path && path[0] != '\0') {
        sb->chroot_dir = fs_sandbox_canonicalize(path, NULL);
        if (!sb->chroot_dir) {
            sb->chroot_dir = strdup(path);
        }
    } else {
        sb->chroot_dir = NULL;
    }
}

/* -------------------------------------------------------------------------
 * Canonicalization & Path Matching
 * ------------------------------------------------------------------------- */

char *fs_sandbox_canonicalize(const char *path, const char *cwd) {
    if (!path || path[0] == '\0')
        return NULL;

    char combined[PATH_MAX * 2];
    if (path[0] == '/') {
        strncpy(combined, path, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
    } else {
        char cwd_buf[PATH_MAX];
        if (!cwd) {
            if (!getcwd(cwd_buf, sizeof(cwd_buf))) {
                cwd_buf[0] = '/';
                cwd_buf[1] = '\0';
            }
            cwd = cwd_buf;
        }
        snprintf(combined, sizeof(combined), "%s/%s", cwd, path);
    }

    char *segments[PATH_MAX / 2];
    int   seg_count = 0;

    char *p = combined;
    while (*p == '/')
        p++;

    while (*p != '\0') {
        char *start = p;
        while (*p != '/' && *p != '\0')
            p++;
        if (*p != '\0') {
            *p = '\0';
            p++;
            while (*p == '/')
                p++;
        }

        if (strcmp(start, ".") == 0 || start[0] == '\0') {
            continue;
        } else if (strcmp(start, "..") == 0) {
            if (seg_count > 0)
                seg_count--;
        } else {
            segments[seg_count++] = start;
        }
    }

    if (seg_count == 0)
        return strdup("/");

    char   out[PATH_MAX];
    size_t offset = 0;
    out[0]        = '\0';

    for (int i = 0; i < seg_count; i++) {
        offset +=
            snprintf(out + offset, sizeof(out) - offset, "/%s", segments[i]);
        if (offset >= sizeof(out) - 1)
            break;
    }

    return strdup(out);
}

char *fs_sandbox_resolve_path(const char *path, const char *cwd) {
    if (!path)
        return NULL;

    char combined[PATH_MAX * 2];
    if (path[0] == '/') {
        strncpy(combined, path, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
    } else {
        char cwd_buf[PATH_MAX];
        if (!cwd) {
            if (!getcwd(cwd_buf, sizeof(cwd_buf))) {
                cwd_buf[0] = '/';
                cwd_buf[1] = '\0';
            }
            cwd = cwd_buf;
        }
        snprintf(combined, sizeof(combined), "%s/%s", cwd, path);
    }

    char resolved[PATH_MAX];
    if (realpath(combined, resolved)) {
        return strdup(resolved);
    }

    return fs_sandbox_canonicalize(path, cwd);
}

bool fs_sandbox_path_matches(const char *target, const char *prefix) {
    if (!target || !prefix)
        return false;

    if (strcmp(prefix, "/") == 0)
        return true;

    size_t p_len = strlen(prefix);
    size_t t_len = strlen(target);

    if (t_len < p_len)
        return false;

    if (strncmp(target, prefix, p_len) == 0) {
        if (target[p_len] == '\0' || target[p_len] == '/')
            return true;
    }

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

#if defined(__APPLE__)
static int safe_sandbox_init(const char *profile) {
    char *err          = NULL;
    int   saved_stderr = dup(STDERR_FILENO);
    int   dev_null     = open("/dev/null", O_WRONLY);
    if (dev_null >= 0) {
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
    }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    int ret = sandbox_init(profile, 0, &err);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    if (err) {
        sandbox_free_error(err);
    }
    return ret;
}
#endif

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

        safe_sandbox_init(profile);
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
