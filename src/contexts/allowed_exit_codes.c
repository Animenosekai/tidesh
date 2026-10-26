/** allowed_exit_codes.c
 *
 * Implementation of the 'allowed_exit_codes' built-in context manager.
 */

#include <ctype.h>  /* isdigit */
#include <stdbool.h>/* bool */
#include <stdlib.h> /* malloc, free, realloc, strtol */
#include <string.h> /* strcmp, strcasecmp */

#include "context.h"
#include "contexts/allowed_exit_codes.h"
#include "session.h"

int context_enter_allowed_exit_codes(Session *session, ContextArg *args,
                                     size_t argc, void **state) {
    if (!session)
        return 1;
    (void)state;

    bool   allow_all = false;
    int   *codes     = NULL;
    size_t count     = 0;

    // Always include 0 by default
    codes = malloc(sizeof(int));
    if (codes) {
        codes[0] = 0;
        count    = 1;
    }

    for (size_t i = 0; i < argc; i++) {
        const char *val = args[i].value;
        if (!val || val[0] == '\0')
            continue;

        if (strcmp(val, "*") == 0 || strcasecmp(val, "all") == 0) {
            allow_all = true;
            break;
        }

        const char *p = val;
        while (*p) {
            while (*p && (*p == ' ' || *p == '\t' || *p == ',' || *p == ';')) {
                p++;
            }
            if (!*p)
                break;

            if (*p == '*' || strncasecmp(p, "all", 3) == 0) {
                allow_all = true;
                break;
            }

            if (isdigit((unsigned char)*p) ||
                (*p == '-' && isdigit((unsigned char)*(p + 1)))) {
                char *endptr   = NULL;
                long  val_long = strtol(p, &endptr, 10);
                p              = endptr;

                int  code   = (int)val_long;
                bool exists = false;
                for (size_t j = 0; j < count; j++) {
                    if (codes[j] == code) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    int *new_codes = realloc(codes, (count + 1) * sizeof(int));
                    if (new_codes) {
                        codes        = new_codes;
                        codes[count] = code;
                        count++;
                    }
                }
            } else {
                p++;
            }
        }
        if (allow_all)
            break;
    }

    session_push_allowed_exit_codes(session, codes, count, allow_all);
    if (codes) {
        free(codes);
    }
    return 0;
}

int context_exit_allowed_exit_codes(Session *session, int status, void *state) {
    (void)status;
    (void)state;
    if (session) {
        session_pop_allowed_exit_codes(session);
    }
    return 0;
}
