/** ast.c
 *
 * Builtin command to parse shell commands and display the Abstract Syntax Tree.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "builtins/ast.h"
#include "data/dynamic.h"
#include "lexer.h"
#include "session.h"

typedef struct BoxChars {
    const char *branch;      // "├── " or "|-- "
    const char *last_branch; // "╰── " or "\\-- "
    const char *vertical;    // "│   " or "|   "
    const char *space;       // "    "
} BoxChars;

static const BoxChars UNICODE_BOX = {
    .branch      = "├── ",
    .last_branch = "╰── ",
    .vertical    = "│   ",
    .space       = "    ",
};

static const BoxChars ASCII_BOX = {
    .branch      = "|-- ",
    .last_branch = "\\-- ",
    .vertical    = "|   ",
    .space       = "    ",
};

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

static void print_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fputs("  ", out);
    }
}

static void ast_pretty_print_node(FILE *out, const ASTNode *node,
                                  const char *label, const BoxChars *box,
                                  bool is_last_stack[], int depth,
                                  bool is_last) {
    if (!node) {
        if (depth == 0) {
            fputs("(null)\n", out);
        } else {
            for (int i = 0; i < depth - 1; i++) {
                fputs(is_last_stack[i] ? box->space : box->vertical, out);
            }
            fputs(is_last ? box->last_branch : box->branch, out);
            if (label) {
                fprintf(out, "%s: ", label);
            }
            fputs("(null)\n", out);
        }
        return;
    }

    if (depth == 0) {
        fputs(node_type_to_string(node->type), out);
        if (node->background) {
            fputs(" [background]", out);
        }
        fputc('\n', out);
    } else {
        for (int i = 0; i < depth - 1; i++) {
            fputs(is_last_stack[i] ? box->space : box->vertical, out);
        }
        fputs(is_last ? box->last_branch : box->branch, out);
        if (label) {
            fprintf(out, "%s: ", label);
        }
        fputs(node_type_to_string(node->type), out);
        if (node->background) {
            fputs(" [background]", out);
        }
        fputc('\n', out);
    }

    if (depth > 0) {
        is_last_stack[depth - 1] = is_last;
    }

    if (node->type == NODE_COMMAND) {
        bool has_assignments =
            (node->assignments && node->assignments->count > 0);
        bool has_argv       = (node->argc > 0 && node->argv != NULL);
        bool has_redirects  = (node->redirects != NULL);
        int  total_children = (has_assignments ? 1 : 0) + (has_argv ? 1 : 0) +
                              (has_redirects ? 1 : 0);
        int  child_idx      = 0;

        if (has_assignments) {
            child_idx++;
            bool last = (child_idx == total_children);
            for (int i = 0; i < depth; i++) {
                fputs(is_last_stack[i] ? box->space : box->vertical, out);
            }
            fputs(last ? box->last_branch : box->branch, out);
            fputs("assignments: [", out);
            for (size_t i = 0; i < node->assignments->count; i++) {
                if (i > 0)
                    fputs(", ", out);
                fprintf(out, "\"%s\"", node->assignments->items[i]);
            }
            fputs("]\n", out);
        }

        if (has_argv) {
            child_idx++;
            bool last = (child_idx == total_children);
            for (int i = 0; i < depth; i++) {
                fputs(is_last_stack[i] ? box->space : box->vertical, out);
            }
            fputs(last ? box->last_branch : box->branch, out);
            fputs("argv: [", out);
            for (int i = 0; i < node->argc; i++) {
                if (i > 0)
                    fputs(", ", out);
                fprintf(out, "\"%s\"", node->argv[i] ? node->argv[i] : "");
            }
            fputs("]\n", out);
        }

        if (has_redirects) {
            child_idx++;
            bool last = (child_idx == total_children);
            for (int i = 0; i < depth; i++) {
                fputs(is_last_stack[i] ? box->space : box->vertical, out);
            }
            fputs(last ? box->last_branch : box->branch, out);
            fputs("redirects:\n", out);

            int r_count = 0;
            for (Redirection *r = node->redirects; r; r = r->next)
                r_count++;
            int r_idx            = 0;
            is_last_stack[depth] = last;
            for (Redirection *r = node->redirects; r; r = r->next) {
                r_idx++;
                bool r_last = (r_idx == r_count);
                for (int i = 0; i <= depth; i++) {
                    fputs(is_last_stack[i] ? box->space : box->vertical, out);
                }
                fputs(r_last ? box->last_branch : box->branch, out);
                fprintf(out, "fd %d (%s) -> \"%s\"\n", r->fd,
                        token_type_to_string(r->type),
                        r->target ? r->target : "");
            }
        }
        return;
    }

#ifndef TIDESH_DISABLE_FUNCTIONS
    if (node->type == NODE_FUNCTION_DEF) {
        for (int i = 0; i < depth; i++) {
            fputs(is_last_stack[i] ? box->space : box->vertical, out);
        }
        fputs(box->branch, out);
        fprintf(out, "name: \"%s\"\n",
                (node->argc > 0 && node->argv && node->argv[0]) ? node->argv[0]
                                                                : "");
        is_last_stack[depth] = false;
        ast_pretty_print_node(out, node->left, "body", box, is_last_stack,
                              depth + 1, true);
        return;
    }
#endif

#ifndef TIDESH_DISABLE_BLOCKS
    if (node->type == NODE_BLOCK) {
        ast_pretty_print_node(out, node->left, "body", box, is_last_stack,
                              depth + 1, true);
        return;
    }
#endif

#ifndef TIDESH_DISABLE_SUBSHELLS
    if (node->type == NODE_SUBSHELL) {
        ast_pretty_print_node(out, node->left, "body", box, is_last_stack,
                              depth + 1, true);
        return;
    }
#endif

#ifndef TIDESH_DISABLE_LOOPS
    if (node->type == NODE_FOR) {
        for (int i = 0; i < depth; i++) {
            fputs(is_last_stack[i] ? box->space : box->vertical, out);
        }
        fputs(box->branch, out);
        fprintf(out, "variable: \"%s\"\n",
                (node->argc > 0 && node->argv && node->argv[0]) ? node->argv[0]
                                                                : "");
        if (node->for_has_in) {
            for (int i = 0; i < depth; i++) {
                fputs(is_last_stack[i] ? box->space : box->vertical, out);
            }
            fputs(box->branch, out);
            fputs("words: [", out);
            for (int i = 1; i < node->argc; i++) {
                if (i > 1)
                    fputs(", ", out);
                fprintf(out, "\"%s\"", node->argv[i] ? node->argv[i] : "");
            }
            fputs("]\n", out);
        }
        is_last_stack[depth] = false;
        ast_pretty_print_node(out, node->left, "body", box, is_last_stack,
                              depth + 1, true);
        return;
    }
    if (node->type == NODE_WHILE) {
        is_last_stack[depth] = false;
        ast_pretty_print_node(out, node->left, "condition", box, is_last_stack,
                              depth + 1, false);
        is_last_stack[depth] = true;
        ast_pretty_print_node(out, node->right, "body", box, is_last_stack,
                              depth + 1, true);
        return;
    }
#endif

#ifndef TIDESH_DISABLE_CONDITIONALS
    if (node->type == NODE_CASE) {
        for (int i = 0; i < depth; i++) {
            fputs(is_last_stack[i] ? box->space : box->vertical, out);
        }
        fputs(box->branch, out);
        fprintf(out, "word: \"%s\"\n",
                (node->argc > 0 && node->argv && node->argv[0]) ? node->argv[0]
                                                                : "");
        int item_count = 0;
        for (CaseItem *item = node->case_items; item; item = item->next)
            item_count++;
        int idx = 0;
        for (CaseItem *item = node->case_items; item;
             item           = item->next, idx++) {
            bool is_last_item = (idx == item_count - 1);
            for (int i = 0; i < depth; i++) {
                fputs(is_last_stack[i] ? box->space : box->vertical, out);
            }
            fputs(is_last_item ? box->last_branch : box->branch, out);
            fputs("clause: patterns=[", out);
            for (int p = 0; p < item->pattern_count; p++) {
                if (p > 0)
                    fputs(", ", out);
                fprintf(out, "\"%s\"",
                        item->patterns[p] ? item->patterns[p] : "");
            }
            const char *term_str = ";;";
            if (item->terminator == CASE_TERM_FALLTHROUGH)
                term_str = ";&";
            else if (item->terminator == CASE_TERM_CONTINUE)
                term_str = ";;&";
            fprintf(out, "] terminator=\"%s\"\n", term_str);

            is_last_stack[depth] = is_last_item;
            if (item->body) {
                ast_pretty_print_node(out, item->body, "body", box,
                                      is_last_stack, depth + 1, true);
            }
        }
        return;
    }
    if (node->type == NODE_CONDITIONAL) {
        int b_count = 0;
        for (ConditionalBranch *b = node->branches; b; b = b->next)
            b_count++;
        int b_idx = 0;
        for (ConditionalBranch *b = node->branches; b; b = b->next, b_idx++) {
            bool b_last = (b_idx == b_count - 1);
            for (int i = 0; i < depth; i++) {
                fputs(is_last_stack[i] ? box->space : box->vertical, out);
            }
            fputs(b_last ? box->last_branch : box->branch, out);
            if (b->condition) {
                fprintf(out, "branch %d (if/elif)\n", b_idx);
                is_last_stack[depth] = b_last;
                ast_pretty_print_node(out, b->condition, "condition", box,
                                      is_last_stack, depth + 1, false);
                ast_pretty_print_node(out, b->body, "body", box, is_last_stack,
                                      depth + 1, true);
            } else {
                fprintf(out, "branch %d (else)\n", b_idx);
                is_last_stack[depth] = b_last;
                ast_pretty_print_node(out, b->body, "body", box, is_last_stack,
                                      depth + 1, true);
            }
        }
        return;
    }
#endif

    if (node->left && node->right) {
        ast_pretty_print_node(out, node->left, "left", box, is_last_stack,
                              depth + 1, false);
        ast_pretty_print_node(out, node->right, "right", box, is_last_stack,
                              depth + 1, true);
    } else if (node->left) {
        ast_pretty_print_node(out, node->left, "left", box, is_last_stack,
                              depth + 1, true);
    } else if (node->right) {
        ast_pretty_print_node(out, node->right, "right", box, is_last_stack,
                              depth + 1, true);
    }
}

static void ast_pretty_print(FILE *out, const ASTNode *node, bool ascii) {
    if (!node) {
        fputs("(empty AST)\n", out);
        return;
    }
    const BoxChars *box               = ascii ? &ASCII_BOX : &UNICODE_BOX;
    bool            is_last_stack[64] = {false};
    ast_pretty_print_node(out, node, NULL, box, is_last_stack, 0, true);
}

static void ast_json_print(FILE *out, const ASTNode *node, int indent) {
    if (!node) {
        fputs("null", out);
        return;
    }

    fputs("{\n", out);
    print_indent(out, indent + 1);
    fprintf(out, "\"type\": \"%s\",\n", node_type_to_string(node->type));
    print_indent(out, indent + 1);
    fprintf(out, "\"background\": %s", node->background ? "true" : "false");

    if (node->type == NODE_COMMAND) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"argv\": [", out);
        for (int i = 0; i < node->argc; i++) {
            if (i > 0)
                fputs(", ", out);
            print_json_escaped(out, node->argv[i]);
        }
        fputs("],\n", out);

        print_indent(out, indent + 1);
        fprintf(out, "\"argc\": %d,\n", node->argc);

        print_indent(out, indent + 1);
        fputs("\"assignments\": [", out);
        if (node->assignments) {
            for (size_t i = 0; i < node->assignments->count; i++) {
                if (i > 0)
                    fputs(", ", out);
                print_json_escaped(out, node->assignments->items[i]);
            }
        }
        fputs("],\n", out);

        print_indent(out, indent + 1);
        fputs("\"redirects\": [", out);
        if (node->redirects) {
            fputc('\n', out);
            for (Redirection *r = node->redirects; r; r = r->next) {
                print_indent(out, indent + 2);
                fputs("{\n", out);
                print_indent(out, indent + 3);
                fprintf(out, "\"fd\": %d,\n", r->fd);
                print_indent(out, indent + 3);
                fprintf(out, "\"type\": \"%s\",\n",
                        token_type_to_string(r->type));
                print_indent(out, indent + 3);
                fputs("\"target\": ", out);
                print_json_escaped(out, r->target);
                fputs(",\n", out);
                print_indent(out, indent + 3);
                fprintf(out, "\"is_process_substitution\": %s\n",
                        r->is_process_substitution ? "true" : "false");
                print_indent(out, indent + 2);
                fputs("}", out);
                if (r->next)
                    fputs(",", out);
                fputc('\n', out);
            }
            print_indent(out, indent + 1);
        }
        fputs("]", out);
    }

#ifndef TIDESH_DISABLE_FUNCTIONS
    if (node->type == NODE_FUNCTION_DEF) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"name\": ", out);
        print_json_escaped(out, (node->argc > 0 && node->argv && node->argv[0])
                                    ? node->argv[0]
                                    : "");
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"body\": ", out);
        ast_json_print(out, node->left, indent + 1);
    }
#endif

#ifndef TIDESH_DISABLE_BLOCKS
    if (node->type == NODE_BLOCK) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"body\": ", out);
        ast_json_print(out, node->left, indent + 1);
    }
#endif

#ifndef TIDESH_DISABLE_SUBSHELLS
    if (node->type == NODE_SUBSHELL) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"body\": ", out);
        ast_json_print(out, node->left, indent + 1);
    }
#endif

#ifndef TIDESH_DISABLE_CONDITIONALS
    if (node->type == NODE_CASE) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"word\": ", out);
        print_json_escaped(out, (node->argc > 0 && node->argv && node->argv[0])
                                    ? node->argv[0]
                                    : "");
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"items\": [\n", out);
        for (CaseItem *item = node->case_items; item; item = item->next) {
            print_indent(out, indent + 2);
            fputs("{\n", out);
            print_indent(out, indent + 3);
            fputs("\"patterns\": [", out);
            for (int p = 0; p < item->pattern_count; p++) {
                if (p > 0)
                    fputs(", ", out);
                print_json_escaped(out,
                                   item->patterns[p] ? item->patterns[p] : "");
            }
            fputs("],\n", out);
            print_indent(out, indent + 3);
            const char *term_str = ";;";
            if (item->terminator == CASE_TERM_FALLTHROUGH)
                term_str = ";&";
            else if (item->terminator == CASE_TERM_CONTINUE)
                term_str = ";;&";
            fprintf(out, "\"terminator\": \"%s\",\n", term_str);
            print_indent(out, indent + 3);
            fputs("\"body\": ", out);
            ast_json_print(out, item->body, indent + 3);
            fputc('\n', out);
            print_indent(out, indent + 2);
            fputs("}", out);
            if (item->next)
                fputs(",", out);
            fputc('\n', out);
        }
        print_indent(out, indent + 1);
        fputs("]", out);
    }
    if (node->type == NODE_CONDITIONAL) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"branches\": [\n", out);
        for (ConditionalBranch *b = node->branches; b; b = b->next) {
            print_indent(out, indent + 2);
            fputs("{\n", out);
            print_indent(out, indent + 3);
            fputs("\"condition\": ", out);
            ast_json_print(out, b->condition, indent + 3);
            fputs(",\n", out);
            print_indent(out, indent + 3);
            fputs("\"body\": ", out);
            ast_json_print(out, b->body, indent + 3);
            fputc('\n', out);
            print_indent(out, indent + 2);
            fputs("}", out);
            if (b->next)
                fputs(",", out);
            fputc('\n', out);
        }
        print_indent(out, indent + 1);
        fputs("]", out);
    }
#endif

#ifndef TIDESH_DISABLE_LOOPS
    if (node->type == NODE_FOR) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"variable\": ", out);
        print_json_escaped(out, (node->argc > 0 && node->argv && node->argv[0])
                                    ? node->argv[0]
                                    : "");
        if (node->for_has_in) {
            fputs(",\n", out);
            print_indent(out, indent + 1);
            fputs("\"words\": [", out);
            for (int i = 1; i < node->argc; i++) {
                if (i > 1)
                    fputs(", ", out);
                print_json_escaped(out, node->argv[i] ? node->argv[i] : "");
            }
            fputs("]", out);
        }
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"body\": ", out);
        ast_json_print(out, node->left, indent + 1);
    }
    if (node->type == NODE_WHILE) {
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"condition\": ", out);
        ast_json_print(out, node->left, indent + 1);
        fputs(",\n", out);
        print_indent(out, indent + 1);
        fputs("\"body\": ", out);
        ast_json_print(out, node->right, indent + 1);
    }
#endif

    if (node->type != NODE_COMMAND &&
#ifndef TIDESH_DISABLE_FUNCTIONS
        node->type != NODE_FUNCTION_DEF &&
#endif
#ifndef TIDESH_DISABLE_BLOCKS
        node->type != NODE_BLOCK &&
#endif
#ifndef TIDESH_DISABLE_SUBSHELLS
        node->type != NODE_SUBSHELL &&
#endif
#ifndef TIDESH_DISABLE_CONDITIONALS
        node->type != NODE_CONDITIONAL && node->type != NODE_CASE &&
#endif
#ifndef TIDESH_DISABLE_LOOPS
        node->type != NODE_FOR && node->type != NODE_WHILE &&
#endif
        (node->left || node->right)) {
        if (node->left) {
            fputs(",\n", out);
            print_indent(out, indent + 1);
            fputs("\"left\": ", out);
            ast_json_print(out, node->left, indent + 1);
        }
        if (node->right) {
            fputs(",\n", out);
            print_indent(out, indent + 1);
            fputs("\"right\": ", out);
            ast_json_print(out, node->right, indent + 1);
        }
    }

    fputc('\n', out);
    print_indent(out, indent);
    fputs("}", out);
}

static char *read_all_stdin(void) {
    Dynamic d = {0};
    init_dynamic(&d);
    char   buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf) - 1, stdin)) > 0) {
        buf[n] = '\0';
        dynamic_extend(&d, buf);
    }
    char *result = dynamic_to_string(&d);
    free_dynamic(&d);
    return result;
}

static void print_help(void) {
    printf(
        "Usage: ast [OPTIONS] [COMMAND...]\n\n"
        "Parse a shell command string into an Abstract Syntax Tree (AST).\n\n"
        "Options:\n"
        "  -j, --json     Output AST in JSON format\n"
        "  -p, --pretty   Output AST in pretty-printed tree format (default)\n"
        "  -a, --ascii    Use ASCII characters instead of rounded Unicode box "
        "drawings\n"
        "  -h, --help     Display this help message\n\n"
        "If no COMMAND is provided, input is read from standard input.\n");
}

int builtin_ast(int argc, char **argv, Session *session) {
    bool json_mode  = false;
    bool ascii_mode = false;
    int  arg_start  = argc;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "-p") == 0 ||
                   strcmp(argv[i], "--pretty") == 0) {
            json_mode = false;
        } else if (strcmp(argv[i], "-a") == 0 ||
                   strcmp(argv[i], "--ascii") == 0) {
            ascii_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "ast: unrecognized option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'ast --help' for more information.\n");
            return 1;
        } else {
            arg_start = i;
            break;
        }
    }

    char *command_str      = NULL;
    bool  free_command_str = false;

    if (arg_start >= argc) {
        command_str      = read_all_stdin();
        free_command_str = true;
    } else {
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

    LexerInput *lexer = init_lexer_input(NULL, command_str, NULL, session);
    if (!lexer) {
        if (free_command_str && command_str[0] != '\0') {
            free(command_str);
        }
        fprintf(stderr, "ast: failed to initialize lexer\n");
        return 1;
    }

    ASTNode *root = parse(lexer, session);

    if (json_mode) {
        ast_json_print(stdout, root, 0);
        printf("\n");
    } else {
        ast_pretty_print(stdout, root, ascii_mode);
    }

    if (root) {
        free_ast(root);
        free(root);
    }
    free_lexer_input(lexer);
    free(lexer);
    if (free_command_str && command_str[0] != '\0') {
        free(command_str);
    }
    return 0;
}
