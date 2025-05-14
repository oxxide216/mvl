#include "shl_log.h"
#include "ops.h"
#include "misc.h"

static bool instr_op_eq(ProcedureContext *proc_ctx, InstrOp *op_instr, Op *op) {
  if (!str_eq(op_instr->name, op->name))
    return false;

  if (op_instr->args.len != op->arity)
    return false;

  for (u32 i = 0; i < op->arity; ++i) {
    ValueKind op_instr_arg_kind = get_arg_kind(proc_ctx, &op_instr->args.items[i]);
    if (op_instr_arg_kind != op->args[i].kind)
      return false;
  }

  return true;
}

Op *get_instr_op(ProcedureContext *proc_ctx, InstrOp *op_instr, Ops *ops) {
  for (u32 i = 0; i < ops->len; ++i)
    if (instr_op_eq(proc_ctx, op_instr, ops->items + i))
      return ops->items + i;

  ERROR("Operation `"STR_FMT"` with such signature was not found\n",
        STR_ARG(op_instr->name));
  exit(1);
}
