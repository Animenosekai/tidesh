/** lexer.c
 *
 * Builtin command to run lexical analysis and display tokens.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins/lexer.h"
#include "data/dynamic.h"
#include "lexer.h"
#include "session.h"

static void print_json_escaped(FILE *out, const char *s) {
    if (!s) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':
                fputs("\\\"", out);
                break;
            case '\\':
                fputs("\\\\", out);
                break;
            case '\b':
                fputs("\\b", out);
                break;
            case '\f':
                fputs("\\f", out);
                break;
            case '\n':
                fputs("\\n", out);
                break;
            case '\r':
                fputs("\\r", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            default:
                if ((unsigned char)*p < 0x20) {
                    fprintf(out, "\\u%04x", (unsigned char)*p);
                } else {
                    fputc(*p, out);
                }
                break;
        }
    }
    fputc('"', out);
}

static char *read_all_stdin(void) {
    Dynamic d = {0};
    init_dynamic(&d);
    char    buf[1024];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        dynamic_extend(&d, buf);
    }
    char *result = dynamic_to_string(&d);
    free_dynamic(&d);
    return result;
}

static void print_help(void) {
    printf("Usage: lexer [OPTIONS] [COMMAND...]\n\n"
           "Tokenize a shell command string and display its tokens.\n\n"
           "Options:\n"
           "  -j, --json     Output tokens in JSON format\n"
           "  -p, --pretty   Output tokens in pretty human-readable format "
           "(default)\n"
           "  -h, --help     Display this help message\n\n"
           "If no COMMAND is provided, input is read from standard input.\n");
}

int builtin_lexer(int argc, char **argv, Session *session) {
    bool json_mode = false;
    int  arg_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) {
            json_mode = true;
            arg_start = i + 1;
        } else if (strcmp(argv[i], "-p") == 0 ||
                   strcmp(argv[i], "--pretty") == 0) {
            json_mode = false;
            arg_start = i + 1;
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "lexer: unrecognized option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'lexer --help' for more information.\n");
            return 1;
        } else {
            arg_start = i;
            break;
        }
    }

    char *command_str      = NULL;
    bool  free_command_str = false;

    if (arg_start >= argc) {
        // Read from stdin
        command_str      = read_all_stdin();
        free_command_str = true;
    } else {
        // Concatenate remaining arguments
        Dynamic d = {0};
        init_dynamic(&d);
        for (int i = arg_start; i < argc; i++) {
            if (i > arg_start) {
                dynamic_append(&d, ' ');
            }
            dynamic_extend(&d, argv[i]);
        }
        command_str = dynamic_to_string(&d);
        free_dynamic(&d);
        free_command_str = true;
    }

    if (!command_str) {
        command_str = "";
    }

    LexerInput *input = init_lexer_input(NULL, command_str, NULL, session);
    if (!input) {
        if (free_command_str && command_str[0] != '\0') {
            free(command_str);
        }
        fprintf(stderr, "lexer: failed to initialize lexer\n");
        return 1;
    }

    if (json_mode) {
        printf("[\n");
        size_t count = 0;
        while (true) {
            LexerToken token = lexer_next_token(input);
            TokenType  type  = token.type;

            if (count > 0) {
                printf(",\n");
            }
            printf("  {\n");
            printf("    \"type\": \"%s\",\n", token_type_to_string(type));
            printf("    \"value\": ");
            print_json_escaped(stdout, token.value);
            printf(",\n");
            printf("    \"extra\": ");
            print_json_escaped(stdout, token.extra);
            printf("\n");
            printf("  }");

            free_lexer_token(&token);
            count++;

            if (type == TOKEN_EOF) {
                break;
            }
        }
        printf("\n]\n");
    } else {
        printf("Token Stream:\n");
        size_t index = 0;
        while (true) {
            LexerToken token = lexer_next_token(input);
            TokenType  type  = token.type;

            printf("  [%03zu] %-24s", index, token_type_to_string(type));
            if (token.value) {
                printf(" value=\"%s\"", token.value);
            }
            if (token.extra) {
                printf(" extra=\"%s\"", token.extra);
            }
            printf("\n");

            free_lexer_token(&token);
            index++;

            if (type == TOKEN_EOF) {
                break;
            }
        }
    }

    free_lexer_input(input);
    free(input);
    if (free_command_str && command_str[0] != '\0') {
        free(command_str);
    }
    return 0;
}
