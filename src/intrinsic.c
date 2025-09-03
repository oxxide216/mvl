#include "intrinsic.h"
#include "mvm/src/misc.h"
#include "ir_to_mvm.h"
#include "shl_log.h"

static void segments_push_ir_arg(InlineAsmSegments *segments, IrArg *arg,
                                 TargetLocKind target_loc_kind, bool is_dest_var) {
  if (arg->kind == IrArgKindValue) {
    Value value = ir_arg_value_to_value(&arg->as.value);
    segments_push_text(segments, value_to_str(value));
  } else {
    segments_push_var(segments, arg->as.var, target_loc_kind, is_dest_var);
  }
}

void proc_compile_bin_intrinsic(Procedure *proc, Str dest, Str op, IrArg arg0, IrArg arg1) {
  InlineAsmSegments segments = {0};

  proc_assign(proc, dest, ir_arg_to_arg(&arg0));

  if (str_eq(op, STR_LIT("+")) || str_eq(op, STR_LIT("-"))) {
    if (str_eq(op, STR_LIT("+")))
      segments_push_text(&segments, STR_LIT("add "));
    else
      segments_push_text(&segments, STR_LIT("sub "));
    segments_push_var(&segments, dest, TargetLocKindNotImm, true);
    segments_push_text(&segments, STR_LIT(","));
    segments_push_ir_arg(&segments, &arg1, TargetLocKindImm, false);
  } else if (str_eq(op, STR_LIT("*")) || str_eq(op, STR_LIT("/"))) {
    segments_push_text(&segments, STR_LIT("mov rax,"));
    segments_push_ir_arg(&segments, &arg0, TargetLocKindImm, true);
    if (str_eq(op, STR_LIT("*")))
      segments_push_text(&segments, STR_LIT("\n  imul "));
    else
      segments_push_text(&segments, STR_LIT("\n  idiv "));
    segments_push_ir_arg(&segments, &arg1, TargetLocKindImm, false);
  } else {
    ERROR("Unknown binary operator: `"STR_FMT"`\n", STR_ARG(op));
    exit(1);
  }

  proc_inline_asm(proc, dest, ValueKindS64, segments);
}

void proc_compile_un_intrinsic(Procedure *proc, Str dest, Str op, IrArg arg) {
  InlineAsmSegments segments = {0};

  if (str_eq(op, STR_LIT("&"))) {
    segments_push_text(&segments, STR_LIT("lea "));
    segments_push_var(&segments, dest, TargetLocKindReg, true);
    segments_push_text(&segments, STR_LIT(","));
    if (arg.kind == IrArgKindValue) {
      Value value = ir_arg_value_to_value(&arg.as.value);
      segments_push_text(&segments, value_to_str(value));
    } else {
      segments_push_var(&segments, arg.as.var, TargetLocKindMem, false);
    }
  } else if (str_eq(op, STR_LIT("*"))) {
    segments_push_text(&segments, STR_LIT("mov "));
    segments_push_var(&segments, dest, TargetLocKindReg, true);
    segments_push_text(&segments, STR_LIT(",qword["));
    if (arg.kind == IrArgKindValue) {
      Value value = ir_arg_value_to_value(&arg.as.value);
      segments_push_text(&segments, value_to_str(value));
    } else {
      segments_push_var(&segments, arg.as.var, TargetLocKindReg, false);
    }
    segments_push_text(&segments, STR_LIT("]"));
  } else {
    ERROR("Unknown unary operator: `"STR_FMT"`\n", STR_ARG(op));
    exit(1);
  }

  proc_inline_asm(proc, dest, ValueKindS64, segments);
}

void proc_compile_pre_assign_intrinsic(Procedure *proc, Str dest, Str op, IrArg arg) {
  InlineAsmSegments segments = {0};

  if (str_eq(op, STR_LIT("*"))) {
    segments_push_text(&segments, STR_LIT("mov qword["));
    segments_push_var(&segments, dest, TargetLocKindReg, true);
    segments_push_text(&segments, STR_LIT("],"));
    segments_push_ir_arg(&segments, &arg, TargetLocKindReg, false);
  } else {
    ERROR("Unknown pre-assign operator: `"STR_FMT"`\n", STR_ARG(op));
    exit(1);
  }

  proc_inline_asm(proc, dest, ValueKindUnit, segments);
}
