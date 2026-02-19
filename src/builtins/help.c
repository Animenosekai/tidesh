#include <stdbool.h> /* bool, true, false */
#include <stdio.h>   /* printf */
#include <string.h>  /* strcmp */

#include "builtins/help.h"
#include "feature-flags.h" /* TIDESH_DISABLE_* */
#include "prompt/ansi.h"   /* ANSI color constants */
#include "session.h"       /* Session */

int builtin_help(int argc, char **argv, Session *session) {
    bool cd       = false;
    bool clear    = false;
    bool exit     = false;
    bool export   = false;
    bool eval     = false;
    bool alias    = false;
    bool unalias  = false;
    bool help     = false;
    bool features = false;
    bool hooks    = false;
    bool history  = false;
    bool info     = false;
    bool printenv = false;
    bool pwd      = false;
    bool pushd    = false;
    bool popd     = false;
    bool terminal = false;
    bool which    = false;
    bool source   = false;
    bool type     = false;
    bool test     = false;
    bool jobs     = false;
    bool fg       = false;
    bool bg       = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "cd") == 0)
            cd = true;
        else if (strcmp(argv[i], "clear") == 0)
            clear = true;
        else if (strcmp(argv[i], "exit") == 0)
            exit = true;
        else if (strcmp(argv[i], "export") == 0)
            export = true;
        else if (strcmp(argv[i], "eval") == 0)
            eval = true;
        else if (strcmp(argv[i], "alias") == 0)
            alias = true;
        else if (strcmp(argv[i], "unalias") == 0)
            unalias = true;
        else if (strcmp(argv[i], "help") == 0)
            help = true;
        else if (strcmp(argv[i], "features") == 0)
            features = true;
        else if (strcmp(argv[i], "hooks") == 0)
            hooks = true;
#ifndef TIDESH_DISABLE_HISTORY
        else if (strcmp(argv[i], "history") == 0)
            history = true;
#endif
        else if (strcmp(argv[i], "info") == 0)
            info = true;
        else if (strcmp(argv[i], "printenv") == 0)
            printenv = true;
        else if (strcmp(argv[i], "pwd") == 0)
            pwd = true;
#ifndef TIDESH_DISABLE_DIRSTACK
        else if (strcmp(argv[i], "pushd") == 0)
            pushd = true;
        else if (strcmp(argv[i], "popd") == 0)
            popd = true;
#endif
        else if (strcmp(argv[i], "terminal") == 0)
            terminal = true;
        else if (strcmp(argv[i], "which") == 0)
            which = true;
        else if (strcmp(argv[i], "source") == 0 || strcmp(argv[i], ".") == 0)
            source = true;
        else if (strcmp(argv[i], "type") == 0)
            type = true;
        else if (strcmp(argv[i], "test") == 0 || strcmp(argv[i], "[") == 0)
            test = true;
#ifndef TIDESH_DISABLE_JOB_CONTROL
        else if (strcmp(argv[i], "jobs") == 0)
            jobs = true;
        else if (strcmp(argv[i], "fg") == 0)
            fg = true;
        else if (strcmp(argv[i], "bg") == 0)
            bg = true;
#endif
    }

    bool all = !(cd || clear || exit || export || eval || alias || unalias ||
                 help || features || hooks || history || info || printenv ||
                 pwd || pushd || popd || terminal || which || source || type ||
                 test || jobs || fg || bg);

    bool use_colors = (session && session->terminal)
                          ? session->terminal->supports_colors
                          : false;

    const char *header_clr     = use_colors ? ANSI_BOLD ANSI_BLUE : "";
    const char *command_clr    = use_colors ? ANSI_BOLD ANSI_GREEN : "";
    const char *argument_clr   = use_colors ? ANSI_CYAN : "";
    const char *subcommand_clr = use_colors ? ANSI_ITALIC ANSI_DIM : "";
    const char *reset          = use_colors ? ANSI_COLOR_RESET : "";

    if (all)
        printf("%sBuilt-in commands%s\n", header_clr, reset);

    if (all || cd)
        printf("  %s%-9s %s%-14s%s - Change the current directory\n",
               command_clr, "cd", argument_clr, "[dir?]", reset);

    if (all || clear)
        printf("  %s%-9s %s%-14s%s - Clear the terminal screen\n", command_clr,
               "clear", argument_clr, "", reset);

    if (all || exit)
        printf("  %s%-9s %s%-14s%s - Exit the shell with optional exit code\n",
               command_clr, "exit", argument_clr, "[code?]", reset);

    if (all || export)
        printf("  %s%-9s %s%-14s%s - Set environment variable\n", command_clr,
               "export", argument_clr, "[key]=[value]", reset);

    if (all || eval)
        printf("  %s%-9s %s%-14s%s - Execute arguments as a command\n",
               command_clr, "eval", argument_clr, "[args...]", reset);

    if (all || alias)
        printf("  %s%-9s %s%-14s%s - List or set command aliases\n",
               command_clr, "alias", argument_clr, "[name[=val]]", reset);

    if (all || unalias)
        printf("  %s%-9s %s%-14s%s - Remove command aliases\n", command_clr,
               "unalias", argument_clr, "name", reset);

    if (all || help)
        printf("  %s%-9s %s%-14s%s - Show this help message\n", command_clr,
               "help", argument_clr, "", reset);

    if (all || features)
        printf("  %s%-9s %s%-14s%s - Show or manage feature flags\n",
               command_clr, "features", argument_clr, "[subcommand]", reset);

    if (all || features)
        printf("                             %sSubcommands: list, status, "
               "enable, disable%s\n",
               subcommand_clr, reset);

    if (all || hooks)
        printf("  %s%-9s %s%-14s%s - Show or manage hooks\n", command_clr,
               "hooks", argument_clr, "[subcommand]", reset);

    if (all || hooks)
        printf("                             %sSubcommands: list, enable, "
               "disable, status, run, path, types%s\n",
               subcommand_clr, reset);

