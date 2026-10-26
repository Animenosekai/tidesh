#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "builtin.h"
#include "builtins/context_manager.h"
#include "context.h"
#include "environ.h"
#include "execute.h"
#include "lexer.h"
#include "session.h"
#include "snow/snow.h"

describe(context_manager) {
    it("should identify context-manager and context as builtins and special builtins") {
        assert(is_builtin("context-manager"));
        assert(is_special_builtin("context-manager"));
        assertneq(get_builtin("context-manager"), NULL);

        assert(is_builtin("context"));
        assert(is_special_builtin("context"));
        assertneq(get_builtin("context"), NULL);
    }

    it("should register, list, and unregister custom context managers") {
        Session *session = init_session(NULL, NULL);

        char *reg_argv[] = {"context-manager", "register", "test_ctx",
                            "--enter", "echo entered",
                            "--exit", "echo exited"};
        int st = builtin_context_manager(7, reg_argv, session);
        asserteq(st, 0);

        CustomContextManager *mgr = get_custom_context_manager(session, "test_ctx");
        assertneq(mgr, NULL);
        asserteq_str(mgr->name, "test_ctx");
        asserteq_str(mgr->enter_cmd, "echo entered");
        asserteq_str(mgr->exit_cmd, "echo exited");

        // List managers
        char *ls_argv[] = {"context-manager", "list"};
        st = builtin_context_manager(2, ls_argv, session);
        asserteq(st, 0);

        // Cannot register with a built-in name
        char *bad_reg[] = {"context-manager", "register", "env", "--enter", "x"};
        st = builtin_context_manager(5, bad_reg, session);
        assertneq(st, 0);

        // Unregister
        char *unreg_argv[] = {"context-manager", "unregister", "test_ctx"};
        st = builtin_context_manager(3, unreg_argv, session);
        asserteq(st, 0);

        mgr = get_custom_context_manager(session, "test_ctx");
        asserteq(mgr, NULL);

        free_session(session);
        free(session);
    }

    it("should parse with statement in AST") {
        Session *session = init_session(NULL, NULL);

        const char *cmd = "with env(A_VARIABLE = \"Hello\"), cwd(\"/tmp\") {\n    echo $A_VARIABLE\n}";
        LexerInput *lexer = init_lexer_input(NULL, (char *)cmd, NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_WITH);
        assertneq(ast->with_contexts, NULL);
        asserteq_str(ast->with_contexts->name, "env");
        asserteq(ast->with_contexts->arg_count, 1);
        asserteq_str(ast->with_contexts->args[0].key, "A_VARIABLE");
        asserteq_str(ast->with_contexts->args[0].value, "Hello");

        assertneq(ast->with_contexts->next, NULL);
        asserteq_str(ast->with_contexts->next->name, "cwd");
        asserteq(ast->with_contexts->next->arg_count, 1);
        asserteq_str(ast->with_contexts->next->args[0].value, "/tmp");

        assertneq(ast->left, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should parse with statement using do done") {
        Session *session = init_session(NULL, NULL);

        const char *cmd = "with env(FOO=\"bar\"), cwd(\"/tmp\") do echo $FOO; done";
        LexerInput *lexer = init_lexer_input(NULL, (char *)cmd, NULL, session);
        ASTNode *ast = parse(lexer, session);

        assertneq(ast, NULL);
        asserteq(ast->type, NODE_WITH);
        assertneq(ast->with_contexts, NULL);
        asserteq_str(ast->with_contexts->name, "env");
        assertneq(ast->left, NULL);

        free_ast(ast);
        free(ast);
        free_lexer_input(lexer);
        free(lexer);
        free_session(session);
        free(session);
    }

    it("should execute with env context manager and restore original environment") {
        Session *session = init_session(NULL, NULL);

        environ_set(session->environ, "EXISTING_VAR", "initial");

        // Inside with block, EXISTING_VAR should be changed and NEW_VAR set
        int st = execute_string(
            "with env(EXISTING_VAR = \"changed\", NEW_VAR = \"hello\") {\n"
            "    IN_BLOCK_EXISTING=$EXISTING_VAR\n"
            "    IN_BLOCK_NEW=$NEW_VAR\n"
            "}\n", session);
        asserteq(st, 0);

        // Check values that were set inside the block
        char *in_exist = environ_get(session->environ, "IN_BLOCK_EXISTING");
        char *in_new = environ_get(session->environ, "IN_BLOCK_NEW");
        assertneq(in_exist, NULL);
        assertneq(in_new, NULL);
        asserteq_str(in_exist, "changed");
        asserteq_str(in_new, "hello");

        // After the block, EXISTING_VAR must be restored to "initial", and NEW_VAR must be unset!
        char *post_exist = environ_get(session->environ, "EXISTING_VAR");
        char *post_new = environ_get(session->environ, "NEW_VAR");
        assertneq(post_exist, NULL);
        asserteq_str(post_exist, "initial");
        asserteq(post_new, NULL);

        free_session(session);
        free(session);
    }

    it("should execute with cwd context manager and restore working directory") {
        Session *session = init_session(NULL, NULL);

        char initial_dir[4096];
        assertneq(getcwd(initial_dir, sizeof(initial_dir)), NULL);

        int st = execute_string(
            "with cwd(\"/tmp\") {\n"
            "    IN_BLOCK_PWD=$(pwd)\n"
            "}\n", session);
        asserteq(st, 0);

        char *in_pwd = environ_get(session->environ, "IN_BLOCK_PWD");
        assertneq(in_pwd, NULL);
        // /tmp might resolve to /private/tmp on macOS
        assert(strstr(in_pwd, "tmp") != NULL);

        // After the block, current directory must be restored
        char current_dir[4096];
        assertneq(getcwd(current_dir, sizeof(current_dir)), NULL);
        asserteq_str(current_dir, initial_dir);

        free_session(session);
        free(session);
    }

    it("should execute combined env and cwd context managers") {
        Session *session = init_session(NULL, NULL);

        char initial_dir[4096];
        assertneq(getcwd(initial_dir, sizeof(initial_dir)), NULL);

        int st = execute_string(
            "with env(MY_VAR = \"test_value\"), cwd(\"/tmp\") {\n"
            "    CAPTURED_VAR=$MY_VAR\n"
            "    CAPTURED_DIR=$(pwd)\n"
            "}\n", session);
        asserteq(st, 0);

        char *v = environ_get(session->environ, "CAPTURED_VAR");
        assertneq(v, NULL);
        asserteq_str(v, "test_value");

        // Outside with:
        asserteq(environ_get(session->environ, "MY_VAR"), NULL);
        char current_dir[4096];
        assertneq(getcwd(current_dir, sizeof(current_dir)), NULL);
        asserteq_str(current_dir, initial_dir);

        free_session(session);
        free(session);
    }

    it("should execute custom context manager registered with context-manager") {
        Session *session = init_session(NULL, NULL);

        // Register custom manager that echoes state on enter, and checks $1 (state) and $2 (status) on exit
        char *reg_argv[] = {"context-manager", "register", "tracker",
                            "--enter", "echo custom_token_123",
                            "--exit", "TRACKER_EXIT_TOKEN=\"$1\"; TRACKER_EXIT_STATUS=\"$2\""};
        builtin_context_manager(7, reg_argv, session);

        int st = execute_string(
            "with tracker(\"arg_val\") {\n"
            "    BODY_RAN=yes\n"
            "}\n", session);
        asserteq(st, 0);

        asserteq_str(environ_get(session->environ, "BODY_RAN"), "yes");
        asserteq_str(environ_get(session->environ, "TRACKER_EXIT_TOKEN"), "custom_token_123");
        asserteq_str(environ_get(session->environ, "TRACKER_EXIT_STATUS"), "0");

        free_session(session);
        free(session);
    }

    it("should unwind previous context managers if a later manager enter fails") {
        Session *session = init_session(NULL, NULL);

        environ_set(session->environ, "SAVED_FLAG", "original");

        // cwd to nonexistent directory will fail! env should be unwound, body should not run
        int st = execute_string(
            "with env(SAVED_FLAG = \"modified\"), cwd(\"/nonexistent_dir_tidesh_test_12345\") {\n"
            "    BODY_EXEC=ran\n"
            "}\n", session);
        assertneq(st, 0);

        // Body must NOT have executed
        asserteq(environ_get(session->environ, "BODY_EXEC"), NULL);

        // SAVED_FLAG must have been unwound back to original
        asserteq_str(environ_get(session->environ, "SAVED_FLAG"), "original");

        free_session(session);
        free(session);
    }

    it("should support nested with statements") {
        Session *session = init_session(NULL, NULL);

        int st = execute_string(
            "with env(LEVEL = \"1\") {\n"
            "    L1=$LEVEL\n"
            "    with env(LEVEL = \"2\") {\n"
            "        L2=$LEVEL\n"
            "    }\n"
            "    L1_AFTER=$LEVEL\n"
            "}\n", session);
        asserteq(st, 0);

        asserteq_str(environ_get(session->environ, "L1"), "1");
        asserteq_str(environ_get(session->environ, "L2"), "2");
        asserteq_str(environ_get(session->environ, "L1_AFTER"), "1");
        asserteq(environ_get(session->environ, "LEVEL"), NULL);

        free_session(session);
        free(session);
    }

    it("should execute session context manager and revert all session changes on exit") {
        Session *session = init_session(NULL, NULL);

        char initial_cwd[4096];
        char *cwd_res = getcwd(initial_cwd, sizeof(initial_cwd));
        assertneq(cwd_res, NULL);

        environ_set(session->environ, "PRE_EXISTING", "original_value");

        int st = execute_string(
            "with session {\n"
            "    cd /tmp\n"
            "    export PRE_EXISTING=\"overwritten\"\n"
            "    export NEW_SESSION_VAR=\"created_in_session\"\n"
            "    alias session_alias=\"echo test_alias\"\n"
            "    test_func() { echo \"func_result\"; }\n"
            "    set -x\n"
            "    IN_SESSION_VAR=$PRE_EXISTING\n"
            "    IN_SESSION_DIR=$(pwd)\n"
            "}\n", session);
        asserteq(st, 0);

        // Outside with session block:
        // Directory must be reverted to initial_cwd
        char curr_cwd[4096];
        getcwd(curr_cwd, sizeof(curr_cwd));
        asserteq_str(curr_cwd, initial_cwd);
        asserteq_str(session->current_working_dir, initial_cwd);

        // PRE_EXISTING must be reverted back to original_value
        asserteq_str(environ_get(session->environ, "PRE_EXISTING"), "original_value");

        // NEW_SESSION_VAR must be unset
        asserteq(environ_get(session->environ, "NEW_SESSION_VAR"), NULL);

        // IN_SESSION_VAR was set inside and thus also reverted with the session environment
        asserteq(environ_get(session->environ, "IN_SESSION_VAR"), NULL);

        // Alias must not exist in reverted session
        #ifndef TIDESH_DISABLE_ALIASES
        asserteq(trie_contains(session->aliases, "session_alias"), false);
        #endif

        // Function must not exist in reverted session
        #ifndef TIDESH_DISABLE_FUNCTIONS
        asserteq(trie_contains(session->functions, "test_func"), false);
        #endif

        // Option -x (xtrace) must be reverted to false
        asserteq(session->options.xtrace, false);

        free_session(session);
        free(session);
    }

    it("should allow changes from subcommands and functions within session block and revert on exit") {
        Session *session = init_session(NULL, NULL);

        char initial_cwd[4096];
        getcwd(initial_cwd, sizeof(initial_cwd));

        // Define a helper function before entering with session
        execute_string("change_state() { export SUB_VAR=\"sub_val\"; cd /tmp; }", session);

        int st = execute_string(
            "with session() {\n"
            "    change_state\n"
            "    CALL_CHECK=\"ran\"\n"
            "}\n", session);
        asserteq(st, 0);

        // After exit: directory reverted
        char curr_cwd[4096];
        getcwd(curr_cwd, sizeof(curr_cwd));
        asserteq_str(curr_cwd, initial_cwd);

        // Environment reverted
        asserteq(environ_get(session->environ, "SUB_VAR"), NULL);
        asserteq(environ_get(session->environ, "CALL_CHECK"), NULL);

        // The change_state function defined before with session is still present!
        #ifndef TIDESH_DISABLE_FUNCTIONS
        asserteq(trie_contains(session->functions, "change_state"), true);
        #endif

        free_session(session);
        free(session);
    }

    it("should handle tempdir context manager for isolated workspaces") {
        Session *session = init_session(NULL, NULL);

        char initial_cwd[4096];
        getcwd(initial_cwd, sizeof(initial_cwd));

        char captured_dir[4096] = {0};

        int st = execute_string(
            "with tempdir(var=\"ISOLATED_DIR\") {\n"
            "    echo \"hello sandbox\" > test_file.txt\n"
            "    cat test_file.txt\n"
            "}\n", session);
        asserteq(st, 0);

        // After exit, cwd should be restored
        char curr_cwd[4096];
        getcwd(curr_cwd, sizeof(curr_cwd));
        asserteq_str(curr_cwd, initial_cwd);

        // Environment variables TMPDIR, TEMP, TMP, SANDBOX_DIR, ISOLATED_DIR restored
        asserteq(environ_get(session->environ, "ISOLATED_DIR"), NULL);

        free_session(session);
        free(session);
    }

    it("should handle sandbox with readonly mode and write exceptions") {
        Session *session = init_session(NULL, NULL);

        // Normal write should succeed
        int st = execute_string("echo 'ok' > /tmp/tidesh_test_rw.txt", session);
        asserteq(st, 0);

        // Inside sandbox(readonly=true), write redirection should be denied
        st = execute_string(
            "with sandbox(readonly=true) {\n"
            "    echo 'should fail' > /tmp/tidesh_test_ro_fail.txt\n"
            "}\n", session);
        assertneq(st, 0);

        // Reading allowed inside sandbox(readonly=true)
        st = execute_string(
            "with sandbox(readonly=true) {\n"
            "    cat /tmp/tidesh_test_rw.txt\n"
            "}\n", session);
        asserteq(st, 0);

        // With writable exception
        st = execute_string(
            "with sandbox(readonly=true, rw=\"/tmp\") {\n"
            "    echo 'allowed' > /tmp/tidesh_test_allowed.txt\n"
            "}\n", session);
        asserteq(st, 0);

        unlink("/tmp/tidesh_test_rw.txt");
        unlink("/tmp/tidesh_test_ro_fail.txt");
        unlink("/tmp/tidesh_test_allowed.txt");

        free_session(session);
        free(session);
    }

    it("should handle sandbox with fine-grained ro, rw, and deny paths") {
        Session *session = init_session(NULL, NULL);

        // Test with sandbox: writing to /tmp allowed, writing to /etc blocked
        int st = execute_string(
            "with sandbox(rw=\"/tmp\", ro=\"/etc\", deny=\"/private_test\") {\n"
            "    echo 'in tmp' > /tmp/tidesh_sb_test.txt\n"
            "}\n", session);
        asserteq(st, 0);

        // Write to ro path denied
        st = execute_string(
            "with sandbox(rw=\"/tmp\", ro=\"/tmp/ro_dir\") {\n"
            "    echo 'blocked' > /tmp/ro_dir/fail.txt\n"
            "}\n", session);
        assertneq(st, 0);

        // Denied path blocked from reading
        st = execute_string(
            "with sandbox(deny=\"/tmp/denied_file.txt\") {\n"
            "    cat /tmp/denied_file.txt\n"
            "}\n", session);
        assertneq(st, 0);

        unlink("/tmp/tidesh_sb_test.txt");

        free_session(session);
        free(session);
    }

    it("should handle chroot context manager jailing paths with read and write access") {
        Session *session = init_session(NULL, NULL);

        char initial_cwd[4096];
        getcwd(initial_cwd, sizeof(initial_cwd));

        // Create a temporary directory to act as jail
        char jail_path[] = "/tmp/tidesh_jail_XXXXXX";
        char *created = mkdtemp(jail_path);
        assertneq(created, NULL);

        char chroot_cmd[1024];
        snprintf(chroot_cmd, sizeof(chroot_cmd),
                 "with chroot(\"%s\") {\n"
                 "    pwd\n"
                 "    echo 'in jail' > jail_file.txt\n"
                 "    cat jail_file.txt\n"
                 "}\n",
                 jail_path);

        int st = execute_string(chroot_cmd, session);
        asserteq(st, 0);

        // Verify file was created in jail
        char created_file[PATH_MAX];
        snprintf(created_file, sizeof(created_file), "%s/jail_file.txt", jail_path);
        asserteq(access(created_file, F_OK), 0);
        unlink(created_file);

        // After exiting chroot, cwd restored
        char curr_cwd[4096];
        getcwd(curr_cwd, sizeof(curr_cwd));
        asserteq_str(curr_cwd, initial_cwd);

        rmdir(jail_path);
        free_session(session);
        free(session);
    }
}
