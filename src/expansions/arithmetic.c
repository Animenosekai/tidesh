/** arithmetic.c
 * Implementation of integer arithmetic $(( expression )) for tidesh.
 */

#include <ctype.h>   /* isspace, isdigit, isalpha, isalnum */
#include <errno.h>   /* errno, ERANGE */
#include <stdbool.h> /* bool, true, false */
#include <stdint.h>  /* int64_t */
#include <stdio.h>   /* fprintf, snprintf, stderr */
#include <stdlib.h>  /* strtoll, free, malloc, calloc */
#include <string.h>  /* strlen, strdup, strchr, strstr, strcmp, strncmp */

#include "data/array.h"   /* Array, free_array */
#include "data/dynamic.h" /* Dynamic */
#include "environ.h"      /* environ_get, environ_set */
#include "execute.h"
#include "expand.h"                /* expand_command_substitution */
#include "expansions/arithmetic.h" /* evaluate_arithmetic_expression, expand_arithmetic */
#include "expansions/variables.h" /* variable_expansion */
#include "session.h"              /* Session */

/* Token types for arithmetic expression parser */
typedef enum {
    TOK_EOF,
    TOK_INT,
    TOK_IDENT,
    TOK_PLUS,          // +
    TOK_MINUS,         // -
    TOK_STAR,          // *
    TOK_SLASH,         // /
    TOK_PERCENT,       // %
    TOK_POWER,         // **
    TOK_LSHIFT,        // <<
    TOK_RSHIFT,        // >>
    TOK_LT,            // <
    TOK_LE,            // <=
    TOK_GT,            // >
    TOK_GE,            // >=
    TOK_EQ,            // ==
    TOK_NE,            // !=
    TOK_BIT_AND,       // &
    TOK_BIT_XOR,       // ^
    TOK_BIT_OR,        // |
    TOK_BIT_NOT,       // ~
    TOK_LOG_AND,       // &&
    TOK_LOG_OR,        // ||
    TOK_LOG_NOT,       // !
    TOK_QUESTION,      // ?
    TOK_COLON,         // :
    TOK_ASSIGN,        // =
    TOK_ADD_ASSIGN,    // +=
    TOK_SUB_ASSIGN,    // -=
    TOK_MUL_ASSIGN,    // *=
    TOK_DIV_ASSIGN,    // /=
    TOK_MOD_ASSIGN,    // %=
    TOK_LSHIFT_ASSIGN, // <<=
    TOK_RSHIFT_ASSIGN, // >>=
    TOK_AND_ASSIGN,    // &=
    TOK_XOR_ASSIGN,    // ^=
    TOK_OR_ASSIGN,     // |=
    TOK_INC,           // ++
    TOK_DEC,           // --
    TOK_COMMA,         // ,
    TOK_LPAREN,        // (
    TOK_RPAREN,        // )
    TOK_ERROR
} ArithTokenType;

typedef struct {
    ArithTokenType type;
    int64_t        val;
    char          *name;
} ArithToken;

typedef struct {
    const char *src;
    size_t      pos;
    size_t      len;
} ArithLexer;

typedef struct {
    ArithLexer lexer;
    ArithToken cur;
    ArithToken peek;
    Session   *session;
    bool       had_error;
    int        recursion_depth;
} ArithParser;

static void free_token(ArithToken *tok) {
    if (tok->name) {
        free(tok->name);
        tok->name = NULL;
    }
}

static char lexer_peek(ArithLexer *l) {
    if (l->pos >= l->len) {
        return '\0';
    }
    return l->src[l->pos];
}

static char lexer_peek_next(ArithLexer *l) {
    if (l->pos + 1 >= l->len) {
        return '\0';
    }
    return l->src[l->pos + 1];
}

static char lexer_advance(ArithLexer *l) {
    if (l->pos >= l->len) {
        return '\0';
    }
    return l->src[l->pos++];
}

static void lexer_skip_whitespace(ArithLexer *l) {
    while (l->pos < l->len && isspace((unsigned char)l->src[l->pos])) {
        l->pos++;
    }
}

