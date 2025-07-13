#ifndef COMPILER_H
#define COMPILER_H

#include "../libs/mvm/mvm.h"
#include "ir.h"

Program compile_ir(Ir *ir);

#endif // COMPILER_H
