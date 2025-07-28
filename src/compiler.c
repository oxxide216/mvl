#include "compiler.h"
#include "shl_log.h"

typedef struct {
  Program  program;
  Ir      *ir;
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

static Value ir_arg_value_to_value(IrArgValue *value) {
  switch (value->type->kind) {
  case TypeKindUnit: return (Value) { ValueKindUnit, {} };
  case TypeKindS64:  return (Value) { ValueKindS64,  { .s64 = value->as._s64 } };
  case TypeKindS32:  return (Value) { ValueKindS32,  { .s32 = value->as._s32} };
  case TypeKindS16:  return (Value) { ValueKindS16,  { .s16 = value->as._s16} };
  case TypeKindS8:   return (Value) { ValueKindS8,   { .s8 = value->as._s8} };
  case TypeKindU64:  return (Value) { ValueKindU64,  { .u64 = value->as._u64 } };
  case TypeKindU32:  return (Value) { ValueKindU32,  { .u32 = value->as._u32} };
  case TypeKindU16:  return (Value) { ValueKindU16,  { .u16 = value->as._u16} };
  case TypeKindU8:   return (Value) { ValueKindU8,   { .u8 = value->as._u8} };
  case TypeKindPtr:  return (Value) { ValueKindU64,  { .u64 = value->as._u64 } };

  default: {
    ERROR("Wrong value type\n");
    exit(1);
  }
  }
}

static Arg ir_arg_to_arg(IrArg *ir_arg) {
  Arg arg = {0};

  if (ir_arg->kind == IrArgKindValue) {
    Value value = ir_arg_value_to_value(&ir_arg->as.value);
    return (Arg) { ArgKindValue, { .value = value } };
  } else if (ir_arg->kind == IrArgKindVar) {
    return (Arg) { ArgKindVar, { .var = ir_arg->as.var } };
  }

  return arg;
}

static void compile_ir_instrs(Compiler *compiler, Procedure *proc, u32 ir_proc_index) {
  IrProc *ir_proc = compiler->ir->procs.items + ir_proc_index;
  for (u32 i = 0; i < ir_proc->instrs.len; ++i) {
    IrInstr *ir_instr = ir_proc->instrs.items + i;

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

    case IrInstrKindWhile: {
      RelOp rel_op = ir_instr->as._while.rel_op;
      Arg arg0 = ir_arg_to_arg(&ir_instr->as._while.arg0);
      Arg arg1 = ir_arg_to_arg(&ir_instr->as._while.arg1);
      Str begin_label_name = ir_instr->as._while.begin_label_name;
      Str end_label_name = ir_instr->as._while.end_label_name;

      proc_add_label(proc, begin_label_name);
      proc_cond_jump(proc, rel_op, arg0, arg1, end_label_name);
    } break;

    case IrInstrKindJump: {
      Str label_name = ir_instr->as.jump.label_name;

      proc_jump(proc, label_name);
    } break;

    case IrInstrKindLabel: {
      proc_add_label(proc, ir_instr->as.label.name);
    } break;

    case IrInstrKindRet: {
      proc_return(proc);
    } break;

    case IrInstrKindRetVal: {
      Arg arg = ir_arg_to_arg(&ir_instr->as.ret_val.arg);
      proc_return_value(proc, arg);
    } break;

    case IrInstrKindCall: {
      Str dest = ir_instr->as.call.dest;
      Str callee_name = ir_instr->as.call.callee_name;
      IrArgs *ir_args = &ir_instr->as.call.args;

      Args args = {0};

      for (u32 j = 0; j < ir_args->len; ++j) {
        Arg arg = ir_arg_to_arg(ir_args->items + j);
        DA_APPEND(args, arg);
      }

      if (dest.len > 0)
        proc_call_assign(proc, dest, callee_name, args);
      else
        proc_call(proc, callee_name, args);
    } break;

    case IrInstrKindAsm: {
      Str dest = ir_instr->as._asm.dest;
      Type *dest_type = ir_instr->as._asm.dest_type;
      Str code = ir_instr->as._asm.code;
      VarNames *var_names = &ir_instr->as._asm.var_names;

      ValueKind dest_kind = type_kinds_value_kinds_table[dest_type->kind];
      InlineAsmSegments segments = {0};

      StringBuilder sb = {0};
      u32 var_index = 0;

      for (u32 i = 0; i < code.len; ++i) {
        if (code.ptr[i] == '@') {
          if (sb.len > 0) {
            segments_push_text(&segments, sb_to_str(sb));
            sb = (StringBuilder) {0};
          }

          if (++i >= code.len) {
            ERROR("Expected variable location specifier, but got end of string\n");
            exit(1);
          }

          if (var_index >= var_names->len) {
            ERROR("Not enough variable arguments in inline assembly\n");
            exit(1);
          }

          if (i >= code.len) {
            ERROR("Expected variable location specifier, but got end of string\n");
            exit(1);
          }

          TargetLocKind target_loc_kind;
          if (code.ptr[i] == 'a') {
            target_loc_kind = TargetLocKindAny;
          } else if (code.ptr[i] == 'r') {
            target_loc_kind = TargetLocKindReg;
          } else if (code.ptr[i] == 'm') {
            target_loc_kind = TargetLocKindMem;
          } else {
            ERROR("Invalid variable location specifier: %c\n", code.ptr[i]);
            exit(1);
          }

          segments_push_var(&segments, var_names->items[var_index++],
                            target_loc_kind, false);
        } else {
          sb_push_char(&sb, code.ptr[i]);
        }
      }

      if (sb.len > 0)
        segments_push_text(&segments, sb_to_str(sb));

      proc_inline_asm(proc, dest, dest_kind, segments);
    } break;

    default: {
      ERROR("Unexpected IR instr kind: %u\n", ir_instr->kind);
      exit(1);
    }
    }
  }
}

Program compile_ir(Ir *ir) {
  Compiler compiler = {0};
  compiler.ir = ir;

  for (u32 i = 0; i < ir->procs.len; ++i) {
    IrProc *ir_proc = ir->procs.items + i;

    ValueKind ret_val_kind =
      type_kinds_value_kinds_table[ir_proc->ret_val_type->kind];

    ProcParams params = {0};
    for (u32 i = 0; i < ir_proc->params.len; ++i) {
      IrProcParam *ir_proc_param = ir_proc->params.items + i;
      ValueKind proc_param_kind = type_kinds_value_kinds_table[ir_proc_param->type->kind];
      ProcParam proc_param = { ir_proc_param->name, proc_param_kind };
      DA_APPEND(params, proc_param);
    }

    Procedure *proc = program_push_proc(&compiler.program, ir_proc->name,
                                        ret_val_kind, params, ir_proc->is_naked,
                                        ir_proc->is_inlined);

    compile_ir_instrs(&compiler, proc, i);
  }

  return compiler.program;
}
