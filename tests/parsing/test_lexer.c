#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "snow/snow.h"

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
    }

    it("should handle empty input") {
        LexerInput *input = init_lexer_input(NULL, "", NULL, NULL);
        assertneq(input, NULL);
        asserteq_str(input->data, "");
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_EOF);
        free_lexer_token(&token);
        free_lexer_input(input);
    }

    it("should handle simple word token") {
        Session *session = init_session(NULL, "/tmp/test_history");
        LexerInput *input = init_lexer_input(NULL, "echo", NULL, session);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);
        consume_all_tokens(input);
        free_lexer_input(input);
        free_session(session);
    }

    it("should handle multiple words") {
        Session *session = init_session(NULL, "/tmp/test_history");
        LexerInput *input = init_lexer_input(NULL, "echo hello world", NULL, session);
        assertneq(input, NULL);
        LexerToken token = lexer_next_token(input);
        asserteq(token.type, TOKEN_WORD);
        asserteq_str(token.value, "echo");
        free_lexer_token(&token);
        consume_all_tokens(input);
        free_lexer_input(input);
        free_session(session);
    }

    it("should handle pipe operator") {
        LexerInput *input = init_lexer_input(NULL, "cat file | grep pattern", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_PIPE));
        free_lexer_input(input);
    }

    it("should handle redirection operators") {
        LexerInput *input = init_lexer_input(NULL, "cat > output.txt < input.txt", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT));
        assert(contains_token_type(input, TOKEN_REDIRECT_IN));
        free_lexer_input(input);
    }

    it("should handle AND operator") {
        LexerInput *input = init_lexer_input(NULL, "cmd1 && cmd2", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_SEQUENCE));
        free_lexer_input(input);
    }

    it("should handle OR operator") {
        LexerInput *input = init_lexer_input(NULL, "cmd1 || cmd2", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_OR));
        free_lexer_input(input);
    }

    it("should handle semicolon") {
        LexerInput *input = init_lexer_input(NULL, "cmd1 ; cmd2", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_SEMICOLON));
        free_lexer_input(input);
    }

    it("should handle background operator") {
        LexerInput *input = init_lexer_input(NULL, "sleep 10 &", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_BACKGROUND));
        free_lexer_input(input);
    }

    it("should handle parentheses") {
        LexerInput *input = init_lexer_input(NULL, "(echo test)", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_LPAREN));
        assert(contains_token_type(input, TOKEN_RPAREN));
        free_lexer_input(input);
    }

    it("should handle variable assignment") {
        LexerInput *input = init_lexer_input(NULL, "VAR=value", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_ASSIGNMENT));
        free_lexer_input(input);
    }

    it("should handle quoted strings") {
        LexerInput *input = init_lexer_input(NULL, "echo \"hello world\"", NULL, NULL);
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
    }

    it("should handle single quoted strings") {
        LexerInput *input = init_lexer_input(NULL, "echo 'hello world'", NULL, NULL);
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
    }

    it("should handle comments") {
        LexerInput *input = init_lexer_input(NULL, "echo test # this is a comment", NULL, NULL);
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
    }

    it("should handle IO number redirection") {
        LexerInput *input = init_lexer_input(NULL, "2>&1", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_IO_NUMBER));
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT_ERR));
        free_lexer_input(input);
    }

    it("should handle process substitution input") {
        LexerInput *input = init_lexer_input(NULL, "cat <(echo test)", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_PROCESS_SUBSTITUTION_IN));
        free_lexer_input(input);
    }

    it("should handle process substitution output") {
        LexerInput *input = init_lexer_input(NULL, "cat >(tee file)", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_PROCESS_SUBSTITUTION_OUT));
        free_lexer_input(input);
    }

    it("should handle heredoc") {
        LexerInput *input = init_lexer_input(NULL, "cat << EOF", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_HEREDOC));
        free_lexer_input(input);
    }

    it("should handle herestring") {
        LexerInput *input = init_lexer_input(NULL, "cat <<< \"test\"", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_HERESTRING));
        free_lexer_input(input);
    }

    it("should handle append redirection") {
        LexerInput *input = init_lexer_input(NULL, "echo >> file.txt", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_REDIRECT_APPEND));
        free_lexer_input(input);
    }

    it("should handle complex command") {
        LexerInput *input = init_lexer_input(NULL, "VAR=value cmd1 arg1 arg2 | cmd2 > out.txt 2>&1 & echo done", NULL, NULL);
        assertneq(input, NULL);
        assert(contains_token_type(input, TOKEN_ASSIGNMENT));
        assert(contains_token_type(input, TOKEN_PIPE));
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT));
        assert(contains_token_type(input, TOKEN_REDIRECT_OUT_ERR));
        assert(contains_token_type(input, TOKEN_BACKGROUND));
        free_lexer_input(input);
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
        free_session(session);
    }

    it("should handle whitespace") {
        LexerInput *input = init_lexer_input(NULL, "   echo   test   ", NULL, NULL);
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
    }
}