/* Helper to convert character to digit in base 2..64 */
static int char_to_digit(char c, int base) {
    int val = -1;
    if (c >= '0' && c <= '9') {
        val = c - '0';
    } else if (base <= 36) {
        if (c >= 'a' && c <= 'z') {
            val = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z') {
            val = c - 'A' + 10;
        }
    } else {
        // Base > 36: lowercase 10..35, uppercase 36..61, @ is 62, _ is 63
        if (c >= 'a' && c <= 'z') {
            val = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z') {
            val = c - 'A' + 36;
        } else if (c == '@') {
            val = 62;
        } else if (c == '_') {
            val = 63;
        }
    }

    if (val >= 0 && val < base) {
        return val;
    }
    return -1;
}

static ArithToken lexer_read_number(ArithLexer *l) {
    ArithToken tok = {0};
    tok.type       = TOK_INT;
    size_t start   = l->pos;

    // Scan until non-alphanumeric and not '#' or '@' or '_'
    while (l->pos < l->len &&
           (isalnum((unsigned char)l->src[l->pos]) || l->src[l->pos] == '#' ||
            l->src[l->pos] == '@' || l->src[l->pos] == '_')) {
        l->pos++;
    }

    size_t token_len = l->pos - start;
    char  *buf       = malloc(token_len + 1);
    memcpy(buf, l->src + start, token_len);
    buf[token_len] = '\0';

    // Check for base#number format (e.g. 2#1010, 16#FF)
    char *hash = strchr(buf, '#');
    if (hash) {
        *hash          = '\0';
        char *end_base = NULL;
        long  base     = strtol(buf, &end_base, 10);
        char *num_str  = hash + 1;
        if (end_base == hash && base >= 2 && base <= 64 && *num_str) {
            int64_t val = 0;
            for (size_t i = 0; num_str[i]; i++) {
                int d = char_to_digit(num_str[i], (int)base);
                if (d < 0) {
                    tok.type = TOK_ERROR;
                    free(buf);
                    return tok;
                }
                val = val * (int64_t)base + (int64_t)d;
            }
            tok.val = val;
            free(buf);
            return tok;
        }
        tok.type = TOK_ERROR;
        free(buf);
        return tok;
    }

    // Hexadecimal 0x... or 0X...
    if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X') && buf[2]) {
        char   *end = NULL;
        int64_t val = (int64_t)strtoll(buf, &end, 16);
        if (end && *end == '\0') {
            tok.val = val;
            free(buf);
            return tok;
        }
        tok.type = TOK_ERROR;
        free(buf);
        return tok;
    }

    // Octal 0...
    if (buf[0] == '0' && token_len > 1) {
        bool all_oct = true;
        for (size_t i = 0; i < token_len; i++) {
            if (buf[i] < '0' || buf[i] > '7') {
                all_oct = false;
                break;
            }
        }
        if (all_oct) {
            char   *end = NULL;
            int64_t val = (int64_t)strtoll(buf, &end, 8);
            tok.val     = val;
            free(buf);
            return tok;
        }
        // Invalid octal constant in shell is an error
        tok.type = TOK_ERROR;
        free(buf);
        return tok;
    }

    // Decimal
    char   *end = NULL;
    int64_t val = (int64_t)strtoll(buf, &end, 10);
    if (end && *end == '\0') {
        tok.val = val;
        free(buf);
        return tok;
    }

    tok.type = TOK_ERROR;
    free(buf);
    return tok;
}