#ifndef TIDESH_DISABLE_HISTORY
    if (all || history)
        printf("  %s%-9s %s%-14s%s - Show or manage command history\n",
               command_clr, "history", argument_clr, "[subcommand]", reset);

    if (all || history)
        printf("                             %sSubcommands: disable, enable, "
               "status, "
               "size, clear, limit %s[num]%s%s, file %s[path]%s\n",
               subcommand_clr, argument_clr, reset, subcommand_clr,
               argument_clr, reset);
#endif

    if (all || info)
        printf("  %s%-9s %s%-14s%s - Show shell and build information\n",
               command_clr, "info", argument_clr, "", reset);

    if (all || printenv)
        printf("  %s%-9s %s%-14s%s - Print environment variables\n",
               command_clr, "printenv", argument_clr, "", reset);

    if (all || pwd)
        printf("  %s%-9s %s%-14s%s - Print the current working directory\n",
               command_clr, "pwd", argument_clr, "", reset);

#ifndef TIDESH_DISABLE_DIRSTACK
    if (all || pushd)
        printf("  %s%-9s %s%-14s%s - Push directory to stack and change CWD\n",
               command_clr, "pushd", argument_clr, "[dir|+N]", reset);

    if (all || popd)
        printf("  %s%-9s %s%-14s%s - Pop directory from stack and change CWD\n",
               command_clr, "popd", argument_clr, "", reset);
#endif

    if (all || terminal)
        printf("  %s%-9s %s%-14s%s - Show or manage terminal settings\n",
               command_clr, "terminal", argument_clr, "[subcommand]", reset);

    if (all || terminal)
        printf("                             %sSubcommands: colors "
               "%s[enable|disable]%s\n",
               subcommand_clr, argument_clr, reset);

    if (all || which)
        printf("  %s%-9s %s%-14s%s - Locate a command in PATH\n", command_clr,
               "which", argument_clr, "[command]", reset);

    if (all || source)
        printf("  %s%-9s %s%-14s%s - Execute commands from a file\n",
               command_clr, "source", argument_clr, "<file>", reset);

    if (all || source)
        printf(
            "  %s%-9s %s%-14s%s - Execute commands from a file (shorthand)\n",
            command_clr, ".", argument_clr, "<file>", reset);

    if (all || type)
        printf("  %s%-9s %s%-14s%s - Show the type of a command\n", command_clr,
               "type", argument_clr, "[command...]", reset);

    if (all || test)
        printf("  %s%-9s %s%-14s%s - Evaluate conditional expressions\n",
               command_clr, "test", argument_clr, "[expr]", reset);

    if (all || test)
        printf("  %s%-9s %s%-14s%s - Evaluate conditional expressions\n",
               command_clr, "[", argument_clr, "expr ]", reset);

#ifndef TIDESH_DISABLE_JOB_CONTROL
    if (all || jobs)
        printf("  %s%-9s %s%-14s%s - List background jobs\n", command_clr,
               "jobs", argument_clr, "", reset);

    if (all || fg)
        printf("  %s%-9s %s%-14s%s - Bring a job to the foreground\n",
               command_clr, "fg", argument_clr, "[job_id?]", reset);

    if (all || bg)
        printf("  %s%-9s %s%-14s%s - Continue a stopped job in background\n",
               command_clr, "bg", argument_clr, "[job_id?]", reset);
#endif

    return 0;
}
