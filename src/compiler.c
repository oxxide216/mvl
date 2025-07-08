#include "compiler.h"
#include "shl_log.h"

typedef struct {
  Str end_label;
} Block;

typedef Da(Block) Blocks;

typedef struct {
  Program program;
  Blocks  blocks;
} Compiler;

static ValueKind type_kinds_value_kinds_table[TypeKindsCount] = {
  [TypeKindUnit] = ValueKindUnit,
  [TypeKindS64] = ValueKindS64,
  [TypeKindS32] = ValueKindS32,
  [TypeKindS16] = ValueKindS16,
  [TypeKindS8] = ValueKindS8,
  [TypeKindU64] = ValueKindU64,
  [TypeKindU32] = ValueKindU32,
  [TypeKindU16] = ValueKindU16,
  [TypeKindU8] = ValueKindU8,
  [TypeKindPtr] = ValueKindS64,
};

static Value ir_arg_value_to_value(IrArgValue *ir_arg_value) {
  switch (ir_arg_value->type->kind) {
  case TypeKindUnit: return (Value) { ValueKindUnit, {0} };
  case TypeKindS64:  return (Value) { ValueKindS64, { .s64 = ir_arg_value->as._s64 } };
  case TypeKindS32:  return (Value) { ValueKindS32, { .s32 = ir_arg_value->as._s32 } };
  case TypeKindS16:  return (Value) { ValueKindS16, { .s16 = ir_arg_value->as._s16 } };
  case TypeKindS8:   return (Value) { ValueKindS8, { .s8 = ir_arg_value->as._s8 } };
  case TypeKindU64:  return (Value) { ValueKindU64, { .u64 = ir_arg_value->as._u64 } };
  case TypeKindU32:  return (Value) { ValueKindU32, { .u32 = ir_arg_value->as._u32 } };
  case TypeKindU16:  return (Value) { ValueKindU16, { .u16 = ir_arg_value->as._u16 } };
  case TypeKindU8:   return (Value) { ValueKindU8, { .u8 = ir_arg_value->as._s8 } };
  case TypeKindPtr:  return (Value) { ValueKindS64, { .s64 = ir_arg_value->as._s64 } };

  default: {
    ERROR("Wrong type kind\n");
    exit(1);
  }
  }
}

static Arg ir_arg_to_arg(IrArg *ir_arg) {
  if (ir_arg->kind == IrArgKindValue)
    return arg_value(ir_arg_value_to_value(&ir_arg->as.value));
  else if (ir_arg->kind == IrArgKindVar)
    return arg_var(ir_arg->as.var);

  ERROR("Wrong IR arg kind\n");
  exit(1);
}

static void compile_ir_instrs(Procedure *proc, IrInstrs *ir_instrs) {
  for (u32 i = 0; i < ir_instrs->len; ++i) {
    IrInstr *ir_instr = ir_instrs->items + i;

    switch (ir_instr->kind) {
    case IrInstrKindAssign: {
      Arg arg = ir_arg_to_arg(&ir_instr->as.assign.arg);
      proc_assign(proc, ir_instr->as.assign.dest, arg);
    } break;

    case IrInstrKindIf: {
      RelOp rel_op = ir_instr->as._if.rel_op;
      Arg arg0 = ir_arg_to_arg(&ir_instr->as._if.arg0);
      Arg arg1 = ir_arg_to_arg(&ir_instr->as._if.arg1);
      Str label_name = ir_instr->as._if.label_name;

      proc_cond_jump(proc, rel_op, arg0, arg1, label_name);
    } break;

    case IrInstrKindLabel: {
      proc_add_label(proc, ir_instr->as.label.name);
    } break;

    case IrInstrKindRetVal: {
      Arg arg = ir_arg_to_arg(&ir_instr->as.ret_val.arg);
      proc_return_value(proc, arg);
    } break;

    default: {
      ERROR("Wrong IR instr kind\n");
      exit(1);
    }
    }
  }
}

Program compile_ir(IrProcs ir) {
  Compiler compiler = {0};

  for (u32 i = 0; i < ir.len; ++i) {
    IrProc *ir_proc = ir.items + i;

    ValueKind ret_val_kind = type_kinds_value_kinds_table[ir_proc->ret_val_type->kind];

    ProcParams params = {0};
    for (u32 i = 0; i < ir_proc->params.len; ++i) {
      IrProcParam *ir_proc_param = ir_proc->params.items + i;
      ValueKind proc_param_kind = type_kinds_value_kinds_table[ir_proc_param->type->kind];
      ProcParam proc_param = { ir_proc_param->name, proc_param_kind };
      DA_APPEND(params, proc_param);
    }

    Procedure *proc = program_push_proc(&compiler.program, ir_proc->name,
                                        ret_val_kind, params, ir_proc->is_naked);

    compile_ir_instrs(proc, &ir_proc->instrs);
  }

  return compiler.program;
}
