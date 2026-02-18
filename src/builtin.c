#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* NULL */
#include <stdlib.h>  /* NULL */
#include <string.h>  /* strcmp */

#include "builtin.h" /* get_builtin, is_builtin, is_special_builtin, builtins */
#include "builtins/alias.h"    /* builtin_alias */
#include "builtins/cd.h"       /* builtin_cd */
#include "builtins/clear.h"    /* builtin_clear */
#include "builtins/eval.h"     /* builtin_eval */
#include "builtins/exit.h"     /* builtin_exit */
#include "builtins/export.h"   /* builtin_export */
#include "builtins/help.h"     /* builtin_help */
#include "builtins/history.h"  /* builtin_history */
#include "builtins/info.h"     /* builtin_info */
#include "builtins/popd.h"     /* builtin_popd */
#include "builtins/printenv.h" /* builtin_printenv */
#include "builtins/pushd.h"    /* builtin_pushd */
#include "builtins/pwd.h"      /* builtin_pwd */
#include "builtins/source.h"   /* builtin_source */
#include "builtins/terminal.h" /* builtin_terminal */
#include "builtins/type.h"     /* builtin_type */
#include "builtins/unalias.h"  /* builtin_unalias */
#include "builtins/which.h"    /* builtin_which */
#include "session.h"           /* Session */

const char *builtins[] = {
    "cd",    "exit",   "pwd",    "clear",   "history", "help",     "printenv",
    "which", "export", "alias",  "unalias", "eval",    "terminal", "info",
    "pushd", "popd",   "source", "type",    NULL};

int (*get_builtin(const char *name))(int argc, char **argv, Session *session) {
    if (strcmp(name, "cd") == 0)
        return builtin_cd;
    if (strcmp(name, "exit") == 0)
        return builtin_exit;
    if (strcmp(name, "pwd") == 0)
        return builtin_pwd;
    if (strcmp(name, "clear") == 0)
        return builtin_clear;
    if (strcmp(name, "history") == 0)
        return builtin_history;
    if (strcmp(name, "help") == 0)
        return builtin_help;
    if (strcmp(name, "printenv") == 0)
        return builtin_printenv;
    if (strcmp(name, "which") == 0)
        return builtin_which;
    if (strcmp(name, "export") == 0)
        return builtin_export;
    if (strcmp(name, "alias") == 0)
        return builtin_alias;
    if (strcmp(name, "unalias") == 0)
        return builtin_unalias;
    if (strcmp(name, "eval") == 0)
        return builtin_eval;
    if (strcmp(name, "terminal") == 0)
        return builtin_terminal;
    if (strcmp(name, "info") == 0)
        return builtin_info;
    if (strcmp(name, "pushd") == 0)
        return builtin_pushd;
    if (strcmp(name, "popd") == 0)
        return builtin_popd;
    if (strcmp(name, "source") == 0)
        return builtin_source;
    if (strcmp(name, ".") == 0)
        return builtin_source;
    if (strcmp(name, "type") == 0)
        return builtin_type;
    return NULL;
}

bool is_builtin(const char *name) { return get_builtin(name) != NULL; }

bool is_special_builtin(const char *name) {
    return (strcmp(name, "cd") == 0 || strcmp(name, "exit") == 0 ||
            strcmp(name, "history") == 0 || strcmp(name, "export") == 0 ||
            strcmp(name, "info") == 0 || strcmp(name, "alias") == 0 ||
            strcmp(name, "unalias") == 0 || strcmp(name, "eval") == 0 ||
            strcmp(name, "terminal") == 0 || strcmp(name, "pushd") == 0 ||
            strcmp(name, "popd") == 0 || strcmp(name, "source") == 0 ||
            strcmp(name, ".") == 0 || strcmp(name, "type") == 0);
}
