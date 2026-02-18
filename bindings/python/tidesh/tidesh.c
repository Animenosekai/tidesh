/** tidesh.c
 *
 * The main source file for CFFI bindings to the tidesh shell.
 * This file includes all the necessary headers for the shell.
 *
 * Copyright (c) 2026 Animenosekai
 */

#include "ast.h"
#include "builtins/bg.h"
#include "builtins/cd.h"
#include "builtins/fg.h"
#include "builtins/info.h"
#include "builtins/jobs.h"
#include "builtins/which.h"
#include "data/array.h"
#include "data/trie.h"
#include "dirstack.h"
#include "environ.h"
#include "execute.h"
#include "expand.h"
#include "features.h"
#include "history.h"
#include "jobs.h"
#include "lexer.h"
#include "prompt/terminal.h"
#include "session.h"

const char *tidesh_compiler = __VERSION__;
