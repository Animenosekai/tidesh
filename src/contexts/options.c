/** options.c
 *
 * Implementation of the 'options' built-in context manager.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "contexts/options.h"
#include "session.h"

int context_enter_options(Session *session, ContextArg *args, size_t argc,
                          void **state) {
    ShellOptions *saved = malloc(sizeof(ShellOptions));
    if (!saved)
        return 1;
    *saved = session->options;

    for (size_t i = 0; i < argc; i++) {
        const char *opt = args[i].value;
        if (!opt)
            continue;

        bool enable = true;
        if (opt[0] == '+') {
            enable = false;
            opt++;
        } else if (opt[0] == '-') {
            enable = true;
            opt++;
        }

        if (strcmp(opt, "e") == 0 || strcmp(opt, "errexit") == 0) {
            session->options.errexit = enable;
        } else if (strcmp(opt, "x") == 0 || strcmp(opt, "xtrace") == 0) {
            session->options.xtrace = enable;
        } else if (strcmp(opt, "v") == 0 || strcmp(opt, "verbose") == 0) {
            session->options.verbose = enable;
        } else if (strcmp(opt, "u") == 0 || strcmp(opt, "nounset") == 0) {
            session->options.nounset = enable;
        } else if (strcmp(opt, "f") == 0 || strcmp(opt, "noglob") == 0) {
            session->options.noglob = enable;
        } else if (strcmp(opt, "a") == 0 || strcmp(opt, "allexport") == 0) {
            session->options.allexport = enable;
        } else if (strcmp(opt, "pipefail") == 0) {
            session->options.pipefail = enable;
        }
    }

    *state = saved;
    return 0;
}

int context_exit_options(Session *session, int status, void *state) {
    (void)status;
    ShellOptions *saved = (ShellOptions *)state;
    if (saved) {
        session->options = *saved;
        free(saved);
    }
    return 0;
}
