#include <stdio.h> /* printf */

#include "builtins/pwd.h"
#include "session.h" /* Session */

int builtin_pwd(int argc, char **argv, Session *session) {
    printf("%s\n", session->current_working_dir);
    return 0;
}