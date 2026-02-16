#ifndef COMMAND_MAPS_H
#define COMMAND_MAPS_H

#include "session.h"

/* Affiche les régions mémoire:  début/fin, permissions, offset, inode, pathname, annotations  comme [heap], [stack], ou marquage [*] suspicious ... */
 
int builtin_mymaps(int argc, char **argv, Session *session);

#endif