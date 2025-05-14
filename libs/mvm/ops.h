#ifndef OPS_H
#define OPS_H

#include "mvm.h"
#include "ops/general.h"

Op *get_instr_op(ProcedureContext *proc_ctx, InstrOp *op_instr, Ops *ops);

Ops        get_ops_x86_64(void);
OpGenProcs get_op_gen_procs_x86_64(void);
Ops        get_ops_linux(void);
OpGenProcs get_op_gen_procs_linux(void);

#endif // OPS_H
