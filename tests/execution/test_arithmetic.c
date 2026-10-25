/** test_arithmetic.c
 *
 * Tests for integer arithmetic expansion $(( expression ))
 */

#include <stdlib.h>
#include <string.h>

#include "environ.h"
#include "execute.h"
#include "expand.h"
#include "expansions/arithmetic.h"
#include "session.h"
#include "snow/snow.h"

describe(arithmetic) {
    it("should evaluate basic arithmetic operators") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("1 + 2", session, &val));
        asserteq(val, 3);

        assert(evaluate_arithmetic_expression("10 - 4", session, &val));
        asserteq(val, 6);

        assert(evaluate_arithmetic_expression("3 * 7", session, &val));
        asserteq(val, 21);

        assert(evaluate_arithmetic_expression("20 / 3", session, &val));
        asserteq(val, 6);

        assert(evaluate_arithmetic_expression("20 % 3", session, &val));
        asserteq(val, 2);

        free_session(session);
        free(session);
    }

    it("should respect operator precedence and parentheses") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("2 + 3 * 4", session, &val));
        asserteq(val, 14);

        assert(evaluate_arithmetic_expression("(2 + 3) * 4", session, &val));
        asserteq(val, 20);

        assert(evaluate_arithmetic_expression("2 * 3 + 4", session, &val));
        asserteq(val, 10);

        assert(evaluate_arithmetic_expression("2 * (3 + 4)", session, &val));
        asserteq(val, 14);

        assert(evaluate_arithmetic_expression("((2 + 3)) * ((4 + 1))", session,
                                              &val));
        asserteq(val, 25);

        free_session(session);
        free(session);
    }

    it("should evaluate exponentiation with right-associativity") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("2 ** 3", session, &val));
        asserteq(val, 8);

        assert(evaluate_arithmetic_expression("2 ** 0", session, &val));
        asserteq(val, 1);

        assert(evaluate_arithmetic_expression("0 ** 5", session, &val));
        asserteq(val, 0);

        // 2 ** (3 ** 2) = 2 ** 9 = 512
        assert(evaluate_arithmetic_expression("2 ** 3 ** 2", session, &val));
        asserteq(val, 512);

        free_session(session);
        free(session);
    }

    it("should evaluate unary operators") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("+5", session, &val));
        asserteq(val, 5);

        assert(evaluate_arithmetic_expression("-5", session, &val));
        asserteq(val, -5);

        assert(evaluate_arithmetic_expression("-(-5)", session, &val));
        asserteq(val, 5);

        assert(evaluate_arithmetic_expression("!0", session, &val));
        asserteq(val, 1);

        assert(evaluate_arithmetic_expression("!5", session, &val));
        asserteq(val, 0);

        assert(evaluate_arithmetic_expression("~0", session, &val));
        asserteq(val, -1);

        assert(evaluate_arithmetic_expression("~(-1)", session, &val));
        asserteq(val, 0);

        free_session(session);
        free(session);
    }

    it("should support different integer number bases") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        // Decimal
        assert(evaluate_arithmetic_expression("42", session, &val));
        asserteq(val, 42);

        // Hexadecimal
        assert(evaluate_arithmetic_expression("0x10", session, &val));
        asserteq(val, 16);
        assert(evaluate_arithmetic_expression("0xFF", session, &val));
        asserteq(val, 255);

        // Octal
        assert(evaluate_arithmetic_expression("010", session, &val));
        asserteq(val, 8);
        assert(evaluate_arithmetic_expression("077", session, &val));
        asserteq(val, 63);

        // Base#number format
        assert(evaluate_arithmetic_expression("2#1010", session, &val));
        asserteq(val, 10);
        assert(evaluate_arithmetic_expression("8#77", session, &val));
        asserteq(val, 63);
        assert(evaluate_arithmetic_expression("16#FF", session, &val));
        asserteq(val, 255);
        assert(evaluate_arithmetic_expression("36#Z", session, &val));
        asserteq(val, 35);

        free_session(session);
        free(session);
    }

    it("should evaluate bitwise operators and shifts") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("1 << 4", session, &val));
        asserteq(val, 16);

        assert(evaluate_arithmetic_expression("16 >> 2", session, &val));
        asserteq(val, 4);

        assert(evaluate_arithmetic_expression("14 & 7", session, &val));
        asserteq(val, 6);

        assert(evaluate_arithmetic_expression("14 | 7", session, &val));
        asserteq(val, 15);

        assert(evaluate_arithmetic_expression("14 ^ 7", session, &val));
        asserteq(val, 9);

        free_session(session);
        free(session);
    }

    it("should evaluate comparison and equality operators") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("5 < 10", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("10 < 5", session, &val));
        asserteq(val, 0);

        assert(evaluate_arithmetic_expression("5 <= 5", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("6 <= 5", session, &val));
        asserteq(val, 0);

        assert(evaluate_arithmetic_expression("5 > 3", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("3 > 5", session, &val));
        asserteq(val, 0);

        assert(evaluate_arithmetic_expression("5 >= 5", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("4 >= 5", session, &val));
        asserteq(val, 0);

        assert(evaluate_arithmetic_expression("5 == 5", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("5 == 6", session, &val));
        asserteq(val, 0);

        assert(evaluate_arithmetic_expression("5 != 6", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("5 != 5", session, &val));
        asserteq(val, 0);

        free_session(session);
        free(session);
    }

    it("should evaluate logical operators and short-circuit") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("1 && 1", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("1 && 0", session, &val));
        asserteq(val, 0);

        assert(evaluate_arithmetic_expression("0 || 1", session, &val));
        asserteq(val, 1);
        assert(evaluate_arithmetic_expression("0 || 0", session, &val));
        asserteq(val, 0);

        // Short-circuiting assignment prevention
        environ_set(session->environ, "x", "0");
        assert(evaluate_arithmetic_expression("0 && (x = 1)", session, &val));
        asserteq(val, 0);
        asserteq_str(environ_get(session->environ, "x"), "0");

        assert(evaluate_arithmetic_expression("1 || (x = 1)", session, &val));
        asserteq(val, 1);
        asserteq_str(environ_get(session->environ, "x"), "0");

        // Short-circuiting error prevention (division by zero)
        assert(evaluate_arithmetic_expression("0 && (1 / 0)", session, &val));
        asserteq(val, 0);
        assert(evaluate_arithmetic_expression("1 || (1 / 0)", session, &val));
        asserteq(val, 1);

        free_session(session);
        free(session);
    }

    it("should evaluate conditional ternary operator and short-circuit") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("1 ? 10 : 20", session, &val));
        asserteq(val, 10);

        assert(evaluate_arithmetic_expression("0 ? 10 : 20", session, &val));
        asserteq(val, 20);

        // Short-circuit: division by zero in unselected branch
        assert(
            evaluate_arithmetic_expression("1 ? 42 : (1 / 0)", session, &val));
        asserteq(val, 42);

        assert(
            evaluate_arithmetic_expression("0 ? (1 / 0) : 42", session, &val));
        asserteq(val, 42);

        // Nested ternary
        assert(
            evaluate_arithmetic_expression("1 ? 2 ? 3 : 4 : 5", session, &val));
        asserteq(val, 3);

        free_session(session);
        free(session);
    }

    it("should evaluate variable assignment and compound assignments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("x = 42", session, &val));
        asserteq(val, 42);
        asserteq_str(environ_get(session->environ, "x"), "42");

        assert(evaluate_arithmetic_expression("x += 8", session, &val));
        asserteq(val, 50);
        asserteq_str(environ_get(session->environ, "x"), "50");

        assert(evaluate_arithmetic_expression("x -= 10", session, &val));
        asserteq(val, 40);
        asserteq_str(environ_get(session->environ, "x"), "40");

        assert(evaluate_arithmetic_expression("x *= 2", session, &val));
        asserteq(val, 80);
        asserteq_str(environ_get(session->environ, "x"), "80");

        assert(evaluate_arithmetic_expression("x /= 4", session, &val));
        asserteq(val, 20);
        asserteq_str(environ_get(session->environ, "x"), "20");

        assert(evaluate_arithmetic_expression("x %= 6", session, &val));
        asserteq(val, 2);
        asserteq_str(environ_get(session->environ, "x"), "2");

        // Chained assignment
        assert(evaluate_arithmetic_expression("a = b = 15", session, &val));
        asserteq(val, 15);
        asserteq_str(environ_get(session->environ, "a"), "15");
        asserteq_str(environ_get(session->environ, "b"), "15");

        free_session(session);
        free(session);
    }

    it("should evaluate increment and decrement operators") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        environ_set(session->environ, "n", "10");

        // Post-increment: returns old value (10), increments to 11
        assert(evaluate_arithmetic_expression("n++", session, &val));
        asserteq(val, 10);
        asserteq_str(environ_get(session->environ, "n"), "11");

        // Pre-increment: increments to 12, returns new value (12)
        assert(evaluate_arithmetic_expression("++n", session, &val));
        asserteq(val, 12);
        asserteq_str(environ_get(session->environ, "n"), "12");

        // Post-decrement: returns old value (12), decrements to 11
        assert(evaluate_arithmetic_expression("n--", session, &val));
        asserteq(val, 12);
        asserteq_str(environ_get(session->environ, "n"), "11");

        // Pre-decrement: decrements to 10, returns new value (10)
        assert(evaluate_arithmetic_expression("--n", session, &val));
        asserteq(val, 10);
        asserteq_str(environ_get(session->environ, "n"), "10");

        free_session(session);
        free(session);
    }

    it("should evaluate comma operator") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(evaluate_arithmetic_expression("x = 1, y = 2, x + y", session,
                                              &val));
        asserteq(val, 3);
        asserteq_str(environ_get(session->environ, "x"), "1");
        asserteq_str(environ_get(session->environ, "y"), "2");

        free_session(session);
        free(session);
    }

    it("should resolve variables directly and recursively") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        // Unset variable defaults to 0
        assert(evaluate_arithmetic_expression("not_set + 5", session, &val));
        asserteq(val, 5);

        // Defined variable
        environ_set(session->environ, "val", "25");
        assert(evaluate_arithmetic_expression("val * 2", session, &val));
        asserteq(val, 50);

        // Recursive variable resolution
        environ_set(session->environ, "ref", "val");
        assert(evaluate_arithmetic_expression("ref + 5", session, &val));
        asserteq(val, 30);

        free_session(session);
        free(session);
    }

    it("should handle division by zero error") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int64_t val = 0;
        assert(!evaluate_arithmetic_expression("10 / 0", session, &val));
        assert(!evaluate_arithmetic_expression("10 % 0", session, &val));

        free_session(session);
        free(session);
    }

    it("should expand $(( expression )) in strings") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *expanded = expand_arithmetic("result=$(( 1 + 2 ))", session);
        assertneq(expanded, NULL);
        asserteq_str(expanded, "result=3");
        free(expanded);

        expanded = expand_arithmetic("a=$((2*3)) b=$((10-5))", session);
        assertneq(expanded, NULL);
        asserteq_str(expanded, "a=6 b=5");
        free(expanded);

        expanded = expand_arithmetic("$(( (2 + 3) * 4 ))", session);
        assertneq(expanded, NULL);
        asserteq_str(expanded, "20");
        free(expanded);

        expanded = expand_arithmetic("$(( ))", session);
        assertneq(expanded, NULL);
        asserteq_str(expanded, "0");
        free(expanded);

        // Nested arithmetic expansion
        expanded = expand_arithmetic("$(( 1 + $(( 2 * 3 )) ))", session);
        assertneq(expanded, NULL);
        asserteq_str(expanded, "7");
        free(expanded);

        free_session(session);
        free(session);
    }

    it("should execute commands with integer arithmetic") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *output = execute_string_stdout("echo $(( 10 + 20 ))", session);
        assertneq(output, NULL);
        asserteq_str(output, "30");
        free(output);

        output = execute_string_stdout("echo $(( 5 * (2 + 3) ))", session);
        assertneq(output, NULL);
        asserteq_str(output, "25");
        free(output);

        output = execute_string_stdout("echo \"The total is $(( 4 * 7 ))!\"",
                                       session);
        assertneq(output, NULL);
        asserteq_str(output, "The total is 28!");
        free(output);

        // Variable assignment in arithmetic
        output = execute_string_stdout("echo $(( x = 12 )); echo $x", session);
        assertneq(output, NULL);
        asserteq_str(output, "12\n12");
        free(output);

        // Variable expansion inside arithmetic
        environ_set(session->environ, "A", "5");
        output =
            execute_string_stdout("echo $(( A + 15 )) $(( $A + 20 ))", session);
        assertneq(output, NULL);
        asserteq_str(output, "20 25");
        free(output);

        // Variable assignment from command
        execute_string("ANSWER=$(( 6 * 7 ))", session);
        asserteq_str(environ_get(session->environ, "ANSWER"), "42");

        free_session(session);
        free(session);
    }

    it("should integrate with command substitution inside arithmetic") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *output =
            execute_string_stdout("echo $(( 5 + $(echo 10) ))", session);
        assertneq(output, NULL);
        asserteq_str(output, "15");
        free(output);

        output = execute_string_stdout("echo $(echo $(( 5 * 5 )))", session);
        assertneq(output, NULL);
        asserteq_str(output, "25");
        free(output);

        free_session(session);
        free(session);
    }

    it("should work in loops and conditionals") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Conditional
        char *output = execute_string_stdout(
            "if [ $(( 2 + 2 )) -eq 4 ]; then echo correct; fi", session);
        assertneq(output, NULL);
        asserteq_str(output, "correct");
        free(output);

        // While loop
        output = execute_string_stdout("i=0; while [ $(( i < 3 )) -eq 1 ]; do "
                                       "i=$(( i + 1 )); done; echo $i",
                                       session);
        assertneq(output, NULL);
        asserteq_str(output, "3");
        free(output);

        // For loop
        output = execute_string_stdout(
            "for x in $(( 1 + 1 )) $(( 2 + 2 )); do echo $x; done", session);
        assertneq(output, NULL);
        asserteq_str(output, "2\n4");
        free(output);

        // Case statement
        output = execute_string_stdout(
            "case $(( 10 % 3 )) in 1) echo one ;; 2) echo two ;; esac",
            session);
        assertneq(output, NULL);
        asserteq_str(output, "one");
        free(output);

        free_session(session);
        free(session);
    }

    it("should honor feature toggle for arithmetic expansion") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Disable arithmetic expansion
        session->features.arithmetic_expansion = false;
        char *expanded = expand_arithmetic("$(( 1 + 2 ))", session);
        asserteq_str(expanded, "$(( 1 + 2 ))");
        free(expanded);

        // Re-enable arithmetic expansion
        session->features.arithmetic_expansion = true;
        expanded = expand_arithmetic("$(( 1 + 2 ))", session);
        asserteq_str(expanded, "3");
        free(expanded);

        free_session(session);
        free(session);
    }
}
