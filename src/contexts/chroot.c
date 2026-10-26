/** chroot.c
 *
 * Implementation of the 'chroot' built-in context manager.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "context.h"
#include "contexts/chroot.h"
#include "sandbox.h"
#include "session.h"

typedef struct ChrootState {
    char      *jail_path;
    char      *saved_cwd;
    FSSandbox *sandbox_layer;
} ChrootState;

int context_enter_chroot(Session *session, ContextArg *args, size_t argc,
                         void **state) {
    if (!session)
        return 1;

    const char *path     = NULL;
    bool        readonly = false;

    // First find the path
    for (size_t i = 0; i < argc; i++) {
        const char *key = args[i].key;
        const char *val = args[i].value ? args[i].value : "";

        if (key) {
            if (strcmp(key, "path") == 0 || strcmp(key, "root") == 0 ||
                strcmp(key, "dir") == 0) {
                path = val;
            }
        } else if (val[0] != '\0' && !path && strncmp(val, "rw=", 3) != 0 &&
                   strncmp(val, "ro=", 3) != 0 &&
                   strncmp(val, "deny=", 5) != 0 &&
                   strcmp(val, "readonly") != 0) {
            path = val;
        }
    }

    if (!path) {
        fprintf(stderr, "tidesh: chroot: directory path not specified\n");
        return 1;
    }

    char *canon = fs_sandbox_resolve_path(path, session->current_working_dir);
    if (!canon) {
        fprintf(stderr, "tidesh: chroot: invalid path '%s'\n", path);
        return 1;
    }

    struct stat st;
    if (stat(canon, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr,
                "tidesh: chroot: '%s' is not a directory or does not exist\n",
                canon);
        free(canon);
        return 1;
    }

    ChrootState *cs = calloc(1, sizeof(ChrootState));
    if (!cs) {
        free(canon);
        return 1;
    }

    cs->jail_path = canon;

    char buf[PATH_MAX];
    if (session->current_working_dir) {
        cs->saved_cwd = strdup(session->current_working_dir);
    } else if (getcwd(buf, sizeof(buf))) {
        cs->saved_cwd = strdup(buf);
    }

    if (chdir(canon) != 0) {
        fprintf(stderr, "tidesh: chroot: failed to chdir to %s: %s\n", canon,
                strerror(errno));
        free(cs->jail_path);
        free(cs->saved_cwd);
        free(cs);
        return 1;
    }

    update_working_dir(session);

    FSSandbox *sb = fs_sandbox_create();
    if (sb) {
        fs_sandbox_set_chroot(sb, canon);

        // Check for readonly / rw / ro / deny parameters
        bool has_explicit_rw = false;
        for (size_t i = 0; i < argc; i++) {
            const char *key = args[i].key;
            const char *val = args[i].value ? args[i].value : "";

            if (key) {
                if (strcmp(key, "readonly") == 0 || strcmp(key, "ro") == 0) {
                    if (val[0] == '\0' || strcmp(val, "true") == 0 ||
                        strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) {
                        readonly = true;
                    } else {
                        fs_sandbox_add_paths(sb, val, 0);
                    }
                } else if (strcmp(key, "rw") == 0 ||
                           strcmp(key, "allow_write") == 0) {
                    fs_sandbox_add_paths(sb, val, 1);
                    has_explicit_rw = true;
                } else if (strcmp(key, "deny") == 0) {
                    fs_sandbox_add_paths(sb, val, 2);
                }
            } else {
                if (strcmp(val, "readonly") == 0) {
                    readonly = true;
                } else if (strncmp(val, "ro=", 3) == 0) {
                    if (strcmp(val + 3, "true") == 0) {
                        readonly = true;
                    } else {
                        fs_sandbox_add_paths(sb, val + 3, 0);
                    }
                } else if (strncmp(val, "rw=", 3) == 0) {
                    fs_sandbox_add_paths(sb, val + 3, 1);
                    has_explicit_rw = true;
                } else if (strncmp(val, "deny=", 5) == 0) {
                    fs_sandbox_add_paths(sb, val + 5, 2);
                }
            }
        }

        if (readonly) {
            sb->readonly_all = true;
        } else if (!has_explicit_rw) {
            // By default, the chroot jail itself has read-write access
            fs_sandbox_add_rw_path(sb, canon);
        }

        fs_sandbox_push(session, sb);
        cs->sandbox_layer = sb;
    }

    *state = cs;
    return 0;
}

int context_exit_chroot(Session *session, int status, void *state) {
    (void)status;
    ChrootState *cs = (ChrootState *)state;
    if (!session || !cs)
        return 0;

    // Pop sandbox layer BEFORE restoring working directory so cd isn't blocked
    if (cs->sandbox_layer) {
        FSSandbox *popped = fs_sandbox_pop(session);
        if (popped) {
            fs_sandbox_free(popped);
        }
    }

    if (cs->saved_cwd) {
        if (chdir(cs->saved_cwd) != 0) {
            fprintf(stderr, "tidesh: chroot: failed to restore cwd %s: %s\n",
                    cs->saved_cwd, strerror(errno));
        }
        update_working_dir(session);
    }

    free(cs->jail_path);
    free(cs->saved_cwd);
    free(cs);
    return 0;
}
