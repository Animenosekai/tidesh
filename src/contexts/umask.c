/** umask.c
 *
 * Implementation of the 'umask' built-in context manager.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "context.h"
#include "contexts/umask.h"
#include "session.h"

int context_enter_umask(Session *session, ContextArg *args, size_t argc,
                        void **state) {
    (void)session;
    mode_t current_mask = umask(0);
    umask(current_mask); // restore immediately to query

    if (argc == 0 || !args[0].value || args[0].value[0] == '\0') {
        fprintf(stderr, "tidesh: umask: mask argument required\n");
        return 1;
    }

    char *endptr = NULL;
    long  val    = strtol(args[0].value, &endptr, 8);
    if (*endptr != '\0' || val < 0 || val > 0777) {
        fprintf(stderr, "tidesh: umask: %s: invalid octal number\n",
                args[0].value);
        return 1;
    }

    mode_t new_mask = (mode_t)val;
    mode_t old      = umask(new_mask);

    mode_t *saved = malloc(sizeof(mode_t));
    if (!saved)
        return 1;
    *saved = old;
    *state = saved;
    return 0;
}

int context_exit_umask(Session *session, int status, void *state) {
    (void)session;
    (void)status;
    mode_t *saved = (mode_t *)state;
    if (saved) {
        umask(*saved);
        free(saved);
    }
    return 0;
}
