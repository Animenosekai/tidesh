#include <stdlib.h>
#include <string.h>

#include "data/array.h"
#include "execute.h"
#include "expansions/filenames.h"
#include "session.h"
#include "snow/snow.h"

describe(filename_expansion) {
    it("should handle NULL and non-glob literal inputs") {
        Session *session = init_session(NULL, NULL);

        Array *empty_res = filename_expansion(NULL, session);
        assertneq(empty_res, NULL);
        asserteq(empty_res->count, 0);
        free_array(empty_res);
        free(empty_res);

        Array *lit_res = filename_expansion("literal_word", session);
        assertneq(lit_res, NULL);
        asserteq(lit_res->count, 1);
        asserteq_str(lit_res->items[0], "literal_word");
        free_array(lit_res);
        free(lit_res);

        free_session(session);
        free(session);
    }

    it("should expand recursive glob patterns with ** by default") {
        Session *session = init_session(NULL, NULL);
        assert(!session->options.default_glob);

        // **/*.md matches README.md at depth 0, tests/README.md at depth 1,
        // etc.
        Array *res = filename_expansion("**/*.md", session);
        assertneq(res, NULL);
        // There are at least 5 markdown files in the repo (including README.md
        // at depth 0)
        assert(res->count >= 5);

        bool found_root_readme = false;
        bool found_nested_snow = false;
        for (size_t i = 0; i < res->count; i++) {
            if (strcmp(res->items[i], "README.md") == 0) {
                found_root_readme = true;
            }
            if (strstr(res->items[i], "tests/snow/README.md") != NULL) {
                found_nested_snow = true;
            }
        }
        assert(found_root_readme);
        assert(found_nested_snow);

        free_array(res);
        free(res);

        free_session(session);
        free(session);
    }

    it("should fallback to system glob when default_glob option is enabled") {
        Session *session              = init_session(NULL, NULL);
        session->options.default_glob = true;

        // With system glob, ** is treated as * (single directory component),
        // so **/*.md will NOT match root README.md (depth 0) and NOT
        // tests/snow/README.md (depth 2)
        Array *res = filename_expansion("**/*.md", session);
        assertneq(res, NULL);

        bool found_root_readme = false;
        bool found_nested_snow = false;
        for (size_t i = 0; i < res->count; i++) {
            if (strcmp(res->items[i], "README.md") == 0) {
                found_root_readme = true;
            }
            if (strstr(res->items[i], "tests/snow/README.md") != NULL) {
                found_nested_snow = true;
            }
        }
        assert(!found_root_readme);
        assert(!found_nested_snow);

        free_array(res);
        free(res);

        free_session(session);
        free(session);
    }

    it("should return unmatched glob pattern unchanged") {
        Session *session = init_session(NULL, NULL);

        Array *res =
            filename_expansion("nonexistent_dir/**/*.nonexistent_ext", session);
        assertneq(res, NULL);
        asserteq(res->count, 1);
        asserteq_str(res->items[0], "nonexistent_dir/**/*.nonexistent_ext");
        free_array(res);
        free(res);

        free_session(session);
        free(session);
    }

    it("should toggle default-glob dynamically with set -o and set +o") {
        Session *session = init_session(NULL, NULL);

        // By default, default-glob is false
        assert(!session->options.default_glob);

        execute_string("set -o default-glob", session);
        assert(session->options.default_glob);

        execute_string("set +o default-glob", session);
        assert(!session->options.default_glob);

        free_session(session);
        free(session);
    }
}