static ArithToken arith_lexer_next_token(ArithLexer *l) {
    lexer_skip_whitespace(l);
    ArithToken tok = {0};

    if (l->pos >= l->len) {
        tok.type = TOK_EOF;
        return tok;
    }

    char c = lexer_peek(l);

    // Number literal
    if (isdigit((unsigned char)c)) {
        return lexer_read_number(l);
    }

    // Variable identifier
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = l->pos;
        while (l->pos < l->len && (isalnum((unsigned char)l->src[l->pos]) ||
                                   l->src[l->pos] == '_')) {
            l->pos++;
        }
        size_t len = l->pos - start;
        tok.type   = TOK_IDENT;
        tok.name   = malloc(len + 1);
        memcpy(tok.name, l->src + start, len);
        tok.name[len] = '\0';
        return tok;
    }

    lexer_advance(l);
    switch (c) {
        case '+':
            if (lexer_peek(l) == '+') {
                lexer_advance(l);
                tok.type = TOK_INC;
            } else if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_ADD_ASSIGN;
            } else {
                tok.type = TOK_PLUS;
            }
            break;
        case '-':
            if (lexer_peek(l) == '-') {
                lexer_advance(l);
                tok.type = TOK_DEC;
            } else if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_SUB_ASSIGN;
            } else {
                tok.type = TOK_MINUS;
            }
            break;
        case '*':
            if (lexer_peek(l) == '*') {
                lexer_advance(l);
                tok.type = TOK_POWER;
            } else if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_MUL_ASSIGN;
            } else {
                tok.type = TOK_STAR;
            }
            break;
        case '/':
            if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_DIV_ASSIGN;
            } else {
                tok.type = TOK_SLASH;
            }
            break;
        case '%':
            if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_MOD_ASSIGN;
            } else {
                tok.type = TOK_PERCENT;
            }
            break;
        case '<':
            if (lexer_peek(l) == '<') {
                lexer_advance(l);
                if (lexer_peek(l) == '=') {
                    lexer_advance(l);
                    tok.type = TOK_LSHIFT_ASSIGN;
                } else {
                    tok.type = TOK_LSHIFT;
                }
            } else if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_LE;
            } else {
                tok.type = TOK_LT;
            }
            break;
        case '>':
            if (lexer_peek(l) == '>') {
                lexer_advance(l);
                if (lexer_peek(l) == '=') {
                    lexer_advance(l);
                    tok.type = TOK_RSHIFT_ASSIGN;
                } else {
                    tok.type = TOK_RSHIFT;
                }
            } else if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_GE;
            } else {
                tok.type = TOK_GT;
            }
            break;
        case '=':
            if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_EQ;
            } else {
                tok.type = TOK_ASSIGN;
            }
            break;
        case '!':
            if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_NE;
            } else {
                tok.type = TOK_LOG_NOT;
            }
            break;
        case '&':
            if (lexer_peek(l) == '&') {
                lexer_advance(l);
                tok.type = TOK_LOG_AND;
            } else if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_AND_ASSIGN;
            } else {
                tok.type = TOK_BIT_AND;
            }
            break;
        case '|':
            if (lexer_peek(l) == '|') {
                lexer_advance(l);
                tok.type = TOK_LOG_OR;
            } else if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_OR_ASSIGN;
            } else {
                tok.type = TOK_BIT_OR;
            }
            break;
        case '^':
            if (lexer_peek(l) == '=') {
                lexer_advance(l);
                tok.type = TOK_XOR_ASSIGN;
            } else {
                tok.type = TOK_BIT_XOR;
            }
            break;
        case '~':
            tok.type = TOK_BIT_NOT;
            break;
        case '?':
            tok.type = TOK_QUESTION;
            break;
        case ':':
            tok.type = TOK_COLON;
            break;
        case ',':
            tok.type = TOK_COMMA;
            break;
        case '(':
            tok.type = TOK_LPAREN;
            break;
        case ')':
            tok.type = TOK_RPAREN;
            break;
        default:
            tok.type = TOK_ERROR;
            break;
    }

    return tok;
}

/* Forward declarations for parser grammar */
static int64_t parse_expr(ArithParser *p, bool eval);
static int64_t parse_assign(ArithParser *p, bool eval);
static int64_t parse_conditional(ArithParser *p, bool eval);
static int64_t parse_logical_or(ArithParser *p, bool eval);
static int64_t parse_logical_and(ArithParser *p, bool eval);
static int64_t parse_bitwise_or(ArithParser *p, bool eval);
static int64_t parse_bitwise_xor(ArithParser *p, bool eval);
static int64_t parse_bitwise_and(ArithParser *p, bool eval);
static int64_t parse_equality(ArithParser *p, bool eval);
static int64_t parse_relational(ArithParser *p, bool eval);
static int64_t parse_shift(ArithParser *p, bool eval);
static int64_t parse_additive(ArithParser *p, bool eval);
static int64_t parse_multiplicative(ArithParser *p, bool eval);
static int64_t parse_power(ArithParser *p, bool eval);
static int64_t parse_unary(ArithParser *p, bool eval);
static int64_t parse_postfix(ArithParser *p, bool eval);
static int64_t parse_primary(ArithParser *p, bool eval, bool *is_ident,
                             char **ident_name);

