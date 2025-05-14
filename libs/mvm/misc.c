#include <string.h>

#include "misc.h"
#include "shl_log.h"

u32 get_value_size(ValueKind kind) {
  switch (kind) {
  case ValueKindUnit: return 0;
  case ValueKindS64:  return 8;

  default: {
    ERROR("Unreachable\n");
    exit(1);
  }
  }
}

ValueKind str_to_value_kind(Str str) {
  static Str value_kind_strs[ValueKindCount] = {
    [ValueKindUnit] = STR_LIT("unit"),
    [ValueKindS64] = STR_LIT("s64"),
  };

  for (u32 i = 0; i < ValueKindCount; ++i) {
    if (str_eq(value_kind_strs[i], str))
      return (ValueKind) i;
  }

  ERROR("Wrong type name: "STR_FMT"\n", STR_ARG(str));
  exit(1);
}

Str value_to_str(Value value) {
  StringBuilder sb = {0};

  switch (value.kind) {
  case ValueKindUnit: {
    ERROR("Value of type unit cannot be used\n");
    exit(1);
  }

  case ValueKindS64: sb_push_i64(&sb, value.as.s64); break;

  default: {
    ERROR("Wrong value kind\n");
    exit(1);
  } break;
  }

  return sb_to_str(sb);
}

Arg *get_first_arg_ptr(Instr *instr) {
  switch (instr->kind) {
  case InstrKindOp:              return &instr->as.op.args.items[0];
  case InstrKindReturnValue:     return &instr->as.ret_val.arg;
  case InstrKindConditionalJump: return &instr->as.cond_jump.arg0;
  default:                       return NULL;
  }
}

Arg *get_second_arg_ptr(Instr *instr) {
  switch (instr->kind) {
  case InstrKindOp:              return &instr->as.op.args.items[1];
  case InstrKindConditionalJump: return &instr->as.cond_jump.arg1;
  default:                       return NULL;
  }
}

void *arrays_concat(void *a, u32 a_len, void *b, u32 b_len, u32 size) {
  void *new = malloc((a_len + b_len) * size);

  memcpy(new, a, a_len * size);
  memcpy(new + a_len * size, b, b_len * size);

  return new;
}
