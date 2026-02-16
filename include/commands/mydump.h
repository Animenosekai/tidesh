#ifndef COMMAND_DUMP_H
#define COMMAND_DUMP_H

#include "session.h"

/*gère la conversion des adresses hexadécimales et la copie par blocs */
int builtin_mydump(int argc, char **argv, Session *session);
#endif

