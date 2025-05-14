#ifndef COMPILER_H
#define COMPILER_H

#include "../libs/mvm/mvm.h"
#include "lexer.h"

void compile(Tokens tokens, Program *program);

#endif // COMPILER_H
