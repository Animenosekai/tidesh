#include <stdlib.h>
#include "builtin.h"
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
}
