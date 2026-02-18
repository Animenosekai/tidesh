#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* NULL */
#include <stdlib.h>  /* NULL */
#include <string.h>  /* strcmp */

#include "builtin.h" /* get_builtin, is_builtin, is_special_builtin, builtins */
#ifndef TIDESH_DISABLE_ALIASES
#include "builtins/alias.h"   /* builtin_alias */
#include "builtins/unalias.h" /* builtin_unalias */
#endif
#include "builtins/clear.h"    /* builtin_clear */
#include "builtins/eval.h"     /* builtin_eval */
#include "builtins/exit.h"     /* builtin_exit */
#include "builtins/export.h"   /* builtin_export */
#include "builtins/help.h"     /* builtin_help */
#include "builtins/hooks.h"    /* builtin_hooks */
#include "builtins/info.h"     /* builtin_info */
#include "builtins/printenv.h" /* builtin_printenv */
#include "builtins/pwd.h"      /* builtin_pwd */
#include "builtins/source.h"   /* builtin_source */
#include "builtins/terminal.h" /* builtin_terminal */
#include "builtins/test.h"     /* builtin_test */
#include "builtins/type.h"     /* builtin_type */
#include "builtins/which.h"    /* builtin_which */

#ifndef TIDESH_DISABLE_HISTORY
#include "builtins/history.h" /* builtin_history */
#endif

#ifndef TIDESH_DISABLE_DIRSTACK
#include "builtins/cd.h"    /* builtin_cd */
#include "builtins/popd.h"  /* builtin_popd */
#include "builtins/pushd.h" /* builtin_pushd */
#endif

#ifndef TIDESH_DISABLE_JOB_CONTROL
#include "builtins/bg.h"   /* builtin_bg */
#include "builtins/fg.h"   /* builtin_fg */
#include "builtins/jobs.h" /* builtin_jobs */
#endif
#include "session.h" /* Session */

const char *builtins[] = {"exit",    "pwd",     "clear", "help",     "printenv",
                          "which",   "export",  "eval",  "terminal", "info",
                          "source",  "type",    "test",  "hooks",
#ifndef TIDESH_DISABLE_ALIASES
                          "alias",   "unalias",
#endif
#ifndef TIDESH_DISABLE_HISTORY
                          "history",
#endif
#ifndef TIDESH_DISABLE_DIRSTACK
                          "cd",      "pushd",   "popd",
#endif
#ifndef TIDESH_DISABLE_JOB_CONTROL
                          "jobs",    "fg",      "bg",
#endif
                          NULL};

int (*get_builtin(const char *name))(int argc, char **argv, Session *session) {
    if (strcmp(name, "exit") == 0)
        return builtin_exit;
    if (strcmp(name, "pwd") == 0)
        return builtin_pwd;
    if (strcmp(name, "clear") == 0)
        return builtin_clear;
#ifndef TIDESH_DISABLE_HISTORY
    if (strcmp(name, "history") == 0)
        return builtin_history;
#endif
    if (strcmp(name, "help") == 0)
        return builtin_help;
    if (strcmp(name, "hooks") == 0)
        return builtin_hooks;
    if (strcmp(name, "printenv") == 0)
        return builtin_printenv;
    if (strcmp(name, "which") == 0)
        return builtin_which;
    if (strcmp(name, "export") == 0)
        return builtin_export;
#ifndef TIDESH_DISABLE_ALIASES
    if (strcmp(name, "alias") == 0)
        return builtin_alias;
    if (strcmp(name, "unalias") == 0)
        return builtin_unalias;
#endif
    if (strcmp(name, "eval") == 0)
        return builtin_eval;
    if (strcmp(name, "terminal") == 0)
        return builtin_terminal;
    if (strcmp(name, "info") == 0)
        return builtin_info;
#ifndef TIDESH_DISABLE_DIRSTACK
    if (strcmp(name, "cd") == 0)
        return builtin_cd;
    if (strcmp(name, "pushd") == 0)
        return builtin_pushd;
    if (strcmp(name, "popd") == 0)
        return builtin_popd;
#endif
    if (strcmp(name, "source") == 0)
        return builtin_source;
    if (strcmp(name, ".") == 0)
        return builtin_source;
    if (strcmp(name, "type") == 0)
        return builtin_type;
    if (strcmp(name, "test") == 0)
        return builtin_test;
    if (strcmp(name, "[") == 0)
        return builtin_test;
#ifndef TIDESH_DISABLE_JOB_CONTROL
    if (strcmp(name, "jobs") == 0)
        return builtin_jobs;
    if (strcmp(name, "fg") == 0)
        return builtin_fg;
    if (strcmp(name, "bg") == 0)
        return builtin_bg;
#endif
    return NULL;
}

bool is_builtin(const char *name) { return get_builtin(name) != NULL; }

bool is_special_builtin(const char *name) {
    if (strcmp(name, "exit") == 0 || strcmp(name, "export") == 0 ||
        strcmp(name, "info") == 0 || strcmp(name, "eval") == 0 ||
        strcmp(name, "terminal") == 0 || strcmp(name, "source") == 0 ||
        strcmp(name, ".") == 0 || strcmp(name, "type") == 0 ||
        strcmp(name, "hooks") == 0) {
        return true;
    }
#ifndef TIDESH_DISABLE_ALIASES
    if (strcmp(name, "alias") == 0 || strcmp(name, "unalias") == 0) {
        return true;
    }
#endif
#ifndef TIDESH_DISABLE_HISTORY
    if (strcmp(name, "history") == 0) {
        return true;
    }
#endif
#ifndef TIDESH_DISABLE_DIRSTACK
    if (strcmp(name, "cd") == 0 || strcmp(name, "pushd") == 0 ||
        strcmp(name, "popd") == 0) {
        return true;
    }
#endif
#ifndef TIDESH_DISABLE_JOB_CONTROL
    if (strcmp(name, "jobs") == 0 || strcmp(name, "fg") == 0 ||
        strcmp(name, "bg") == 0) {
        return true;
    }
#endif
    return false;
}
