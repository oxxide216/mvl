#include "ir_to_mvm.h"
#include "shl_log.h"

Value ir_arg_value_to_value(IrArgValue *value) {
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

Arg ir_arg_to_arg(IrArg *ir_arg) {
  Arg arg = {0};

  if (ir_arg->kind == IrArgKindValue) {
    Value value = ir_arg_value_to_value(&ir_arg->as.value);
    return (Arg) { ArgKindValue, { .value = value } };
  } else if (ir_arg->kind == IrArgKindVar) {
    return (Arg) { ArgKindVar, { .var = ir_arg->as.var } };
  }

  return arg;
}
