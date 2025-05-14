#include <ctype.h>

#include "general.h"
#include "../ops.h"
#include "../log.h"
#include "../shl_str.h"

static Str reserve_reg_loc(StringBuilder *sb, Loc loc, Str arg) {
  Str reserved = loc.str;
  if (loc.kind == LocKindStack)
    reserved = STR_LIT("rax");

  if (!str_eq(reserved, arg)) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, reserved);
    sb_push_char(sb, ',');
    sb_push_str(sb, arg);
    sb_push_char(sb, '\n');
  }

  return reserved;
}

static void free_reserved_loc(StringBuilder *sb, Str reserved, Str prev) {
  if (!str_eq(reserved, prev)) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, prev);
    sb_push_char(sb, ',');
    sb_push_str(sb, reserved);
    sb_push_char(sb, '\n');
  }
}

static bool arg_is_on_stack(Str arg) {
  return arg.len > 0 && arg.ptr[arg.len - 1] == ']';
}

static void gen_comparison_op(StringBuilder *sb, Loc dest_loc,
                              Str arg0, Str arg1, Str op) {
  if (arg_is_on_stack(arg0) &&
      arg_is_on_stack(arg1)) {
    sb_push(sb, "  mov rax,");
    sb_push_str(sb, arg0);
    sb_push_char(sb, '\n');

    arg0 = STR_LIT("rax");
  }

  sb_push(sb, "  cmp ");
  sb_push_str(sb, arg0);
  sb_push_char(sb, ',');
  sb_push_str(sb, arg1);
  sb_push(sb, "\n  set");
  sb_push_str(sb, op);
  sb_push(sb, " al\n");

  if (arg_is_on_stack(dest_loc.str)) {
    sb_push(sb, "  mov rax,");
    sb_push_str(sb, dest_loc.str);
    sb_push_char(sb, '\n');
  }

  sb_push(sb, "  movzx ");
  if (arg_is_on_stack(dest_loc.str))
    sb_push(sb, "rax");
  else
    sb_push_str(sb, dest_loc.str);
  sb_push(sb, ",al\n");

  if (arg_is_on_stack(dest_loc.str)) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, dest_loc.str);
    sb_push(sb, ",rax\n");
  }
}

void gen_put_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  if (!str_eq(dest_loc.str, args[0])) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, dest_loc.str);
    sb_push_char(sb, ',');
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }
}

void gen_add_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  Str dest_reg = reserve_reg_loc(sb, dest_loc, args[0]);

  sb_push(sb, "  add ");
  sb_push_str(sb, dest_reg);
  sb_push_char(sb, ',');
  sb_push_str(sb, args[1]);
  sb_push_char(sb, '\n');

  free_reserved_loc(sb, dest_reg, dest_loc.str);
}

static void gen_sub_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  Str dest_reg = reserve_reg_loc(sb, dest_loc, args[0]);

  sb_push(sb, "  sub ");
  sb_push_str(sb, dest_reg);
  sb_push_char(sb, ',');
  sb_push_str(sb, args[1]);
  sb_push_char(sb, '\n');

  free_reserved_loc(sb, dest_reg, dest_loc.str);
}

static void gen_mul_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  bool is_arg1_imm_value = args[1].len > 0 &&
                           isdigit(args[1].ptr[0]) &&
                           args[1].ptr[args[1].len - 1] != ']';

  if (is_arg1_imm_value) {
    sb_push(sb, "  mov r10,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');
  }

  sb_push(sb, "  mov rax,");
  sb_push_str(sb, args[0]);
  sb_push(sb, "\n  imul ");
  if (is_arg1_imm_value)
    sb_push(sb, "r10");
  else
    sb_push_str(sb, args[1]);
  sb_push_char(sb, '\n');
  sb_push(sb, "  mov ");
  sb_push_str(sb, dest_loc.str);
  sb_push(sb, ",rax\n");
}

