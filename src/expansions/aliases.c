#include <stdlib.h> /* malloc, free */
#include <string.h> /* strdup */

#include "data/array.h"         /* init_array, array_add, Array */
#include "expansions/aliases.h" /* alias_expansion */
#include "lexer.h" /* LexerInput, init_lexer_input, lexer_next_token, TOKEN_EOF */
#include "session.h" /* Session, aliases_get */

Array *alias_expansion(char *input, Session *session) {
    char *alias_value = trie_get(session->aliases, input);
    if (!alias_value) {
        Array *results = init_array(NULL);
        array_add(results, input);
        return results;
    }

    Array *results = init_array(NULL);

    // Split alias_value into words using a temporary lexer
    // We pass NULL for the execute function as we don't want to perform
    // command substitution within the alias expansion itself (it will be
    // handled later by full_expansion if needed)
    LexerInput lexer_in = {0};
    init_lexer_input(&lexer_in, alias_value, NULL, session);

    LexerToken token;
    while ((token = lexer_next_token(&lexer_in)).type != TOKEN_EOF) {
        if (token.type == TOKEN_WORD) {
            array_add(results, token.value);
        } else if (token.type == TOKEN_ASSIGNMENT) {
            // Reconstruct assignment: VAR=VALUE
            size_t val_len   = strlen(token.value);
            size_t extra_len = token.extra ? strlen(token.extra) : 0;
            char  *full      = malloc(val_len + extra_len + 2);
            if (full) {
                strcpy(full, token.value);
                strcat(full, "=");
                if (token.extra) {
                    strcat(full, token.extra);
                }
                array_add(results, full);
                free(full);
            }
        }
        free_lexer_token(&token);
    }
    free_lexer_input(&lexer_in);

    // If for some reason we have no words (empty alias), return the input
    if (results->count == 0) {
        array_add(results, input);
    }

    return results;
}
