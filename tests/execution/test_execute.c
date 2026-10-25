#include "execute.h"
#include "snow/snow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

describe(execute) {
    it("should find command in PATH") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Try to find common commands that should exist on most systems
        char *path = find_in_path("ls", session);
        // path may or may not be found depending on system
        // The important thing is the function doesn't crash

        if (path)
            free(path);
        free_session(session);
        free(session);
    }

    it("should handle nonexistent command in PATH") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *path = find_in_path("nonexistent_command_12345", session);
        asserteq(path, NULL);

        free_session(session);
        free(session);
    }

    it("should execute simple echo command") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(NULL, "echo test", NULL, session);
        ASTNode    *ast   = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            // Result should be 0 for success or non-zero for error
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle command execution in session") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Execute a command via string
        int result = execute_string("true", session);
        // true command should succeed
        assert(result >= 0);

        free_session(session);
        free(session);
    }

    it("should capture command stdout") {
        Session *session = init_session(NULL, "/tmp/test_history");

        char *output = execute_string_stdout("echo test", session);
        assertneq(output, NULL);
        assert(strstr(output, "test") != NULL);

        free(output);
        free_session(session);
        free(session);
    }

    it("should handle failing command") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int result = execute_string("false", session);
        // false command should return non-zero status
        assert(result != 0);

        free_session(session);
        free(session);
    }

    it("should handle execution with AST") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(NULL, "true", NULL, session);
        ASTNode    *ast   = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle pipe commands") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "echo hello | cat", NULL, session);
        ASTNode *ast = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle command with arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "echo hello world", NULL, session);
        ASTNode *ast = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle multiple sequential commands") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "true; false; true", NULL, session);
        ASTNode *ast = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle AND operator") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "true && false", NULL, session);
        ASTNode *ast = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result > 0); // Should fail since second command fails
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle OR operator") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer =
            init_lexer_input(NULL, "false || true", NULL, session);
        ASTNode *ast = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            assert(result == 0); // Should succeed since second command succeeds
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle redirections") {
        Session *session = init_session(NULL, "/tmp/test_history");

        LexerInput *lexer = init_lexer_input(
            NULL, "echo test > /tmp/test_output.txt", NULL, session);
        ASTNode *ast = parse(lexer, session);

        if (ast != NULL) {
            int result = execute(ast, session);
            // Command should execute (may succeed or fail depending on
            // permissions)
            assert(result >= 0);
            free_ast(ast);
            free(ast);
        }

        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should handle expansion in commands") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Set an environment variable
        environ_set(session->environ, "TEST_VAR", "value");

        // Try to execute a command with variable expansion
        int result = execute_string("echo test", session);
        assert(result >= 0);

        free_session(session);
        free(session);
    }

    it("should not leak caller positional parameters into shell functions with "
       "fewer arguments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Set caller positional parameters
        environ_set(session->environ, "0", "script.sh");
        environ_set(session->environ, "1", "outer1");
        environ_set(session->environ, "2", "outer2");
        environ_set(session->environ, "3", "outer3");

        // Define a function that outputs $1 and $2
        execute_string("fn() { echo \"$1-$2\"; }", session);

        // Call fn with only 1 argument ($2 should be empty, not 'outer2')
        char *output = execute_string_stdout("fn inner1", session);
        assertneq(output, NULL);
        assert(strstr(output, "inner1-") != NULL);
        assert(strstr(output, "outer2") == NULL);
        free(output);

        // Check that caller positional parameters are fully preserved
        asserteq_str(environ_get(session->environ, "0"), "script.sh");
        asserteq_str(environ_get(session->environ, "1"), "outer1");
        asserteq_str(environ_get(session->environ, "2"), "outer2");
        asserteq_str(environ_get(session->environ, "3"), "outer3");

        free_session(session);
        free(session);
    }

    it("should not leak function positional parameters back to caller") {
        Session *session = init_session(NULL, "/tmp/test_history");

        environ_set(session->environ, "0", "main");
        environ_set(session->environ, "1", "val1");

        // Define a function that takes 3 arguments
        execute_string("fn() { echo \"$1 $2 $3\"; }", session);

        char *output = execute_string_stdout("fn a b c", session);
        assertneq(output, NULL);
        assert(strstr(output, "a b c") != NULL);
        free(output);

        // Caller should still only have $0 and $1, while $2 and $3 must be
        // unset
        asserteq_str(environ_get(session->environ, "0"), "main");
        asserteq_str(environ_get(session->environ, "1"), "val1");
        asserteq(environ_get(session->environ, "2"), NULL);
        asserteq(environ_get(session->environ, "3"), NULL);

        free_session(session);
        free(session);
    }

    it("should properly isolate positional parameters in nested function "
       "calls") {
        Session *session = init_session(NULL, "/tmp/test_history");

        environ_set(session->environ, "0", "top");
        environ_set(session->environ, "1", "top_arg");

        execute_string("inner() { echo \"in:$1-$2\"; }", session);
        execute_string("outer() { echo \"out_pre:$1\"; inner \"in_arg\"; echo "
                       "\"out_post:$1-$2\"; }",
                       session);

        char *output =
            execute_string_stdout("outer \"out_arg1\" \"out_arg2\"", session);
        assertneq(output, NULL);
        assert(strstr(output, "out_pre:out_arg1") != NULL);
        assert(strstr(output, "in:in_arg-") != NULL);
        assert(strstr(output, "out_post:out_arg1-out_arg2") != NULL);
        free(output);

        asserteq_str(environ_get(session->environ, "0"), "top");
        asserteq_str(environ_get(session->environ, "1"), "top_arg");
        asserteq(environ_get(session->environ, "2"), NULL);

        free_session(session);
        free(session);
    }

    it("should have consistent precedence: special builtins take precedence "
       "over functions") {
        Session *session = init_session(NULL, "/tmp/test_history");
        environ_set(session->environ, "TEST_UNSET_VAR", "active");

        // Define a function shadowing a special builtin like unset
        execute_string("unset() { echo \"shadowed_unset\"; }", session);

        // get_command_info should report unset as COMMAND_SPECIAL_BUILTIN
        CommandInfo info = get_command_info("unset", session);
        asserteq(info.type, COMMAND_SPECIAL_BUILTIN);
        if (info.path)
            free(info.path);

        // execute should run the special builtin unset (unsetting the
        // variable), not the function
        execute_string("unset TEST_UNSET_VAR", session);
        asserteq(environ_get(session->environ, "TEST_UNSET_VAR"), NULL);

        free_session(session);
        free(session);
    }

    it("should have consistent precedence: functions take precedence over "
       "regular builtins") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Define a function shadowing a regular builtin (e.g. pwd)
        execute_string("pwd() { echo \"custom_pwd_output\"; }", session);

        // get_command_info should report pwd as COMMAND_FUNCTION
        CommandInfo info = get_command_info("pwd", session);
        asserteq(info.type, COMMAND_FUNCTION);
        if (info.path)
            free(info.path);

        // execute should run the user function
        char *output = execute_string_stdout("pwd", session);
        assertneq(output, NULL);
        assert(strstr(output, "custom_pwd_output") != NULL);
        free(output);

        // When functions feature is disabled, get_command_info should fallback
        // to COMMAND_BUILTIN
        session->features.functions = false;
        info                        = get_command_info("pwd", session);
        asserteq(info.type, COMMAND_BUILTIN);
        if (info.path)
            free(info.path);

        free_session(session);
        free(session);
    }

    it("should redirect output of special builtins without corrupting shell "
       "stdout") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_special_builtin_redir.txt";
        unlink(outfile);

        // type is a special builtin in tidesh
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "type pwd > %s", outfile);
        int st = execute_string(cmd, session);
        asserteq(st, 0);

        // Verify the file was created and contains the redirected output
        FILE *f = fopen(outfile, "r");
        assertneq(f, NULL);
        char  buf[256] = {0};
        char *read_str = fgets(buf, sizeof(buf), f);
        assertneq(read_str, NULL);
        assert(strstr(buf, "pwd is a shell built-in command") != NULL);
        fclose(f);
        unlink(outfile);

        // Verify normal stdout is not corrupted after redirection
        char *out = execute_string_stdout("echo post_test", session);
        assertneq(out, NULL);
        assert(strstr(out, "post_test") != NULL);
        free(out);

        free_session(session);
        free(session);
    }

    it("should redirect input and output of shell functions without corrupting "
       "shell fds") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_func_redir_out.txt";
        const char *infile  = "/tmp/test_func_redir_in.txt";
        unlink(outfile);
        unlink(infile);

        // Test output redirection on function
        execute_string("fn_out() { echo \"from_fn_out: $1\"; }", session);
        char cmd_out[256];
        snprintf(cmd_out, sizeof(cmd_out), "fn_out hello > %s", outfile);
        int st = execute_string(cmd_out, session);
        asserteq(st, 0);

        FILE *f_out = fopen(outfile, "r");
        assertneq(f_out, NULL);
        char buf_out[256] = {0};
        assertneq(fgets(buf_out, sizeof(buf_out), f_out), NULL);
        assert(strstr(buf_out, "from_fn_out: hello") != NULL);
        fclose(f_out);
        unlink(outfile);

        // Test input redirection on function
        FILE *f_in = fopen(infile, "w");
        assertneq(f_in, NULL);
        fputs("hello_from_input_file\n", f_in);
        fclose(f_in);

        execute_string("fn_in() { cat; }", session);
        char cmd_in[256];
        snprintf(cmd_in, sizeof(cmd_in), "fn_in < %s", infile);
        char *out = execute_string_stdout(cmd_in, session);
        assertneq(out, NULL);
        assert(strstr(out, "hello_from_input_file") != NULL);
        free(out);
        unlink(infile);

        // Verify stdout and stdin are intact
        out = execute_string_stdout("echo intact", session);
        assertneq(out, NULL);
        assert(strstr(out, "intact") != NULL);
        free(out);

        free_session(session);
        free(session);
    }

    it("should properly update function and free old AST node when redefining "
       "a function") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Define function version 1
        int st1 = execute_string("redef_fn() { echo \"version 1\"; }", session);
        asserteq(st1, 0);

        char *out1 = execute_string_stdout("redef_fn", session);
        assertneq(out1, NULL);
        assert(strstr(out1, "version 1") != NULL);
        free(out1);

        // Redefine function version 2 (should free version 1 AST node without
        // leak)
        int st2 = execute_string("redef_fn() { echo \"version 2\"; }", session);
        asserteq(st2, 0);

        char *out2 = execute_string_stdout("redef_fn", session);
        assertneq(out2, NULL);
        assert(strstr(out2, "version 2") != NULL);
        assert(strstr(out2, "version 1") == NULL);
        free(out2);

        // Redefine function version 3 with arguments
        int st3 =
            execute_string("redef_fn() { echo \"version 3: $1\"; }", session);
        asserteq(st3, 0);

        char *out3 = execute_string_stdout("redef_fn argval", session);
        assertneq(out3, NULL);
        assert(strstr(out3, "version 3: argval") != NULL);
        free(out3);

        free_session(session);
        free(session);
    }

    it("should expand variables in variable assignments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int st1 = execute_string(
            "BASE=/opt/app; FULL=\"$BASE/bin\"; PATH=\"$FULL:$PATH\"", session);
        asserteq(st1, 0);

        char *val = environ_get(session->environ, "FULL");
        assertneq(val, NULL);
        asserteq_str(val, "/opt/app/bin");

        char *path = environ_get(session->environ, "PATH");
        assertneq(path, NULL);
        assert(strncmp(path, "/opt/app/bin:", 13) == 0);

        free_session(session);
        free(session);
    }

    it("should populate special positional parameters $#, $@, $* in "
       "functions") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int def_st = execute_string(
            "test_params() { echo \"count:$# args:$@ star:$*\"; }", session);
        asserteq(def_st, 0);

        char *out = execute_string_stdout("test_params one two three", session);
        assertneq(out, NULL);
        assert(strstr(out, "count:3") != NULL);
        assert(strstr(out, "args:one two three") != NULL);
        assert(strstr(out, "star:one two three") != NULL);
        free(out);

        free_session(session);
        free(session);
    }

    it("should expand nested variables in parameter defaults") {
        Session *session = init_session(NULL, "/tmp/test_history");

        environ_set(session->environ, "FALLBACK", "default_val");
        char *out =
            execute_string_stdout("echo \"${UNSET_VAR:-$FALLBACK}\"", session);
        assertneq(out, NULL);
        assert(strstr(out, "default_val") != NULL);
        free(out);

        free_session(session);
        free(session);
    }

    it("should expand variable names starting with an underscore without "
       "confusing with $_") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int st = execute_string("_MY_VAR=\"hello_under\"; VAL=\"$_MY_VAR\"",
                                session);
        asserteq(st, 0);

        char *val = environ_get(session->environ, "VAL");
        assertneq(val, NULL);
        asserteq_str(val, "hello_under");

        free_session(session);
        free(session);
    }

    it("should not evaluate command substitution in false branch of "
       "conditional") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int st = execute_string(
            "FOO=initial; if false; then FOO=$(echo changed); fi", session);
        asserteq(st, 0);

        char *val = environ_get(session->environ, "FOO");
        assertneq(val, NULL);
        asserteq_str(val, "initial");

        free_session(session);
        free(session);
    }

    it("should evaluate command substitution when branch is taken") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int st = execute_string(
            "FOO=initial; if true; then FOO=$(echo changed); fi", session);
        asserteq(st, 0);

        char *val = environ_get(session->environ, "FOO");
        assertneq(val, NULL);
        asserteq_str(val, "changed");

        free_session(session);
        free(session);
    }

    it("should not evaluate command substitution on right side of "
       "short-circuited &&") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int st = execute_string("FOO=initial; false && FOO=$(echo changed)",
                                session);
        assertneq(st, 0);

        char *val = environ_get(session->environ, "FOO");
        assertneq(val, NULL);
        asserteq_str(val, "initial");

        free_session(session);
        free(session);
    }

    it("should evaluate command substitution in variable assignments") {
        Session *session = init_session(NULL, "/tmp/test_history");

        int st = execute_string("BAR=$(echo hello_world)", session);
        asserteq(st, 0);

        char *val = environ_get(session->environ, "BAR");
        assertneq(val, NULL);
        asserteq_str(val, "hello_world");

        free_session(session);
        free(session);
    }

    it("should redirect both stdout and stderr with &>") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_redir_amp_gt.txt";
        unlink(outfile);

        int st = execute_string("echo out_msg &> /tmp/test_redir_amp_gt.txt",
                                session);
        asserteq(st, 0);

        FILE *f = fopen(outfile, "r");
        assertneq(f, NULL);
        char buf[256] = {0};
        assertneq(fgets(buf, sizeof(buf), f), NULL);
        assert(strstr(buf, "out_msg") != NULL);
        fclose(f);
        unlink(outfile);

        free_session(session);
        free(session);
    }

    it("should append both stdout and stderr with &>>") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_redir_amp_append.txt";
        unlink(outfile);

        execute_string("echo first_line > /tmp/test_redir_amp_append.txt",
                       session);
        execute_string("echo second_line &>> /tmp/test_redir_amp_append.txt",
                       session);

        FILE *f = fopen(outfile, "r");
        assertneq(f, NULL);
        char   buf[512] = {0};
        size_t n        = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n]          = '\0';
        assert(strstr(buf, "first_line") != NULL);
        assert(strstr(buf, "second_line") != NULL);
        fclose(f);
        unlink(outfile);

        free_session(session);
        free(session);
    }

    it("should duplicate file descriptors with 2>&1") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_redir_dup2.txt";
        unlink(outfile);

        int st = execute_string(
            "echo stdout_msg > /tmp/test_redir_dup2.txt 2>&1", session);
        asserteq(st, 0);

        FILE *f = fopen(outfile, "r");
        assertneq(f, NULL);
        char buf[256] = {0};
        assertneq(fgets(buf, sizeof(buf), f), NULL);
        assert(strstr(buf, "stdout_msg") != NULL);
        fclose(f);
        unlink(outfile);

        free_session(session);
        free(session);
    }

    it("should iterate over words in for loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st      = execute_string(
            "ACC=\"\"; for x in a b c; do ACC=\"${ACC}$x\"; done", session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "abc");
        char *last_x = environ_get(session->environ, "x");
        assertneq(last_x, NULL);
        asserteq_str(last_x, "c");

        free_session(session);
        free(session);
    }

    it("should support multi-line formatting in for loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st      = execute_string(
            "ACC=\"\"\nfor item in 1 2 3\ndo\nACC=\"${ACC}${item}\"\ndone",
            session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "123");

        free_session(session);
        free(session);
    }

    it("should expand braces, variables and command substitution in for loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st1     = execute_string(
            "ACC=\"\"; for num in {1..3}; do ACC=\"${ACC}$num\"; done",
            session);
        asserteq(st1, 0);
        char *val1 = environ_get(session->environ, "ACC");
        assertneq(val1, NULL);
        asserteq_str(val1, "123");

        int st2 = execute_string("OUT=\"\"; for w in $(echo alpha) $(echo "
                                 "beta); do OUT=\"${OUT}:${w}\"; done",
                                 session);
        asserteq(st2, 0);
        char *val2 = environ_get(session->environ, "OUT");
        assertneq(val2, NULL);
        asserteq_str(val2, ":alpha:beta");

        free_session(session);
        free(session);
    }

    it("should handle empty in list in for loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        environ_set(session->environ, "VAR", "untouched");
        int st =
            execute_string("for VAR in; do VAR=\"changed\"; done", session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "VAR");
        assertneq(val, NULL);
        asserteq_str(val, "untouched");

        free_session(session);
        free(session);
    }

    it("should iterate over positional parameters when in is omitted") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      def = execute_string("test_for_pos() { RES=\"\"; for arg; do "
                                 "RES=\"${RES}${arg}.\"; done; }",
                                 session);
        asserteq(def, 0);
        int run = execute_string("test_for_pos one two three", session);
        asserteq(run, 0);
        char *res = environ_get(session->environ, "RES");
        assertneq(res, NULL);
        asserteq_str(res, "one.two.three.");

        free_session(session);
        free(session);
    }

    it("should support break in for loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st =
            execute_string("ACC=\"\"; for x in 1 2 3 4 5; do if test \"$x\" = "
                           "\"3\"; then break; fi; ACC=\"${ACC}$x\"; done",
                           session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "12");

        free_session(session);
        free(session);
    }

    it("should support continue in for loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st =
            execute_string("ACC=\"\"; for x in 1 2 3 4 5; do if test \"$x\" = "
                           "\"3\"; then continue; fi; ACC=\"${ACC}$x\"; done",
                           session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "1245");

        free_session(session);
        free(session);
    }

    it("should support nested for loops") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int st = execute_string("ACC=\"\"; for i in 1 2; do for j in a b; do "
                                "ACC=\"${ACC}$i$j,\"; done; done",
                                session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "1a,1b,2a,2b,");

        free_session(session);
        free(session);
    }

    it("should support redirections on for loop") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_for_redir.txt";
        unlink(outfile);

        int st = execute_string("for i in x y z; do echo \"item_$i\"; done > "
                                "/tmp/test_for_redir.txt",
                                session);
        asserteq(st, 0);

        FILE *f = fopen(outfile, "r");
        assertneq(f, NULL);
        char   buf[256] = {0};
        size_t n        = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n]          = '\0';
        assert(strstr(buf, "item_x") != NULL);
        assert(strstr(buf, "item_y") != NULL);
        assert(strstr(buf, "item_z") != NULL);
        fclose(f);
        unlink(outfile);

        free_session(session);
        free(session);
    }

    it("should support for loop in pipeline") {
        Session *session = init_session(NULL, "/tmp/test_history");
        char    *out     = execute_string_stdout(
            "for item in red green blue; do echo $item; done | grep green",
            session);
        assertneq(out, NULL);
        assert(strstr(out, "green") != NULL);
        assert(strstr(out, "red") == NULL);
        free(out);

        free_session(session);
        free(session);
    }

    it("should support multi-level break in nested loops") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st      = execute_string(
            "ACC=\"\"; for i in 1 2 3; do for j in a b c; do if test \"$j\" = "
            "\"b\"; then break 2; fi; ACC=\"${ACC}$i$j,\"; done; done",
            session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "1a,");

        free_session(session);
        free(session);
    }

    it("should support multi-level continue in nested loops") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st      = execute_string(
            "ACC=\"\"; for i in 1 2; do for j in a b; do if test \"$j\" = "
            "\"b\"; then continue 2; fi; ACC=\"${ACC}$i$j,\"; done; done",
            session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "1a,2a,");

        free_session(session);
        free(session);
    }

    it("should execute basic while loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st =
            execute_string("ACC=\"\"; I=\"\"; while test \"$I\" != \"aaa\"; do "
                           "I=\"${I}a\"; ACC=\"${ACC}$I,\"; done",
                           session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "a,aa,aaa,");

        free_session(session);
        free(session);
    }

    it("should not execute while loop body when condition is false initially") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st      = execute_string(
            "ACC=\"untouched\"; while false; do ACC=\"changed\"; done",
            session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "untouched");

        free_session(session);
        free(session);
    }

    it("should support multi-line formatting in while loop") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *script  = "ACC=\"init\"\n"
                              "X=\"start\"\n"
                              "while test \"$X\" = \"start\"\n"
                              "do\n"
                              "    ACC=\"done\"\n"
                              "    X=\"stop\"\n"
                              "done\n";
        int         st      = execute_string(script, session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "done");

        free_session(session);
        free(session);
    }

    it("should support break and continue in while loop") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st      = execute_string(
            "ACC=\"\"; I=\"\"; while test \"$I\" != \"aaaaa\"; do I=\"${I}a\"; "
            "if test \"$I\" = \"aa\"; then continue; fi; if test \"$I\" = "
            "\"aaaa\"; then break; fi; ACC=\"${ACC}$I,\"; done",
            session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "a,aaa,");

        free_session(session);
        free(session);
    }

    it("should support nested while loops and multi-level break") {
        Session *session = init_session(NULL, "/tmp/test_history");
        int      st =
            execute_string("ACC=\"\"; I=\"\"; while test \"$I\" != \"aa\"; do "
                           "I=\"${I}a\"; J=\"\"; while test \"$J\" != \"bb\"; "
                           "do J=\"${J}b\"; if test \"$I\" = \"aa\"; then "
                           "break 2; fi; ACC=\"${ACC}$I-$J,\"; done; done",
                           session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "a-b,a-bb,");

        free_session(session);
        free(session);
    }

    it("should support redirections on while loop") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_while_redir.txt";
        unlink(outfile);

        int st = execute_string(
            "I=\"\"; while test \"$I\" != \"aaa\"; do I=\"${I}a\"; echo "
            "\"run_$I\"; done > /tmp/test_while_redir.txt",
            session);
        asserteq(st, 0);

        FILE *f = fopen(outfile, "r");
        assertneq(f, NULL);
        char   buf[256] = {0};
        size_t n        = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n]          = '\0';
        assert(strstr(buf, "run_a") != NULL);
        assert(strstr(buf, "run_aa") != NULL);
        assert(strstr(buf, "run_aaa") != NULL);
        fclose(f);
        unlink(outfile);

        free_session(session);
        free(session);
    }

    it("should support while loop in pipeline") {
        Session *session = init_session(NULL, "/tmp/test_history");
        char    *out     = execute_string_stdout(
            "I=\"\"; while test \"$I\" != \"aaa\"; do I=\"${I}a\"; echo "
            "\"val_$I\"; done | grep val_aa",
            session);
        assertneq(out, NULL);
        assert(strstr(out, "val_aa") != NULL);
        free(out);

        free_session(session);
        free(session);
    }

    it("should execute case statement with pattern matching") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Exact match
        int st =
            execute_string("X=\"apple\"; RESULT=\"\"; case \"$X\" in apple) "
                           "RESULT=\"fruit\" ;; *) RESULT=\"other\" ;; esac",
                           session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "RESULT");
        assertneq(val, NULL);
        asserteq_str(val, "fruit");

        // Multiple patterns with |
        st = execute_string(
            "X=\"cherry\"; RESULT=\"\"; case \"$X\" in apple|banana|cherry) "
            "RESULT=\"matched_list\" ;; *) RESULT=\"other\" ;; esac",
            session);
        asserteq(st, 0);
        val = environ_get(session->environ, "RESULT");
        assertneq(val, NULL);
        asserteq_str(val, "matched_list");

        // Wildcard match *
        st = execute_string(
            "X=\"archive.tar.gz\"; RESULT=\"\"; case \"$X\" in *.tar.gz) "
            "RESULT=\"archive\" ;; *) RESULT=\"other\" ;; esac",
            session);
        asserteq(st, 0);
        val = environ_get(session->environ, "RESULT");
        assertneq(val, NULL);
        asserteq_str(val, "archive");

        // Wildcard match ?
        st = execute_string(
            "X=\"b\"; RESULT=\"\"; case \"$X\" in ?) RESULT=\"single_char\" ;; "
            "*) RESULT=\"multi\" ;; esac",
            session);
        asserteq(st, 0);
        val = environ_get(session->environ, "RESULT");
        assertneq(val, NULL);
        asserteq_str(val, "single_char");

        // Character class [...]
        st = execute_string(
            "X=\"5\"; RESULT=\"\"; case \"$X\" in [0-9]) RESULT=\"digit\" ;; "
            "*) RESULT=\"non_digit\" ;; esac",
            session);
        asserteq(st, 0);
        val = environ_get(session->environ, "RESULT");
        assertneq(val, NULL);
        asserteq_str(val, "digit");

        // Default match *)
        st = execute_string(
            "X=\"zebra\"; RESULT=\"\"; case \"$X\" in apple) RESULT=\"fruit\" "
            ";; *) RESULT=\"default_hit\" ;; esac",
            session);
        asserteq(st, 0);
        val = environ_get(session->environ, "RESULT");
        assertneq(val, NULL);
        asserteq_str(val, "default_hit");

        // No match returns 0
        st = execute_string("X=\"nomatch\"; RESULT=\"unchanged\"; case \"$X\" "
                            "in apple) RESULT=\"fruit\" ;; esac",
                            session);
        asserteq(st, 0);
        val = environ_get(session->environ, "RESULT");
        assertneq(val, NULL);
        asserteq_str(val, "unchanged");

        free_session(session);
        free(session);
    }

    it("should support fallthrough and continue matching in case") {
        Session *session = init_session(NULL, "/tmp/test_history");

        // Fallthrough with ;&
        int st =
            execute_string("ACC=\"\"; case \"first\" in first) "
                           "ACC=\"${ACC}1,\";& second) ACC=\"${ACC}2,\";; esac",
                           session);
        asserteq(st, 0);
        char *val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "1,2,");

        // Continue matching with ;;&
        st = execute_string(
            "ACC=\"\"; case \"alpha_beta\" in a*) ACC=\"${ACC}starts_a,\";;& "
            "*_beta) ACC=\"${ACC}ends_beta,\";;& *gamma) "
            "ACC=\"${ACC}gamma,\";; esac",
            session);
        asserteq(st, 0);
        val = environ_get(session->environ, "ACC");
        assertneq(val, NULL);
        asserteq_str(val, "starts_a,ends_beta,");

        free_session(session);
        free(session);
    }

    it("should support redirections and pipelines with case statements") {
        Session    *session = init_session(NULL, "/tmp/test_history");
        const char *outfile = "/tmp/test_case_redir.txt";
        unlink(outfile);

        int st = execute_string("case \"hello\" in hello) echo \"case_output\" "
                                ";; esac > /tmp/test_case_redir.txt",
                                session);
        asserteq(st, 0);

        FILE *f = fopen(outfile, "r");
        assertneq(f, NULL);
        char   buf[256] = {0};
        size_t n        = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n]          = '\0';
        assert(strstr(buf, "case_output") != NULL);
        fclose(f);
        unlink(outfile);

        char *out =
            execute_string_stdout("case \"my_value\" in my_*) echo "
                                  "\"matched_my_val\" ;; esac | grep val",
                                  session);
        assertneq(out, NULL);
        assert(strstr(out, "matched_my_val") != NULL);
        free(out);

        free_session(session);
        free(session);
    }
}