static void gen_div_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  bool is_arg1_imm_value = args[1].len > 0 &&
                           isdigit(args[1].ptr[0]) &&
                           args[1].ptr[args[1].len - 1] != ']';

  if (is_arg1_imm_value) {
    sb_push(sb, "  mov r10,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');
  }

  sb_push(sb, "  mov rax,");
  sb_push_str(sb, args[0]);
  sb_push(sb, "\n  cdq\n  idiv ");
  if (is_arg1_imm_value)
    sb_push(sb, "r10");
  else
    sb_push_str(sb, args[1]);
  sb_push_char(sb, '\n');
  sb_push(sb, "  mov ");
  sb_push_str(sb, dest_loc.str);
  sb_push(sb, ",rax\n");
}

static void gen_mod_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  bool is_arg1_imm_value = args[1].len > 0 &&
                           isdigit(args[1].ptr[0]) &&
                           args[1].ptr[args[1].len - 1] != ']';

  if (is_arg1_imm_value) {
    sb_push(sb, "  mov r10,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');
  }

  sb_push(sb, "  mov rax,");
  sb_push_str(sb, args[0]);
  sb_push(sb, "\n  cdq\n  idiv ");
  if (is_arg1_imm_value)
    sb_push(sb, "r10");
  else
    sb_push_str(sb, args[1]);
  sb_push_char(sb, '\n');
  sb_push(sb, "  mov ");
  sb_push_str(sb, dest_loc.str);
  sb_push(sb, ",rdx\n");
}

static void gen_neg_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  Str dest_reg = reserve_reg_loc(sb, dest_loc, args[0]);

  sb_push(sb, "  neg ");
  sb_push_str(sb, dest_reg);
  sb_push_char(sb, '\n');

  free_reserved_loc(sb, dest_reg, dest_loc.str);
}

static void gen_eq_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  gen_comparison_op(sb, dest_loc, args[0], args[1], STR_LIT("e"));
}

static void gen_ne_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  gen_comparison_op(sb, dest_loc, args[0], args[1], STR_LIT("ne"));
}

static void gen_gt_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  gen_comparison_op(sb, dest_loc, args[0], args[1], STR_LIT("g"));
}

static void gen_ls_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  gen_comparison_op(sb, dest_loc, args[0], args[1], STR_LIT("l"));
}

static void gen_ge_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  gen_comparison_op(sb, dest_loc, args[0], args[1], STR_LIT("ge"));
}

static void gen_le_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  gen_comparison_op(sb, dest_loc, args[0], args[1], STR_LIT("le"));
}

static void gen_ref_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  bool dest_ptr_is_on_stack = arg_is_on_stack(dest_loc.str);

  sb_push(sb, "  lea ");
  if (dest_ptr_is_on_stack)
    sb_push(sb, "rax");
  else
    sb_push_str(sb, dest_loc.str);
  sb_push_char(sb, ',');
  sb_push_str(sb, args[0]);
  sb_push_char(sb, '\n');
  if (dest_ptr_is_on_stack) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, dest_loc.str);
    sb_push(sb, ",rax\n");
  }
}

static void gen_deref_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  bool dest_is_on_stack = arg_is_on_stack(dest_loc.str);
  bool arg0_ptr_is_on_stack = arg_is_on_stack(args[0]);

  Str prev_dest = dest_loc.str;

  if (dest_is_on_stack)
    dest_loc.str = STR_LIT("rax");

  if (arg0_ptr_is_on_stack) {
    sb_push(sb, "  mov rax,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }

  sb_push(sb, "  mov ");
  sb_push_str(sb, dest_loc.str);
  sb_push(sb,  ",qword[");
  if (arg0_ptr_is_on_stack)
    sb_push(sb, "rax");
  else
    sb_push_str(sb, args[0]);
  sb_push(sb, "]\n");

  if (dest_is_on_stack) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, prev_dest);
    sb_push(sb, ",rax\n");
  }
}

