#ifndef IR_H
#define IR_H

#include "../libs/mvm/mvm.h"
#include "shl_defs.h"

typedef enum {
  TypeKindUnit = 0,
  TypeKindS64, TypeKindS32,
  TypeKindS16, TypeKindS8,
  TypeKindU64, TypeKindU32,
  TypeKindU16, TypeKindU8,
  TypeKindPtr,
  TypeKindsCount,
} TypeKind;

typedef struct Type Type;

struct Type {
  TypeKind  kind;
  Type     *ptr_target;
};

typedef enum {
  IrArgKindValue = 0,
  IrArgKindVar,
} IrArgKind;

typedef union {
  i64   _s64;
  i32   _s32;
  i16   _s16;
  i8    _s8;
  u64   _u64;
  u32   _u32;
  u16   _u16;
  u8    _u8;
  Type *ptr;
} IrArgValueAs;

typedef struct {
  Type         *type;
  IrArgValueAs  as;
} IrArgValue;

typedef union {
  IrArgValue value;
  Str        var;
} IrArgAs;

typedef struct {
  IrArgKind  kind;
  IrArgAs    as;
} IrArg;

typedef enum {
  IrInstrKindAssign = 0,
  IrInstrKindRetVal,
  IrInstrKindIf,
  IrInstrKindWhile,
  IrInstrKindLabel,
} IrInstrKind;

typedef struct {
  Str dest;
  IrArg arg;
} IrInstrAssign;

typedef struct {
  IrArg arg;
} IrInstrRetVal;

typedef struct {
  IrArg arg0;
  IrArg arg1;
  RelOp rel_op;
  Str   label_name;
} IrInstrIf;

typedef struct {
  IrArg arg0;
  IrArg arg1;
  RelOp rel_op;
  Str   label_name;
} IrInstrWhile;

typedef struct {
  Str name;
} IrInstrLabel;

typedef union {
  IrInstrAssign assign;
  IrInstrRetVal ret_val;
  IrInstrIf     _if;
  IrInstrWhile  _while;
  IrInstrLabel  label;
} IrInstrAs;

typedef struct {
  IrInstrKind kind;
  IrInstrAs   as;
} IrInstr;

typedef Da(IrInstr) IrInstrs;

typedef struct {
  Str   name;
  Type *type;
} IrProcParam;

typedef Da(IrProcParam) IrProcParams;

typedef struct {
  IrInstrs      instrs;
  Str           name;
  IrProcParams  params;
  Type         *ret_val_type;
  bool          is_naked;
} IrProc;

typedef Da(IrProc) IrProcs;

#endif // IR_H