static void parser_advance(ArithParser *p) {
    free_token(&p->cur);
    p->cur  = p->peek;
    p->peek = arith_lexer_next_token(&p->lexer);
}

static void init_parser(ArithParser *p, const char *expr, Session *session,
                        int recursion_depth) {
    p->lexer.src       = expr;
    p->lexer.pos       = 0;
    p->lexer.len       = strlen(expr);
    p->session         = session;
    p->had_error       = false;
    p->recursion_depth = recursion_depth;
    p->cur             = arith_lexer_next_token(&p->lexer);
    p->peek            = arith_lexer_next_token(&p->lexer);
}

static void free_parser(ArithParser *p) {
    free_token(&p->cur);
    free_token(&p->peek);
}

static void report_error(ArithParser *p, const char *msg) {
    if (!p->had_error) {
        fprintf(stderr, "tidesh: %s\n", msg);
        p->had_error = true;
    }
}

static bool match_token(ArithParser *p, ArithTokenType type) {
    if (p->cur.type == type) {
        parser_advance(p);
        return true;
    }
    return false;
}

static bool expect_token(ArithParser *p, ArithTokenType type, const char *msg) {
    if (p->cur.type == type) {
        parser_advance(p);
        return true;
    }
    report_error(p, msg);
    return false;
}

static bool evaluate_arithmetic_expression_rec(const char *expr,
                                               Session    *session,
                                               int64_t *result, int depth);

static int64_t get_var_value(ArithParser *p, const char *name) {
    if (!p->session || !p->session->environ) {
        return 0;
    }
    char *val = environ_get(p->session->environ, (char *)name);
    if (!val || !*val) {
        return 0;
    }

    char   *end = NULL;
    int64_t num = (int64_t)strtoll(val, &end, 0);
    while (end && isspace((unsigned char)*end)) {
        end++;
    }
    if (end && *end == '\0') {
        return num;
    }

    if (p->recursion_depth >= 32) {
        report_error(p, "expression recursion limit exceeded");
        return 0;
    }

    int64_t rec_result = 0;
    bool ok = evaluate_arithmetic_expression_rec(val, p->session, &rec_result,
                                                 p->recursion_depth + 1);
    if (!ok) {
        p->had_error = true;
        return 0;
    }
    return rec_result;
}

static void set_var_value(ArithParser *p, const char *name, int64_t val) {
    if (!p->session || !p->session->environ) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)val);
    environ_set(p->session->environ, (char *)name, buf);
}

static int64_t ipow(int64_t base, int64_t exp) {
    if (exp < 0) {
        if (base == 1) {
            return 1;
        }
        if (base == -1) {
            return (exp % 2 != 0) ? -1 : 1;
        }
        return 0;
    }
    if (exp == 0) {
        return 1;
    }
    if (base == 0) {
        return 0;
    }

    int64_t result = 1;
    while (exp > 0) {
        if (exp & 1) {
            result *= base;
        }
        base *= base;
        exp >>= 1;
    }
    return result;
}

/* 1. Comma operator: expr ( ',' expr )* */
static int64_t parse_expr(ArithParser *p, bool eval) {
    int64_t val = parse_assign(p, eval);
    while (match_token(p, TOK_COMMA)) {
        val = parse_assign(p, eval);
    }
    return val;
}

