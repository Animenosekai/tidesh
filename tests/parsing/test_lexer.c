#include "lexer.h"
#include "snow/snow.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void consume_all_tokens(LexerInput *input) {
    size_t guard = 0;

    while (guard < 2048) {
        LexerToken token = lexer_next_token(input);
        TokenType  type  = token.type;

        free_lexer_token(&token);

        if (type == TOKEN_EOF) {
            break;
        }

        guard++;
    }

    assert(guard < 2048);
}

static bool contains_token_type(LexerInput *input, TokenType expected) {
    size_t guard = 0;

    while (guard < 2048) {
        LexerToken token = lexer_next_token(input);
        TokenType  type  = token.type;

        free_lexer_token(&token);

        if (type == expected) {
            return true;
        }

        if (type == TOKEN_EOF) {
            break;
        }

        guard++;
    }

    assert(guard < 2048);
    return false;
}

describe(lexer) {
    it("should initialize lexer input") {
        LexerInput *input = init_lexer_input(NULL, "test command", NULL, NULL);
        assertneq(input, NULL);
        asserteq_str(input->data, "test command");
        asserteq(input->pos, 0);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "test");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "command");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);
        free_lexer_input(input);
        free(input);
    }

    it("should handle empty input") {
        LexerInput *input = init_lexer_input(NULL, "", NULL, NULL);
        assertneq(input, NULL);
        asserteq_str(input->data, "");
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);
        free_lexer_input(input);
        free(input);
    }

    it("should handle simple word token") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *input   = init_lexer_input(NULL, "echo", NULL, session);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);
        consume_all_tokens(input);
        free_lexer_input(input);
        free(input);
        free_session(session);
        free(session);
    }

    it("should handle multiple words") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        LexerInput *input =
            init_lexer_input(NULL, "echo hello world", NULL, session);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);
        consume_all_tokens(input);
        free_lexer_input(input);
        free(input);
        free_session(session);
        free(session);
    }

    it("should handle pipe operator") {
        LexerInput *input =
            init_lexer_input(NULL, "cat file | grep pattern", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_PIPE));
        free_lexer_input(input);
        free(input);
    }

    it("should handle redirection operators") {
        LexerInput *input =
            init_lexer_input(NULL, "cat > output.txt < input.txt", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT));
        assert(contains_token_type(input, TOKEN_REDIRECT_IN));
        free_lexer_input(input);
        free(input);
    }

    it("should handle AND operator") {
        LexerInput *input = init_lexer_input(NULL, "cmd1 && cmd2", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_SEQUENCE));
        free_lexer_input(input);
        free(input);
    }

    it("should handle OR operator") {
        LexerInput *input = init_lexer_input(NULL, "cmd1 || cmd2", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_OR));
        free_lexer_input(input);
        free(input);
    }

    it("should handle semicolon") {
        LexerInput *input = init_lexer_input(NULL, "cmd1 ; cmd2", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_SEMICOLON));
        free_lexer_input(input);
        free(input);
    }

    it("should handle background operator") {
        LexerInput *input = init_lexer_input(NULL, "sleep 10 &", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_BACKGROUND));
        free_lexer_input(input);
        free(input);
    }

    it("should handle parentheses") {
        LexerInput *input = init_lexer_input(NULL, "(echo test)", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_LPAREN));
        assert(contains_token_type(input, TOKEN_RPAREN));
        free_lexer_input(input);
        free(input);
    }

    it("should handle variable assignment") {
        LexerInput *input = init_lexer_input(NULL, "VAR=value", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_ASSIGNMENT));
        free_lexer_input(input);
        free(input);
    }

    it("should handle quoted strings") {
        LexerInput *input =
            init_lexer_input(NULL, "echo \"hello world\"", NULL, NULL);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "hello world");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);
        free_lexer_input(input);
        free(input);
    }

    it("should handle single quoted strings") {
        LexerInput *input =
            init_lexer_input(NULL, "echo 'hello world'", NULL, NULL);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "hello world");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);
        free_lexer_input(input);
        free(input);
    }

    it("should handle comments") {
        LexerInput *input =
            init_lexer_input(NULL, "echo test # this is a comment", NULL, NULL);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "test");
        free_lexer_token(&token);

        consume_all_tokens(input);
        free_lexer_input(input);
        free(input);
    }

    it("should handle IO number redirection") {
        LexerInput *input = init_lexer_input(NULL, "2>&1", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_IO_NUMBER));
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT_ERR));
        free_lexer_input(input);
        free(input);
    }

    it("should handle process substitution input") {
        LexerInput *input =
            init_lexer_input(NULL, "cat <(echo test)", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_PROCESS_SUBSTITUTION_IN));
        free_lexer_input(input);
        free(input);
    }

    it("should handle process substitution output") {
        LexerInput *input =
            init_lexer_input(NULL, "cat >(tee file)", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_PROCESS_SUBSTITUTION_OUT));
        free_lexer_input(input);
        free(input);
    }

    it("should handle heredoc") {
        LexerInput *input = init_lexer_input(NULL, "cat << EOF", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_HEREDOC));
        free_lexer_input(input);
        free(input);
    }

    it("should handle herestring") {
        LexerInput *input =
            init_lexer_input(NULL, "cat <<< \"test\"", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_HERESTRING));
        free_lexer_input(input);
        free(input);
    }

    it("should handle append redirection") {
        LexerInput *input =
            init_lexer_input(NULL, "echo >> file.txt", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_REDIRECT_APPEND));
        free_lexer_input(input);
        free(input);
    }

    it("should handle complex command") {
        LexerInput *input = init_lexer_input(
            NULL, "VAR=value cmd1 arg1 arg2 | cmd2 > out.txt 2>&1 & echo done",
            NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_ASSIGNMENT));
        assert(contains_token_type(input, TOKEN_PIPE));
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT));
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT_ERR));
        assert(contains_token_type(input, TOKEN_BACKGROUND));
        free_lexer_input(input);
        free(input);
    }

    it("should initialize with callback functions") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Use NULL for execute function in test
        LexerInput *input = init_lexer_input(NULL, "echo test", NULL, session);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "test");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);
        free_lexer_input(input);
        free(input);
        free_session(session);
        free(session);
    }

    it("should handle command substitution when execute callback is NULL") {
        LexerInput *input =
            init_lexer_input(NULL, "echo $(whoami) \"$(date)\"", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "$(whoami)");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "$(date)");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle whitespace") {
        LexerInput *input =
            init_lexer_input(NULL, "   echo   test   ", NULL, NULL);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "test");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);
        free_lexer_input(input);
        free(input);
    }

    it("should handle &> and &>> redirection operators") {
        LexerInput *input1 =
            init_lexer_input(NULL, "echo hello &> out.txt", NULL, NULL);
        assertneq(input1, NULL);
        assert(contains_token_type(input1, TOKEN_REDIRECT_ALL));
        free_lexer_input(input1);
        free(input1);

        LexerInput *input2 =
            init_lexer_input(NULL, "echo hello &>> out.txt", NULL, NULL);
        assertneq(input2, NULL);
        assert(contains_token_type(input2, TOKEN_REDIRECT_APPEND_ERR));
        free_lexer_input(input2);
        free(input2);
    }

    it("should handle >>& and >| redirection operators") {
        LexerInput *input1 =
            init_lexer_input(NULL, "echo hello >>& out.txt", NULL, NULL);
        assertneq(input1, NULL);
        assert(contains_token_type(input1, TOKEN_REDIRECT_APPEND_ERR));
        free_lexer_input(input1);
        free(input1);

        LexerInput *input2 =
            init_lexer_input(NULL, "echo hello >| out.txt", NULL, NULL);
        assertneq(input2, NULL);
        assert(contains_token_type(input2, TOKEN_REDIRECT_OUT));
        free_lexer_input(input2);
        free(input2);
    }

    it("should tokenize for loop keywords") {
        LexerInput *input = init_lexer_input(
            NULL, "for x in a b; do echo $x; done", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_FOR);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "x");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_IN);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "a");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "b");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_SEMICOLON);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_DO);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_SEMICOLON);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_DONE);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should tokenize while loop keywords") {
        LexerInput *input = init_lexer_input(
            NULL, "while true; do echo loop; done", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WHILE);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "true");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_SEMICOLON);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_DO);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "loop");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_SEMICOLON);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_DONE);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should tokenize until loop keywords") {
        LexerInput *input = init_lexer_input(
            NULL, "until false; do echo loop; done", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_UNTIL);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "false");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_SEMICOLON);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_DO);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "loop");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_SEMICOLON);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_DONE);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle line continuation with trailing backslash") {
        LexerInput *input =
            init_lexer_input(NULL, "echo foo \\\nbar", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "foo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "bar");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle line continuation with indentation") {
        LexerInput *input = init_lexer_input(
            NULL, "echo \\\n    first \\\n    second", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "first");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "second");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle line continuation inside a word") {
        LexerInput *input =
            init_lexer_input(NULL, "echo foo\\\nbar", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "foobar");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle line continuation with operators") {
        LexerInput *input =
            init_lexer_input(NULL, "cmd1 && \\\ncmd2", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "cmd1");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_SEQUENCE);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "cmd2");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle line continuation inside double quotes") {
        LexerInput *input =
            init_lexer_input(NULL, "echo \"hello \\\nworld\"", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "hello world");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should preserve literal backslash and newline in single quotes") {
        LexerInput *input =
            init_lexer_input(NULL, "echo 'hello \\\nworld'", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "hello \\\nworld");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle CRLF line continuation") {
        LexerInput *input =
            init_lexer_input(NULL, "echo foo \\\r\nbar", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "foo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "bar");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle line continuation in comments") {
        LexerInput *input = init_lexer_input(
            NULL, "# comment \\\ncontinuation\necho test", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_COMMENT);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOL);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "test");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should not return EOL when line continuation is at end of input") {
        LexerInput *input = init_lexer_input(NULL, "echo foo \\\n", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "foo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should tokenize case statement keywords and terminators") {
        LexerInput *input = init_lexer_input(
            NULL, "case $x in a) echo 1 ;; b) echo 2 ;& c) echo 3 ;;& esac",
            NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_CASE);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "$x");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_IN);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "a");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_RPAREN);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "1");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_DOUBLE_SEMICOLON);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "b");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_RPAREN);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "2");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_CASE_FALLTHROUGH);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "c");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_RPAREN);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "3");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_CASE_CONTINUE);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_ESAC);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle nested parameter expansions in double quotes") {
        LexerInput *input = init_lexer_input(
            NULL, "echo \"${line#\"${line%%[![:space:]]*}\"}\"", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "${line#\"${line%%[![:space:]]*}\"}");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle adjacent quotes and mixed quoted-unquoted words") {
        // "#bench"[[:space:]]*
        LexerInput *input =
            init_lexer_input(NULL, "\"#bench\"[[:space:]]*", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "#bench[[:space:]]*");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);

        // ("#bench"[[:space:]]*)
        input = init_lexer_input(NULL, "(\"#bench\"[[:space:]]*)", NULL, NULL);
        assertneq(input, NULL);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_LPAREN);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "#bench[[:space:]]*");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_RPAREN);
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);

        // Concatenated quotes: "foo""bar" and 'foo'"bar"
        input = init_lexer_input(
            NULL, "\"foo\"\"bar\" 'foo'\"bar\" \"foo\"bar\"baz\"", NULL, NULL);
        assertneq(input, NULL);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "foobar");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "foobar");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "foobarbaz");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle escaped quotes inside double quotes") {
        LexerInput *input =
            init_lexer_input(NULL, "\"hello \\\"world\\\"\"", NULL, NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "hello \"world\"");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);

        // eval "set -- $1 \"\${$1}\" \"\" \"\""
        input = init_lexer_input(
            NULL, "eval \"set -- $1 \\\"\\${$1}\\\" \\\"\\\" \\\"\\\"\"", NULL,
            NULL);
        assertneq(input, NULL);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "eval");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "set -- $1 \"\\${$1}\" \"\" \"\"");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }

    it("should handle command substitution containing escaped characters and "
       "quotes") {
        LexerInput *input = init_lexer_input(
            NULL, "chunk=$(printf '%s\\n' \"$initializer\"; read_chunk)", NULL,
            NULL);
        assertneq(input, NULL);

        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_ASSIGNMENT);
        asserteq_str(token.value, "chunk");
        asserteq_str(token.extra,
                     "$(printf '%s\\n' \"$initializer\"; read_chunk)");
        free_lexer_token(&token);

        token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);

        free_lexer_input(input);
        free(input);
    }
}
