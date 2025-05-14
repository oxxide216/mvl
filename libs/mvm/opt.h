#ifndef OPT_H
#define OPT_H

#include "mvm.h"
#include "proc-ctx.h"

void proc_opt_tail_recursion(Procedure *proc);
void proc_opt_inline_args(Procedure *proc, Ops ops);
void proc_opt_remove_unused_var_defs(Procedure *proc, Ops ops);

#endif // OPT_H
