#ifndef BUILTIN_BREAK_H
#define BUILTIN_BREAK_H

#include "session.h"

int builtin_break(int argc, char **argv, Session *session);
int builtin_continue(int argc, char **argv, Session *session);

#endif /* BUILTIN_BREAK_H */
