#ifndef GEN_H
#define GEN_H

#include "mvm.h"
#include "ops.h"

void program_gen_asm_x86_64(Program *program, StringBuilder *sb,
                            Ops *ops, OpGenProcs *op_gen_procs);

#endif // GEN_H
