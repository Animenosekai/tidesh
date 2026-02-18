#include <limits.h> /* PATH_MAX */
#include <pwd.h>    /* getpwuid, struct passwd */
#include <signal.h> /* signal, SIGINT, SIGQUIT, SIG_IGN */
#include <stdio.h>  /* printf */
#include <stdlib.h> /* free */
#include <string.h> /* strcmp, strdup */
#include <unistd.h> /* getuid */

#include "ast.h"        /* parse */
#include "data/array.h" /* array_pop, free_array */
#include "data/files.h" /* read_all */
#include "data/trie.h"
#include "environ.h" /* environ_get */
#include "execute.h" /* execute */
#include "expand.h"  /* full_expansion */
#include "hooks.h"   /* HOOK_NAME_* */
#include "lexer.h"   /* free_lexer_token, LexerInput, LexerToken, TOKEN_* */
#include "prompt.h"
#include "prompt/ansi.h" /* ansi_apply */
#include "session.h"     /* init_session, Session */

#define PS1 "❱ "
#define PS2 "╌ "

/* This checks if we are at the end of a terminal line
 * (and not in a string for example) */
bool should_return(char *input, Session *session) {
    char *new_input = strdup(input);

    // Check if the lexer returns an EOL right before the EOF
    LexerInput lexer_input = {0};
    init_lexer_input(&lexer_input, new_input, execute_string_stdout, session);

    LexerToken token;
    TokenType  last_type = TOKEN_EOF;
    do {
        token = lexer_next_token(&lexer_input);
        if (token.type == TOKEN_EOF) {
            break;
        }
        last_type = token.type;
        free_lexer_token(&token);
    } while (1);

    free(new_input);
    free_lexer_input(&lexer_input);

    return last_type == TOKEN_EOL;
}

static char *expand_prompt(const char *prompt, Session *session) {
    if (!prompt) {
        return NULL;
    }

    char *input = strdup(prompt);
    if (!input) {
        return NULL;
    }

    Array *expanded = full_expansion(input, session);
    free(input);
    if (!expanded) {
        return strdup(prompt);
    }

    char *result = NULL;
    if (expanded->count > 0) {
        result = array_pop(expanded, 0);
    } else {
        result = strdup("");
    }

    free_array(expanded);
    free(expanded);
    return result;
}

