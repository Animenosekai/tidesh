/** read.c
 *
 * Implementation of the read builtin command.
 */

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "builtins/read.h"
#include "builtins/trap.h"
#include "data/dynamic.h"
#include "environ.h"
#include "session.h"

static bool is_valid_identifier(const char *name) {
    if (!name || !*name) {
        return false;
    }
    if (!isalpha((unsigned char)*name) && *name != '_') {
        return false;
    }
    for (const char *p = name + 1; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') {
            return false;
        }
    }
    return true;
}

static bool is_ifs_char(char c, const char *ifs) {
    return strchr(ifs, c) != NULL;
}

static bool is_ifs_whitespace(char c, const char *ifs) {
    return (c == ' ' || c == '\t' || c == '\n') && is_ifs_char(c, ifs);
}

static void assign_fields(Session *session, const char *input, int var_count,
                          char **var_names) {
    if (var_count == 0) {
        // No variable specified: assign whole input to REPLY without IFS
        // splitting
        environ_set(session->environ, "REPLY", input ? (char *)input : "");
        return;
    }

    const char *ifs = environ_get(session->environ, "IFS");
    if (!ifs) {
        ifs = " \t\n";
    }

    // If IFS is empty, no field splitting and no whitespace trimming
    if (ifs[0] == '\0') {
        environ_set(session->environ, var_names[0], input ? (char *)input : "");
        for (int i = 1; i < var_count; i++) {
            environ_set(session->environ, var_names[i], "");
        }
        return;
    }

    const char *p = input ? input : "";

    // 1. Strip leading IFS whitespace
    while (*p && is_ifs_whitespace(*p, ifs)) {
        p++;
    }

    for (int i = 0; i < var_count; i++) {
        if (i == var_count - 1) {
            // Last variable gets remainder with trailing IFS whitespace
            // stripped
            if (*p == '\0') {
                environ_set(session->environ, var_names[i], "");
            } else {
                size_t      len = strlen(p);
                const char *end = p + len;
                while (end > p && is_ifs_whitespace(*(end - 1), ifs)) {
                    end--;
                }
                char *val = strndup(p, end - p);
                environ_set(session->environ, var_names[i], val ? val : "");
                free(val);
            }
            break;
        }

        if (*p == '\0') {
            environ_set(session->environ, var_names[i], "");
            continue;
        }

        // Collect characters for field i until an IFS character
        Dynamic field = {0};
        init_dynamic(&field);
        while (*p && !is_ifs_char(*p, ifs)) {
            dynamic_append(&field, *p);
            p++;
        }
        char *field_str = dynamic_to_string(&field);
        environ_set(session->environ, var_names[i], field_str ? field_str : "");
        if (field_str) {
            free(field_str);
        }
        free_dynamic(&field);

        // Consume delimiter between fields
        if (*p) {
            if (is_ifs_whitespace(*p, ifs)) {
                // Consume all IFS whitespace
                while (*p && is_ifs_whitespace(*p, ifs)) {
                    p++;
                }
                // If followed by an IFS non-whitespace character, consume it
                // and following IFS whitespace
                if (*p && !is_ifs_whitespace(*p, ifs) && is_ifs_char(*p, ifs)) {
                    p++;
                    while (*p && is_ifs_whitespace(*p, ifs)) {
                        p++;
                    }
                }
            } else {
                // IFS non-whitespace delimiter
                p++;
                // Consume any following IFS whitespace
                while (*p && is_ifs_whitespace(*p, ifs)) {
                    p++;
                }
            }
        }
    }
}