static void gen_deref_str_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  bool arg0_ptr_is_on_stack = arg_is_on_stack(args[0]);

  if (arg0_ptr_is_on_stack) {
    sb_push(sb, "  mov rax,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');
  }

  sb_push(sb, "  movzx rax,byte[");
  if (arg0_ptr_is_on_stack)
    sb_push(sb, "rax");
  else
    sb_push_str(sb, args[0]);
  sb_push(sb, "]\n");

  if (!str_eq(dest_loc.str, STR_LIT("rax"))) {
    sb_push(sb, "  mov ");
    sb_push_str(sb, dest_loc.str);
    sb_push(sb, ",rax\n");
  }
}

static void gen_deref_put_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  (void) dest_loc;

  if (arg_is_on_stack(args[0])) {
    sb_push(sb, "  mov rax,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');

    args[0] = STR_LIT("rax");
  }

  sb_push(sb, "  mov qword[");
  sb_push_str(sb, args[0]);
  sb_push(sb, "],");
  sb_push_str(sb, args[1]);
  sb_push_char(sb, '\n');
}

static void gen_deref_put_str_op(StringBuilder *sb, Loc dest_loc, Str *args) {
  (void) dest_loc;


  if (arg_is_on_stack(args[0])) {
    sb_push(sb, "  mov rax,");
    sb_push_str(sb, args[0]);
    sb_push_char(sb, '\n');

    args[0] = STR_LIT("rax");
  }

  if (!str_eq(args[1], STR_LIT("r10"))) {
    sb_push(sb, "  mov r10,");
    sb_push_str(sb, args[1]);
    sb_push_char(sb, '\n');

    args[1] = STR_LIT("r10b");
  }

  sb_push(sb, "  mov byte[");
  sb_push_str(sb, args[0]);
  sb_push(sb, "],");
  sb_push_str(sb, args[1]);
  sb_push_char(sb, '\n');
}

static OpArg bin_op_args[] = {
  { ValueKindS64, ArgConditionAny },
  { ValueKindS64, ArgConditionAny },
};

static OpArg un_op_args[] = {
  { ValueKindS64, ArgConditionAny },
};

static OpArg bin_op_args_var[] = {
  { ValueKindS64, ArgConditionVar },
  { ValueKindS64, ArgConditionAny },
};

static OpArg un_op_args_ref_target[] = {
  { ValueKindS64, ArgConditionRefTarget },
};

static Op ops[] = {
  {
    STR_LIT("put"), ValueKindS64, un_op_args, 1, true },
  { STR_LIT("add"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("sub"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("mul"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("div"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("mod"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("neg"), ValueKindS64, un_op_args, 1, false },

  { STR_LIT("eq"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("ne"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("gt"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("ls"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("ge"), ValueKindS64, bin_op_args, 2, false },
  { STR_LIT("le"), ValueKindS64, bin_op_args, 2, false },

  { STR_LIT("ref"),           ValueKindS64,  un_op_args_ref_target, 1, false },
  { STR_LIT("deref"),         ValueKindS64,  un_op_args,            1, false },
  { STR_LIT("deref_str"),     ValueKindS64,  un_op_args,            1, false },
  { STR_LIT("deref_put"),     ValueKindUnit, bin_op_args_var,       2, false },
  { STR_LIT("deref_put_str"), ValueKindUnit, bin_op_args_var,       2, false },
};

static OpGenProc op_gen_procs[] = {
  gen_put_op,
  gen_add_op,
  gen_sub_op,
  gen_mul_op,
  gen_div_op,
  gen_mod_op,
  gen_neg_op,

  gen_eq_op,
  gen_ne_op,
  gen_gt_op,
  gen_ls_op,
  gen_ge_op,
  gen_le_op,

  gen_ref_op,
  gen_deref_op,
  gen_deref_str_op,
  gen_deref_put_op,
  gen_deref_put_str_op,
};

Ops get_ops_x86_64(void) {
  return (Ops) { ops, ARRAY_LEN(ops) };
}

OpGenProcs get_op_gen_procs_x86_64(void) {
  return (OpGenProcs) { op_gen_procs, ARRAY_LEN(op_gen_procs) };
}