/* 2. Assignment operators (right-associative) */
static int64_t parse_assign(ArithParser *p, bool eval) {
    if (p->cur.type == TOK_IDENT && p->peek.type >= TOK_ASSIGN &&
        p->peek.type <= TOK_OR_ASSIGN) {
        char          *name = strdup(p->cur.name);
        ArithTokenType op   = p->peek.type;
        parser_advance(p); // consume IDENT
        parser_advance(p); // consume assignment op

        int64_t rhs     = parse_assign(p, eval);
        int64_t old_val = eval ? get_var_value(p, name) : 0;
        int64_t new_val = 0;

        switch (op) {
            case TOK_ASSIGN:
                new_val = rhs;
                break;
            case TOK_ADD_ASSIGN:
                new_val = old_val + rhs;
                break;
            case TOK_SUB_ASSIGN:
                new_val = old_val - rhs;
                break;
            case TOK_MUL_ASSIGN:
                new_val = old_val * rhs;
                break;
            case TOK_DIV_ASSIGN:
                if (eval && rhs == 0) {
                    report_error(p, "division by 0");
                    free(name);
                    return 0;
                }
                new_val = (eval && rhs != 0) ? (old_val / rhs) : 0;
                break;
            case TOK_MOD_ASSIGN:
                if (eval && rhs == 0) {
                    report_error(p, "division by 0");
                    free(name);
                    return 0;
                }
                new_val = (eval && rhs != 0) ? (old_val % rhs) : 0;
                break;
            case TOK_LSHIFT_ASSIGN:
                new_val = (rhs < 0 || rhs >= 64) ? 0 : (old_val << rhs);
                break;
            case TOK_RSHIFT_ASSIGN:
                new_val = (rhs < 0 || rhs >= 64) ? 0 : (old_val >> rhs);
                break;
            case TOK_AND_ASSIGN:
                new_val = old_val & rhs;
                break;
            case TOK_XOR_ASSIGN:
                new_val = old_val ^ rhs;
                break;
            case TOK_OR_ASSIGN:
                new_val = old_val | rhs;
                break;
            default:
                break;
        }

        if (eval && !p->had_error) {
            set_var_value(p, name, new_val);
        }
        free(name);
        return new_val;
    }

    return parse_conditional(p, eval);
}

/* 3. Conditional (ternary) operator: cond ? then_expr : else_expr */
static int64_t parse_conditional(ArithParser *p, bool eval) {
    int64_t cond = parse_logical_or(p, eval);
    if (match_token(p, TOK_QUESTION)) {
        bool    then_eval = eval && (cond != 0);
        int64_t then_val  = parse_conditional(p, then_eval);
        if (!expect_token(
                p, TOK_COLON,
                "syntax error: `:' expected in conditional expression")) {
            return 0;
        }
        bool    else_eval = eval && (cond == 0);
        int64_t else_val  = parse_conditional(p, else_eval);
        return eval ? (cond != 0 ? then_val : else_val) : 0;
    }
    return cond;
}

/* 4. Logical OR || (short-circuit) */
static int64_t parse_logical_or(ArithParser *p, bool eval) {
    int64_t left = parse_logical_and(p, eval);
    while (match_token(p, TOK_LOG_OR)) {
        bool    right_eval = eval && (left == 0);
        int64_t right      = parse_logical_and(p, right_eval);
        if (eval) {
            left = (left != 0 || right != 0) ? 1 : 0;
        }
    }
    return left;
}

/* 5. Logical AND && (short-circuit) */
static int64_t parse_logical_and(ArithParser *p, bool eval) {
    int64_t left = parse_bitwise_or(p, eval);
    while (match_token(p, TOK_LOG_AND)) {
        bool    right_eval = eval && (left != 0);
        int64_t right      = parse_bitwise_or(p, right_eval);
        if (eval) {
            left = (left != 0 && right != 0) ? 1 : 0;
        }
    }
    return left;
}

/* 6. Bitwise OR | */
static int64_t parse_bitwise_or(ArithParser *p, bool eval) {
    int64_t left = parse_bitwise_xor(p, eval);
    while (match_token(p, TOK_BIT_OR)) {
        int64_t right = parse_bitwise_xor(p, eval);
        if (eval) {
            left = left | right;
        }
    }
    return left;
}

/* 7. Bitwise XOR ^ */
static int64_t parse_bitwise_xor(ArithParser *p, bool eval) {
    int64_t left = parse_bitwise_and(p, eval);
    while (match_token(p, TOK_BIT_XOR)) {
        int64_t right = parse_bitwise_and(p, eval);
        if (eval) {
            left = left ^ right;
        }
    }
    return left;
}

/* 8. Bitwise AND & */
static int64_t parse_bitwise_and(ArithParser *p, bool eval) {
    int64_t left = parse_equality(p, eval);
    while (match_token(p, TOK_BIT_AND)) {
        int64_t right = parse_equality(p, eval);
        if (eval) {
            left = left & right;
        }
    }
    return left;
}

