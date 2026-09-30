#include <stdlib.h>
#include <unistd.h>
#include "builtin.h"
#include "builtins/cd.h"
#include "builtins/unalias.h"
#include "data/trie.h"
#include "session.h"
#include "snow/snow.h"

describe(builtin) {
    it("should find builtin commands") {
        assert(is_builtin("cd"));
        assert(is_builtin("pwd"));
        assert(is_builtin("exit"));
        assert(is_builtin("echo") == false);
        assert(is_builtin("nonexistent") == false);
    }

    it("should identify special builtins") {
        assert(is_special_builtin("cd"));
        assert(is_special_builtin("exit"));
        assert(is_special_builtin("export"));
        assert(is_special_builtin("eval"));
        assert(is_special_builtin("source"));
        assert(is_special_builtin("."));
        assert(is_special_builtin("pwd") == false);
        assert(is_special_builtin("help") == false);
    }

    it("should get builtin function pointers") {
        int (*cd_func)(int, char **, Session *) = get_builtin("cd");
        assertneq(cd_func, NULL);
        
        int (*pwd_func)(int, char **, Session *) = get_builtin("pwd");
        assertneq(pwd_func, NULL);
        
        int (*nonexistent)(int, char **, Session *) = get_builtin("nonexistent");
        asserteq(nonexistent, NULL);
    }

    it("should unalias existing aliases") {
        Session *session = init_session(NULL, "/tmp/test_history");
        trie_set(session->aliases, "test_alias", "echo 123");
        assertneq(trie_get(session->aliases, "test_alias"), NULL);

        char *argv[] = {"unalias", "test_alias"};
        int res = builtin_unalias(2, argv, session);
        asserteq(res, 0);
        asserteq(trie_get(session->aliases, "test_alias"), NULL);

        // Calling unalias on non-existent alias should return non-zero
        int res_nonexistent = builtin_unalias(2, argv, session);
        asserteq(res_nonexistent, 1);

        free_session(session);
        free(session);
    }

    it("should allow cd even when directory_stack is disabled") {
        Session *session = init_session(NULL, NULL);
        session->features.directory_stack = false;

        char original_cwd[1024];
        assertneq(getcwd(original_cwd, sizeof(original_cwd)), NULL);

        char *argv[] = {"cd", "/tmp"};
        int res = builtin_cd(2, argv, session);
        asserteq(res, 0);

        char *restore_argv[] = {"cd", original_cwd};
        res = builtin_cd(2, restore_argv, session);
        asserteq(res, 0);

        free_session(session);
        free(session);
    }
}

