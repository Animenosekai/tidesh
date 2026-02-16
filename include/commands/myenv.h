#ifndef COMMAND_ENV_H
#define COMMAND_ENV_H

#include "session.h"

/* Affiche les variables d'environnement d'un processus (myenv -p PID) */
int builtin_myenv(int argc, char **argv, Session *session);

#endif