/* 9. Equality ==, != */
static int64_t parse_equality(ArithParser *p, bool eval) {
    int64_t left = parse_relational(p, eval);
    while (p->cur.type == TOK_EQ || p->cur.type == TOK_NE) {
        ArithTokenType op = p->cur.type;
        parser_advance(p);
        int64_t right = parse_relational(p, eval);
        if (eval) {
            left = (op == TOK_EQ) ? (left == right) : (left != right);
        }
    }
    return left;
}

/* 10. Relational <, <=, >, >= */
static int64_t parse_relational(ArithParser *p, bool eval) {
    int64_t left = parse_shift(p, eval);
    while (p->cur.type == TOK_LT || p->cur.type == TOK_LE ||
           p->cur.type == TOK_GT || p->cur.type == TOK_GE) {
        ArithTokenType op = p->cur.type;
        parser_advance(p);
        int64_t right = parse_shift(p, eval);
        if (eval) {
            switch (op) {
                case TOK_LT:
                    left = (left < right);
                    break;
                case TOK_LE:
                    left = (left <= right);
                    break;
                case TOK_GT:
                    left = (left > right);
                    break;
                case TOK_GE:
                    left = (left >= right);
                    break;
                default:
                    break;
            }
        }
    }
    return left;
}

/* 11. Bitwise shift <<, >> */
static int64_t parse_shift(ArithParser *p, bool eval) {
    int64_t left = parse_additive(p, eval);
    while (p->cur.type == TOK_LSHIFT || p->cur.type == TOK_RSHIFT) {
        ArithTokenType op = p->cur.type;
        parser_advance(p);
        int64_t right = parse_additive(p, eval);
        if (eval) {
            if (right < 0 || right >= 64) {
                left = 0;
            } else if (op == TOK_LSHIFT) {
                left = left << right;
            } else {
                left = left >> right;
            }
        }
    }
    return left;
}

/* 12. Additive +, - */
static int64_t parse_additive(ArithParser *p, bool eval) {
    int64_t left = parse_multiplicative(p, eval);
    while (p->cur.type == TOK_PLUS || p->cur.type == TOK_MINUS) {
        ArithTokenType op = p->cur.type;
        parser_advance(p);
        int64_t right = parse_multiplicative(p, eval);
        if (eval) {
            left = (op == TOK_PLUS) ? (left + right) : (left - right);
        }
    }
    return left;
}

/* 13. Multiplicative *, /, % */
static int64_t parse_multiplicative(ArithParser *p, bool eval) {
    int64_t left = parse_power(p, eval);
    while (p->cur.type == TOK_STAR || p->cur.type == TOK_SLASH ||
           p->cur.type == TOK_PERCENT) {
        ArithTokenType op = p->cur.type;
        parser_advance(p);
        int64_t right = parse_power(p, eval);
        if (eval) {
            if (op == TOK_STAR) {
                left = left * right;
            } else if (op == TOK_SLASH) {
                if (right == 0) {
                    report_error(p, "division by 0");
                    return 0;
                }
                left = left / right;
            } else if (op == TOK_PERCENT) {
                if (right == 0) {
                    report_error(p, "division by 0");
                    return 0;
                }
                left = left % right;
            }
        }
    }
    return left;
}

/* 14. Exponentiation ** (right-associative) */
static int64_t parse_power(ArithParser *p, bool eval) {
    int64_t base = parse_unary(p, eval);
    if (match_token(p, TOK_POWER)) {
        int64_t exp = parse_power(p, eval);
        if (eval) {
            return ipow(base, exp);
        }
        return 0;
    }
    return base;
}

