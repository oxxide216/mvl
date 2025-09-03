#ifndef INTRINSIC_H
#define INTRINSIC_H

#include "mvm/src/mvm.h"
#include "ir.h"

void proc_compile_bin_intrinsic(Procedure *proc, Str dest, Str op, IrArg arg0, IrArg arg1);
void proc_compile_un_intrinsic(Procedure *proc, Str dest, Str op, IrArg arg);
void proc_compile_pre_assign_intrinsic(Procedure *proc, Str dest, Str op, IrArg arg);

#endif // INTRINSIC_H