int builtin_read(int argc, char **argv, Session *session) {
    if (!session || !session->environ) {
        return 1;
    }

    bool        raw_mode    = false;
    bool        silent      = false;
    const char *prompt      = NULL;
    int         input_fd    = STDIN_FILENO;
    char        delim       = '\n';
    bool        has_nchars  = false;
    int         max_chars   = 0;
    bool        has_timeout = false;
    double      timeout_sec = 0.0;

    int i = 1;
    while (i < argc) {
        char *arg = argv[i];
        if (arg[0] != '-' || arg[1] == '\0') {
            break; // Not an option
        }
        if (strcmp(arg, "--") == 0) {
            i++;
            break; // End of options
        }

        bool stop = false;
        for (int c = 1; arg[c] != '\0'; c++) {
            switch (arg[c]) {
                case 'r':
                    raw_mode = true;
                    break;
                case 's':
                    silent = true;
                    break;
                case 'p':
                    if (arg[c + 1] != '\0') {
                        prompt = &arg[c + 1];
                    } else if (i + 1 < argc) {
                        prompt = argv[++i];
                    } else {
                        fprintf(stderr,
                                "read: option requires an argument -- p\n");
                        return 1;
                    }
                    stop = true;
                    break;
                case 'u': {
                    char *val = NULL;
                    if (arg[c + 1] != '\0') {
                        val = &arg[c + 1];
                    } else if (i + 1 < argc) {
                        val = argv[++i];
                    } else {
                        fprintf(stderr,
                                "read: option requires an argument -- u\n");
                        return 1;
                    }
                    input_fd = atoi(val);
                    if (input_fd < 0) {
                        fprintf(stderr, "read: invalid file descriptor `%s'\n",
                                val);
                        return 1;
                    }
                    stop = true;
                } break;
                case 'd':
                    if (arg[c + 1] != '\0') {
                        delim = arg[c + 1];
                    } else if (i + 1 < argc) {
                        char *val = argv[++i];
                        delim     = val[0]; // If empty string, delim is '\0'
                    } else {
                        fprintf(stderr,
                                "read: option requires an argument -- d\n");
                        return 1;
                    }
                    stop = true;
                    break;
                case 'n': {
                    char *val = NULL;
                    if (arg[c + 1] != '\0') {
                        val = &arg[c + 1];
                    } else if (i + 1 < argc) {
                        val = argv[++i];
                    } else {
                        fprintf(stderr,
                                "read: option requires an argument -- n\n");
                        return 1;
                    }
                    has_nchars = true;
                    max_chars  = atoi(val);
                    stop       = true;
                } break;
                case 't': {
                    char *val = NULL;
                    if (arg[c + 1] != '\0') {
                        val = &arg[c + 1];
                    } else if (i + 1 < argc) {
                        val = argv[++i];
                    } else {
                        fprintf(stderr,
                                "read: option requires an argument -- t\n");
                        return 1;
                    }
                    has_timeout = true;
                    timeout_sec = strtod(val, NULL);
                    stop        = true;
                } break;
                default:
                    fprintf(stderr, "read: -%c: invalid option\n", arg[c]);
                    fprintf(stderr,
                            "read: usage: read [-ers] [-u fd] [-t timeout] [-p "
                            "prompt] [-n nchars] [-d delim] [name ...]\n");
                    return 1;
            }
            if (stop) {
                break;
            }
        }
        i++;
    }

    int    var_count = argc - i;
    char **var_names = &argv[i];
    for (int j = 0; j < var_count; j++) {
        if (!is_valid_identifier(var_names[j])) {
            fprintf(stderr, "read: `%s': not a valid identifier\n",
                    var_names[j]);
            return 1;
        }
    }

    if (prompt) {
        fputs(prompt, stderr);
        fflush(stderr);
    }

    struct termios orig_termios, raw_termios;
    bool           termios_saved = false;
    if (silent && isatty(input_fd)) {
        if (tcgetattr(input_fd, &orig_termios) == 0) {
            raw_termios = orig_termios;
            raw_termios.c_lflag &= ~ECHO;
            tcsetattr(input_fd, TCSANOW, &raw_termios);
            termios_saved = true;
        }
    }

    Dynamic line = {0};
    init_dynamic(&line);

    bool   eof_reached = false;
    bool   read_err    = false;
    bool   timed_out   = false;
    bool   escaped     = false;
    size_t chars_read  = 0;

    struct timeval start_time;
    if (has_timeout) {
        gettimeofday(&start_time, NULL);
    }

    while (true) {
        if (has_nchars && chars_read >= (size_t)max_chars) {
            break;
        }

        if (has_timeout) {
            struct timeval now;
            gettimeofday(&now, NULL);
            double elapsed = (now.tv_sec - start_time.tv_sec) +
                             (now.tv_usec - start_time.tv_usec) / 1000000.0;
            double rem     = timeout_sec - elapsed;
            if (rem <= 0) {
                timed_out = true;
                break;
            }
            int timeout_ms = (int)(rem * 1000.0);
            if (timeout_ms < 0) {
                timeout_ms = 0;
            }
            struct pollfd pfd = {.fd = input_fd, .events = POLLIN};
            int           ret = poll(&pfd, 1, timeout_ms);
            if (ret <= 0) {
                if (ret == 0) {
                    timed_out = true;
                } else if (errno == EINTR) {
                    check_pending_traps(session);
                    if (session->exit_requested || session->return_requested) {
                        break;
                    }
                    continue;
                } else {
                    read_err = true;
                }
                break;
            }
        }

        char    ch = 0;
        ssize_t n  = read(input_fd, &ch, 1);
        if (n < 0) {
            if (errno == EINTR) {
                check_pending_traps(session);
                if (session->exit_requested || session->return_requested) {
                    break;
                }
                continue;
            }
            read_err = true;
            break;
        }
        if (n == 0) {
            eof_reached = true;
            break;
        }

        chars_read++;

        if (!raw_mode) {
            if (escaped) {
                escaped = false;
                if (ch == '\n') {
                    // \<newline> line continuation
                    continue;
                }
                dynamic_append(&line, ch);
                continue;
            }

            if (ch == '\\') {
                escaped = true;
                continue;
            }
        }

        if (ch == delim) {
            break;
        }

        dynamic_append(&line, ch);
    }

    if (termios_saved) {
        tcsetattr(input_fd, TCSANOW, &orig_termios);
    }

    if (escaped) {
        dynamic_append(&line, '\\');
    }

    char *full_line = dynamic_to_string(&line);
    assign_fields(session, full_line ? full_line : "", var_count, var_names);
    if (full_line) {
        free(full_line);
    }
    free_dynamic(&line);

    if (read_err || timed_out) {
        return 1;
    }
    if (eof_reached) {
        return 1;
    }
    return 0;
}