/* 15. Unary operators +, -, !, ~, ++var, --var */
static int64_t parse_unary(ArithParser *p, bool eval) {
    if (match_token(p, TOK_PLUS)) {
        return parse_unary(p, eval);
    }
    if (match_token(p, TOK_MINUS)) {
        int64_t val = parse_unary(p, eval);
        return eval ? -val : 0;
    }
    if (match_token(p, TOK_LOG_NOT)) {
        int64_t val = parse_unary(p, eval);
        return eval ? (val == 0 ? 1 : 0) : 0;
    }
    if (match_token(p, TOK_BIT_NOT)) {
        int64_t val = parse_unary(p, eval);
        return eval ? ~val : 0;
    }
    if (p->cur.type == TOK_INC || p->cur.type == TOK_DEC) {
        ArithTokenType op = p->cur.type;
        parser_advance(p);
        if (p->cur.type != TOK_IDENT) {
            report_error(p, "lvalue required as increment/decrement operand");
            return 0;
        }
        char   *name    = strdup(p->cur.name);
        int64_t old_val = eval ? get_var_value(p, name) : 0;
        int64_t new_val = (op == TOK_INC) ? (old_val + 1) : (old_val - 1);
        if (eval && !p->had_error) {
            set_var_value(p, name, new_val);
        }
        free(name);
        parser_advance(p);
        return new_val;
    }

    return parse_postfix(p, eval);
}

/* 16. Postfix operators var++, var-- */
static int64_t parse_postfix(ArithParser *p, bool eval) {
    bool    is_ident   = false;
    char   *ident_name = NULL;
    int64_t val        = parse_primary(p, eval, &is_ident, &ident_name);

    if (is_ident && (p->cur.type == TOK_INC || p->cur.type == TOK_DEC)) {
        ArithTokenType op      = p->cur.type;
        int64_t        old_val = val;
        int64_t new_val = (op == TOK_INC) ? (old_val + 1) : (old_val - 1);
        parser_advance(p);
        if (eval && !p->had_error) {
            set_var_value(p, ident_name, new_val);
        }
        free(ident_name);
        return old_val; // Postfix returns value before modification
    }

    if (ident_name) {
        free(ident_name);
    }
    return val;
}

/* 17. Primary expressions: int, ident, ( expr ) */
static int64_t parse_primary(ArithParser *p, bool eval, bool *is_ident,
                             char **ident_name) {
    *is_ident   = false;
    *ident_name = NULL;

    if (p->cur.type == TOK_INT) {
        int64_t val = p->cur.val;
        parser_advance(p);
        return val;
    }

    if (p->cur.type == TOK_IDENT) {
        *is_ident   = true;
        *ident_name = strdup(p->cur.name);
        int64_t val = eval ? get_var_value(p, p->cur.name) : 0;
        parser_advance(p);
        return val;
    }

    if (match_token(p, TOK_LPAREN)) {
        int64_t val = parse_expr(p, eval);
        if (!expect_token(p, TOK_RPAREN, "syntax error: `)' expected")) {
            return 0;
        }
        return val;
    }

    if (p->cur.type == TOK_EOF) {
        return 0; // Empty expression
    }

    report_error(p, "syntax error: operand expected");
    return 0;
}

static bool evaluate_arithmetic_expression_rec(const char *expr,
                                               Session    *session,
                                               int64_t *result, int depth) {
    if (!expr) {
        if (result) {
            *result = 0;
        }
        return true;
    }

    ArithParser p = {0};
    init_parser(&p, expr, session, depth);

    if (p.cur.type == TOK_EOF) {
        if (result) {
            *result = 0;
        }
        free_parser(&p);
        return true;
    }

    int64_t val = parse_expr(&p, true);
    if (p.cur.type != TOK_EOF && !p.had_error) {
        report_error(&p, "syntax error in expression (unexpected token)");
    }

    bool ok = !p.had_error;
    if (ok && result) {
        *result = val;
    } else if (result) {
        *result = 0;
    }

    free_parser(&p);
    return ok;
}

bool evaluate_arithmetic_expression(const char *expr, Session *session,
                                    int64_t *result) {
    return evaluate_arithmetic_expression_rec(expr, session, result, 0);
}