static void print_usage(const char *prog_name, bool colors) {
    const char *header_clr       = colors ? ANSI_BOLD ANSI_YELLOW : "";
    const char *option_clr       = colors ? ANSI_BOLD ANSI_GREEN : "";
    const char *placeholder_clr  = colors ? ANSI_YELLOW : "";
    const char *meta_clr         = colors ? ANSI_MAGENTA : "";
    const char *version_meta_clr = colors ? ANSI_BRIGHT_BLACK : "";
    const char *italic_clr       = colors ? ANSI_ITALIC : "";
    const char *name_clr         = colors ? ANSI_BOLD ANSI_CYAN : "";
    const char *version_clr      = colors ? ANSI_CYAN : "";
    const char *program_clr      = colors ? ANSI_ITALIC ANSI_CYAN : "";
    const char *reset            = colors ? ANSI_COLOR_RESET : "";

#ifdef PROJECT_NAME
    printf("%s%s%s", name_clr, PROJECT_NAME, reset);
#else
    printf("%s%s%s", name_clr, "tidesh", reset);
#endif

#ifdef RAW_VERSION
    printf(" version %s%s%s", version_clr, RAW_VERSION, reset);
#endif
#ifdef GIT_VERSION
    printf(" %s(%s)%s", version_meta_clr, GIT_VERSION, reset);
#endif
    printf("\n");

#ifdef BRIEF
    printf("%s%s%s\n", italic_clr, BRIEF, reset);
#endif
    printf("\n");

    printf("%sUsage:%s %s%s%s %s[options]%s %s[script_file | -]%s\n\n",
           header_clr, reset, program_clr, prog_name, reset, meta_clr, reset,
           meta_clr, reset);

    printf("%sOptions:%s\n", header_clr, reset);

    printf("  %s%-20s%s %s\n", option_clr, "--help", reset,
           "Show this help message");
    printf("  %s%s%s, %s%s%s %s%-9s%s %s\n", option_clr, "--eval", reset,
           option_clr, "-c", reset, placeholder_clr, "<cmd>", reset,
           "Execute the given command and exit");
    printf("  %s%-20s%s %s\n", option_clr, "--keep-alive", reset,
           "Stay interactive after executing a script or eval command");
    printf("  %s%s%s %s%-15s%s %s\n", option_clr, "--cd", reset,
           placeholder_clr, "<dir>", reset, "Change to directory on startup");
    printf("  %s%s%s %s%-15s%s %s%s%s%s\n", option_clr, "--rc", reset,
           placeholder_clr, "<file>", reset, "Use custom RC file ", meta_clr,
           "(default: ~/.tideshrc)", reset);
    printf("  %s%s%s %s%-10s%s %s%s%s%s\n", option_clr, "--history", reset,
           placeholder_clr, "<file>", reset, "Use custom history file ",
           meta_clr, "(default: ~/.tidesh-history)", reset);
    printf("  %s%-20s%s %s\n", option_clr, "--enable-colors", reset,
           "(Force) Enable terminal colors");
    printf("  %s%-20s%s %s\n", option_clr, "--disable-colors", reset,
           "Disable terminal colors");
    printf("  %s%-20s%s %s\n", option_clr, "--disable-history", reset,
           "Disable command history");
}

#ifndef TESTING
int main(int argc, char **argv) {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    bool  help             = false;
    bool  no_fin           = false;
    bool  keep_alive       = false;
    char *eval_command     = NULL;
    char *script_path      = NULL;
    char *custom_rc_path   = NULL;
    char *custom_hist_path = NULL;
    char *startup_cd       = NULL;
    bool  enable_colors    = false;
    bool  disable_colors   = false;
    bool  disable_history  = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            help = true;
        } else if (strcmp(argv[i], "--no-fin") == 0) {
            no_fin = true;
        } else if (strcmp(argv[i], "--keep-alive") == 0) {
            keep_alive = true;
        } else if ((strcmp(argv[i], "--eval") == 0 ||
                    strcmp(argv[i], "-c") == 0) &&
                   i + 1 < argc) {
            eval_command = argv[++i];
            continue;
        } else if (strcmp(argv[i], "--cd") == 0 && i + 1 < argc) {
            startup_cd = argv[++i];
            continue;
        } else if ((strcmp(argv[i], "--rc") == 0 ||
                    strcmp(argv[i], "--tideshrc") == 0) &&
                   i + 1 < argc) {
            custom_rc_path = argv[++i];
            continue;
        } else if (strcmp(argv[i], "--history") == 0 && i + 1 < argc) {
            custom_hist_path = argv[++i];
            continue;
        } else if (strcmp(argv[i], "--enable-colors") == 0) {
            enable_colors = true;
        } else if (strcmp(argv[i], "--disable-colors") == 0) {
            disable_colors = true;
        } else if (strcmp(argv[i], "--disable-history") == 0) {
            disable_history = true;
        } else if (script_path == NULL) {
            script_path = argv[i];
        }
    }

    if (help) {
        print_usage(argv[0], !disable_colors);
        return 0;
    }

    if (enable_colors && disable_colors) {
        fprintf(
            stderr,
            "Error: Cannot use both --enable-colors and --disable-colors\n");
        return 1;
    }

    // Get the user's home directory
    struct passwd *pw = getpwuid(getuid());
    Session       *session;

    // Set default history path
    char history_path[PATH_MAX] = {0};
    if (custom_hist_path) {
        strncpy(history_path, custom_hist_path, sizeof(history_path) - 1);
    } else if (pw) {
        char *home = pw->pw_dir;
        snprintf(history_path, sizeof(history_path), "%s/.tidesh-history",
                 home);
    }
    session = init_session(NULL, history_path);

    if (argc > 0)
        environ_set(session->environ, "0", argv[0]);

    if (!no_fin) {
        // Set alias 'fin' to 'exit'
#ifndef TIDESH_DISABLE_ALIASES
        trie_set(session->aliases, "fin", "exit");
#endif
    }

    if (disable_colors) {
        session->terminal->supports_colors = false;
    } else if (enable_colors) {
        session->terminal->supports_colors = true;
    }

