#include <string.h>

#include "opt.h"
#include "misc.h"
#include "mvm.h"
#include "ops.h"

void proc_opt_tail_recursion(Procedure *proc) {
  bool added_begin_label = false;

  Instr *instr = proc->instrs;
  while (instr) {
    if (instr->kind != InstrKindCall) {
      instr = instr->next;
      continue;
    }

    if (!str_eq(instr->as.call.callee_name, proc->name)) {
      instr = instr->next;
      continue;
    }

    Str begin_label_name = STR_LIT(".begin");

    if (!added_begin_label &&
        (!instr->next || instr->next->kind == InstrKindReturn ||
         instr->next->kind == InstrKindReturnValue)) {
        LL_APPEND(proc->instrs, Instr);
        proc->instrs->kind = InstrKindLabel;
        proc->instrs->as.label = (InstrLabel) { begin_label_name };
        proc->instrs->next->prev = proc->instrs;

        added_begin_label = true;
    }

    instr->kind = InstrKindJump;
    instr->as.jump = (InstrJump) { begin_label_name, NULL, true };

    instr = instr->next;
  }
}

static Instr *get_last_var_def(Instr *instr, Str var_name) {
  if (!instr)
    return NULL;

  instr = instr->prev;
  while (instr) {
    if (instr->kind == InstrKindOp &&
        str_eq(instr->as.op.dest, var_name))
      return instr;

    if (instr->kind == InstrKindLabel)
      return NULL;

    instr = instr->prev;
  }

  return NULL;
}

static bool op_can_be_inlined(Op *op) {
  return op->arity == 1 &&
         op->can_be_inlined &&
         op->args[0].cond == ArgConditionAny;
}

void proc_opt_inline_args(Procedure *proc, Ops ops) {
  Instr *instr = proc->instrs_end;
  while (instr) {
    if (instr->kind == InstrKindOp) {
      Op *op = get_instr_op(proc->ctx, &instr->as.op, &ops);
      for (u32 i = 0; i < instr->as.op.args.len; ++i) {
        if (op->args[i].cond != ArgConditionAny)
          continue;

        Arg *arg = instr->as.op.args.items + i;
        if (arg->kind != ArgKindVar)
          continue;

        Instr *last_def = get_last_var_def(instr, arg->as.var);
        if (!last_def)
          continue;

        Op *last_def_op = get_instr_op(proc->ctx, &last_def->as.op, &ops);
        if (!op_can_be_inlined(last_def_op))
          continue;

        Arg *last_def_first_arg = last_def->as.op.args.items + 0;
        if (last_def_first_arg->kind == ArgKindVar)
          continue;

        *arg = *last_def_first_arg;

        InstrData *last_def_data = get_instr_data(proc->ctx, last_def);
        --last_def_data->dest_var->used.len;
      }
    }

    instr = instr->prev;
  }
}

static bool instr_can_be_deleted(ProcedureContext *proc_ctx, Instr *instr,
                                 InstrData *data, Ops *ops) {
  if (instr->kind != InstrKindOp)
    return false;

  if (data->dest_var) {
    if (data->dest_var->used.len > 0)
      return false;
  } else {
    return false;
  }

  Op *op = get_instr_op(proc_ctx, &instr->as.op, ops);
  for (u32 i = 0; i < op->arity; ++i)
    if (op->args[i].cond != ArgConditionAny)
      return false;

  return true;
}

void proc_opt_remove_unused_var_defs(Procedure *proc, Ops ops) {
  Instr *instr = proc->instrs;
  while (instr) {
    InstrData *data = get_instr_data(proc->ctx, instr);
    instr->removed = instr_can_be_deleted(proc->ctx, instr, data, &ops);

    instr = instr->next;
  }
}
