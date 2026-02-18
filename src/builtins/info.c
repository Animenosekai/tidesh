#include <stdbool.h> /* bool */
#include <stdio.h>   /* printf */
#include <unistd.h>  /* getpid, getppid */

#include "builtins/info.h"
#include "environ.h"     /* environ_get_default */
#include "prompt/ansi.h" /* ANSI color constants */
#include "session.h"     /* Session */

int builtin_info(int argc, char **argv, Session *session) {
    (void)argc;
    (void)argv;

    bool use_colors = (session && session->terminal)
                          ? session->terminal->supports_colors
                          : false;

    const char *l_clr = use_colors ? ANSI_BOLD : "";
    const char *reset = use_colors ? ANSI_COLOR_RESET : "";

    /* Build Information */
#ifdef PROJECT_NAME
    printf("%sName:        %s %s\n", l_clr, reset, PROJECT_NAME);
#endif

#ifdef VERSION
    printf("%sVersion:     %s %s\n", l_clr, reset, VERSION);
#endif

#ifdef RAW_VERSION
    printf("%sRaw Version: %s %s\n", l_clr, reset, RAW_VERSION);
#endif

#ifdef GIT_VERSION
    printf("%sGit Revision:%s %s\n", l_clr, reset, GIT_VERSION);
#endif

#ifdef BUILD_DATE
    printf("%sBuild Date:  %s %s\n", l_clr, reset, BUILD_DATE);
#endif

#ifdef PLATFORM
    printf("%sPlatform:    %s %s\n", l_clr, reset, PLATFORM);
#endif

#ifdef __VERSION__
    printf("%sCompiler:    %s %s\n", l_clr, reset, __VERSION__);
#endif

#ifdef NDEBUG
    printf("%sBuild Type:  %s %s\n", l_clr, reset, "Release");
#else
    printf("%sBuild Type:  %s %s\n", l_clr, reset, "Debug");
#endif

    /* Runtime Information */
    printf("%sShell PID:   %s %d\n", l_clr, reset, getpid());
    printf("%sShell PPID:  %s %d\n", l_clr, reset, getppid());

    char *shlvl = environ_get_default(session->environ, "SHLVL", "N/A");
    printf("%sShell Level: %s %s\n", l_clr, reset, shlvl);

    char *shell_path = environ_get_default(session->environ, "SHELL", "N/A");
    printf("%sShell Path:  %s %s\n", l_clr, reset, shell_path);
    return 0;
}