#ifndef TIDESH_DISABLE_HISTORY
    if (disable_history) {
        session->history->disabled = true;
    }
#endif

    if (startup_cd) {
        if (chdir(startup_cd) == 0) {
            update_working_dir(session);
        } else {
            perror("tidesh: --cd");
        }
    }

    run_cwd_hook(session, HOOK_NAME_BEFORE_RC);

    // Read and execute .tideshrc if it exists in the home directory
    char rc_path[PATH_MAX] = {0};
    if (custom_rc_path) {
        strncpy(rc_path, custom_rc_path, sizeof(rc_path) - 1);
    } else if (pw) {
        snprintf(rc_path, sizeof(rc_path), "%s/.tideshrc", pw->pw_dir);
    }

    if (rc_path[0] != '\0') {
        FILE *rc_file = fopen(rc_path, "r");
        if (rc_file) {
            char *content = read_all(rc_file);
            if (content) {
                // Temporarily disable history for .tideshrc commands
#ifndef TIDESH_DISABLE_HISTORY
                bool was_disabled          = session->history->disabled;
                session->history->disabled = true;
                execute_string(content, session);
                session->history->disabled = was_disabled;
#else
                execute_string(content, session);
#endif

                free(content);
            }
            fclose(rc_file);
        } else if (custom_rc_path) {
            fprintf(stderr, "tidesh: could not open rc file: %s\n",
                    custom_rc_path);
        }
    }

    run_cwd_hook(session, HOOK_NAME_SESSION_START);

    // If an eval command is provided, execute it
    if (eval_command) {
        // Temporarily disable history for eval commands
#ifndef TIDESH_DISABLE_HISTORY
        bool was_disabled          = session->history->disabled;
        session->history->disabled = true;
        int exit_status            = execute_string(eval_command, session);
        session->history->disabled = was_disabled;
#else
        int exit_status = execute_string(eval_command, session);
#endif

        if (!keep_alive && !script_path) {
            free_session(session);
            free(session);
            return exit_status;
        }
    }

    // If a script path is provided, read and execute the script
    if (script_path) {
        FILE *f = NULL;
        if (strcmp(script_path, "-") == 0) {
            f = stdin;
        } else {
            f = fopen(script_path, "r");
        }

        if (f) {
            char *content = read_all(f);
            if (content) {
                // Temporarily disable history for script commands
#ifndef TIDESH_DISABLE_HISTORY
                bool was_disabled          = session->history->disabled;
                session->history->disabled = true;
                int exit_status            = execute_string(content, session);
                session->history->disabled = was_disabled;
#else
                int exit_status = execute_string(content, session);
#endif

                free(content);

                if (!keep_alive) {
                    run_cwd_hook(session, HOOK_NAME_SESSION_END);
                    run_cwd_hook(session, HOOK_NAME_SESSION_END);
                    free_session(session);
                    free(session);
                    return exit_status;
                }
            }
            if (f != stdin) {
                fclose(f);
            }
        } else {
#ifdef PROJECT_NAME
            fprintf(stderr, "%s: could not open file: %s\n", PROJECT_NAME,
                    script_path);
#else
            fprintf(stderr, "tidesh: could not open file: %s\n", script_path);
#endif
            free_session(session);
            free(session);
            return 1;
        }

        if (!keep_alive) {
            run_cwd_hook(session, HOOK_NAME_SESSION_END);
            free_session(session);
            free(session);
            return 0;
        }
    }

    // Interactive shell loop
    while (true) {
        const char *applied_prompt;    // Prompt to display
        bool  ps1_should_free = false; // Whether applied_prompt was allocated
        char *temp_ps1        = NULL;  // Temporary buffer if expanded

        // Determine PS1 prompt
        if (session->features.prompt_expansion) {
            char       *ps1_env     = environ_get(session->environ, "PS1");
            const char *raw_prompt  = ps1_env ? ps1_env : PS1;
            bool        ps1_default = ps1_env == NULL;

            char *expanded_prompt = expand_prompt(raw_prompt, session);
            if (!expanded_prompt) {
                expanded_prompt = strdup(raw_prompt);
            }

            applied_prompt = expanded_prompt;

            if (session->terminal->supports_colors && ps1_default) {
                const char *colored =
                    ansi_apply(expanded_prompt, "", ANSI_MAGENTA, NULL);
                if (colored) {
                    applied_prompt  = colored;
                    ps1_should_free = true;
                }
            }

            temp_ps1 = expanded_prompt;
        } else {
            // Feature disabled: check env var, fall back to constant
            char *ps1_env  = environ_get(session->environ, "PS1");
            applied_prompt = ps1_env ? ps1_env : PS1;
        }

        const char
            *applied_continuation_prompt; // Continuation prompt to display
        bool ps2_should_free =
            false; // Whether applied_continuation_prompt was allocated
        char *temp_ps2 = NULL; // Temporary buffer if expanded

        // Determine PS2 (continuation) prompt
        if (session->features.prompt_expansion) {
            char       *ps2_env = environ_get(session->environ, "PS2");
            const char *raw_continuation_prompt = ps2_env ? ps2_env : PS2;
            bool        ps2_default             = ps2_env == NULL;

            char *expanded_continuation_prompt =
                expand_prompt(raw_continuation_prompt, session);
            if (!expanded_continuation_prompt) {
                expanded_continuation_prompt = strdup(raw_continuation_prompt);
            }

            applied_continuation_prompt = expanded_continuation_prompt;

            if (session->terminal->supports_colors && ps2_default) {
                const char *colored = ansi_apply(expanded_continuation_prompt,
                                                 "", ANSI_WHITE, NULL);
                if (colored) {
                    applied_continuation_prompt = colored;
                    ps2_should_free             = true;
                }
            }

            temp_ps2 = expanded_continuation_prompt;
        } else {
            // Feature disabled: check env var, fall back to constant
            char *ps2_env               = environ_get(session->environ, "PS2");
            applied_continuation_prompt = ps2_env ? ps2_env : PS2;
        }

        char *input =
            prompt((char *)applied_prompt, (char *)applied_continuation_prompt,
                   session, should_return);

        // Cleanup allocated prompts
        if (ps1_should_free && applied_prompt != temp_ps1) {
            free((void *)applied_prompt);
        }
        if (temp_ps1) {
            free(temp_ps1);
        }
        if (ps2_should_free && applied_continuation_prompt != temp_ps2) {
            free((void *)applied_continuation_prompt);
        }
        if (temp_ps2) {
            free(temp_ps2);
        }

        if (input == NULL) {
            // Usually indicates Ctrl + C or Ctrl + D or EOF
            if (session->exit_requested) {
                break;
            }
            printf("\n");
            continue;
        }

        // Ignore empty input
        if (strcmp(input, "") == 0) {
            free(input);
            continue;
        }

        // Si l'utilisateur tape exit, on sort proprement de la boucle
        if (strcmp(input, "exit") == 0) {
            free(input);
            break;
        }

        execute_string(input, session);
        free(input);
    }
    run_cwd_hook(session, HOOK_NAME_SESSION_END);
    free_session(session);
    free(session);

    return 0;
}
#endif