char *expand_arithmetic(const char *input, Session *session) {
    if (!input) {
        return NULL;
    }
#ifndef TIDESH_DISABLE_EXPANSIONS
    if (session && !session->features.arithmetic_expansion) {
        return strdup(input);
    }
    if (strstr(input, "$((") == NULL) {
        return strdup(input);
    }

    Dynamic result = {0};
    init_dynamic(&result);
    size_t i   = 0;
    size_t len = strlen(input);

    while (i < len) {
        if (input[i] == '\\' && input[i + 1] == '$') {
            dynamic_append(&result, '\\');
            dynamic_append(&result, '$');
            i += 2;
            continue;
        }

        if (input[i] == '$' && i + 2 < len && input[i + 1] == '(' &&
            input[i + 2] == '(') {
            i += 3; // skip "$(("
            size_t  pdepth   = 0;
            Dynamic expr_dyn = {0};
            init_dynamic(&expr_dyn);
            bool found_close = false;

            while (i < len) {
                char c = input[i];
                if (c == '\\' && i + 1 < len) {
                    dynamic_append(&expr_dyn, c);
                    dynamic_append(&expr_dyn, input[++i]);
                    i++;
                    continue;
                }
                if (c == '\'') {
                    dynamic_append(&expr_dyn, c);
                    i++;
                    while (i < len && input[i] != '\'') {
                        dynamic_append(&expr_dyn, input[i++]);
                    }
                    if (i < len) {
                        dynamic_append(&expr_dyn, input[i++]);
                    }
                    continue;
                }
                if (c == '"') {
                    dynamic_append(&expr_dyn, c);
                    i++;
                    while (i < len && input[i] != '"') {
                        if (input[i] == '\\' && i + 1 < len) {
                            dynamic_append(&expr_dyn, input[i++]);
                        }
                        dynamic_append(&expr_dyn, input[i++]);
                    }
                    if (i < len) {
                        dynamic_append(&expr_dyn, input[i++]);
                    }
                    continue;
                }
                if (c == '(') {
                    pdepth++;
                    dynamic_append(&expr_dyn, c);
                    i++;
                } else if (c == ')') {
                    if (pdepth > 0) {
                        pdepth--;
                        dynamic_append(&expr_dyn, c);
                        i++;
                    } else if (i + 1 < len && input[i + 1] == ')') {
                        i += 2; // consume both ')'
                        found_close = true;
                        break;
                    } else {
                        dynamic_append(&expr_dyn, c);
                        i++;
                    }
                } else {
                    dynamic_append(&expr_dyn, c);
                    i++;
                }
            }

            if (!found_close) {
                fprintf(
                    stderr,
                    "tidesh: syntax error: unexpected EOF while looking for "
                    "matching `))'\n");
                free_dynamic(&expr_dyn);
                free_dynamic(&result);
                return NULL;
            }

            char *expr = dynamic_to_string(&expr_dyn);
            free_dynamic(&expr_dyn);

            // Nested arithmetic expansion $(( ... ))
            char *expanded_expr = NULL;
            if (strstr(expr, "$((") != NULL) {
                expanded_expr = expand_arithmetic(expr, session);
            } else {
                expanded_expr = strdup(expr);
            }
            free(expr);

#ifndef TIDESH_DISABLE_COMMAND_SUBSTITUTION
            if (session && session->features.command_substitution &&
                strstr(expanded_expr, "$(") != NULL) {
                char *cmd_sub =
                    expand_command_substitution(expanded_expr, session);
                if (cmd_sub) {
                    free(expanded_expr);
                    expanded_expr = cmd_sub;
                }
            }
#endif

            // Variable expansion ($VAR within arithmetic expression)
            if (session && session->features.variable_expansion &&
                strchr(expanded_expr, '$') != NULL) {
                Array *vars = variable_expansion(expanded_expr, session);
                if (vars && vars->count > 0) {
                    Dynamic joined = {0};
                    init_dynamic(&joined);
                    for (size_t k = 0; k < vars->count; k++) {
                        if (k > 0) {
                            dynamic_append(&joined, ' ');
                        }
                        dynamic_extend(&joined, vars->items[k]);
                    }
                    free(expanded_expr);
                    expanded_expr = dynamic_to_string(&joined);
                    free_dynamic(&joined);
                    free_array(vars);
                    free(vars);
                }
            }

            int64_t val = 0;
            bool    ok =
                evaluate_arithmetic_expression(expanded_expr, session, &val);
            free(expanded_expr);

            if (ok) {
                char val_str[32];
                snprintf(val_str, sizeof(val_str), "%lld", (long long)val);
                dynamic_extend(&result, val_str);
            } else {
                dynamic_append(&result, '0');
            }
        } else {
            dynamic_append(&result, input[i++]);
        }
    }

    char *ret = dynamic_to_string(&result);
    free_dynamic(&result);
    return ret;
#else
    return strdup(input);
#endif
}
