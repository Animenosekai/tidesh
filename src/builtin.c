#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"
#include "builtins/alias.h"
#include "builtins/cd.h"
#include "builtins/clear.h"
#include "builtins/eval.h"
#include "builtins/exit.h"
#include "builtins/export.h"
#include "builtins/help.h"
#include "builtins/history.h"
#include "builtins/info.h"
#include "builtins/popd.h"
#include "builtins/printenv.h"
#include "builtins/pushd.h"
#include "builtins/pwd.h"
#include "builtins/terminal.h"
#include "builtins/unalias.h"
#include "builtins/which.h"
#include "commands/myarp.h"
#include "commands/mydelexe.h"
#include "commands/mydump.h"
#include "commands/myenv.h"
#include "commands/myexe.h"
#include "commands/myinfo.h"
#include "commands/mylof.h"
#include "commands/mymaps.h"
#include "commands/mynetstat.h"
#include "commands/myps.h"
#include "commands/mypstree.h"
#include "session.h"

const char *builtins[] = {
    "cd",        "exit",       "pwd",    "clear",    "history", "help",
    "printenv",  "which",      "export", "alias",    "unalias", "eval",
    "terminal",  "info",       "pushd",  "popd",     "myps",    "mypstree",
    "mynetstat", "mynetstat2", "myarp",  "myexe",    "mylof",   "myinfo",
    "myenv",     "mymaps",     "mydump", "mydelexe", NULL};

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
    if (strcmp(name, "myps") == 0)
        return builtin_myps;
    if (strcmp(name, "mypstree") == 0)
        return builtin_mypstree;
    if (strcmp(name, "mynetstat") == 0)
        return builtin_mynetstat;
    if (strcmp(name, "mynetstat2") == 0)
        return builtin_mynetstat;
    if (strcmp(name, "myarp") == 0)
        return builtin_myarp;
    if (strcmp(name, "myexe") == 0)
        return builtin_myexe;
    if (strcmp(name, "mylof") == 0)
        return builtin_mylof;
    if (strcmp(name, "myinfo") == 0)
        return builtin_myinfo;
    if (strcmp(name, "myenv") == 0)
        return builtin_myenv;
    if (strcmp(name, "mymaps") == 0)
        return builtin_mymaps;
    if (strcmp(name, "mydump") == 0)
        return builtin_mydump;
    if (strcmp(name, "mydelexe") == 0)
        return builtin_mydelexe;
    return NULL;
}

bool is_builtin(const char *name) { return get_builtin(name) != NULL; }

bool is_special_builtin(const char *name) {
    return (strcmp(name, "cd") == 0 || strcmp(name, "exit") == 0 ||
            strcmp(name, "history") == 0 || strcmp(name, "export") == 0 ||
            strcmp(name, "info") == 0 || strcmp(name, "alias") == 0 ||
            strcmp(name, "unalias") == 0 || strcmp(name, "eval") == 0 ||
            strcmp(name, "terminal") == 0 || strcmp(name, "pushd") == 0 ||
            strcmp(name, "popd") == 0);
}
