#ifndef IR_H
#define IR_H

#include "mvm/src/mvm.h"
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

typedef Da(IrArg) IrArgs;

typedef enum {
  IrInstrKindCreate = 0,
  IrInstrKindAssign,
  IrInstrKindIf,
  IrInstrKindWhile,
  IrInstrKindJump,
  IrInstrKindLabel,
  IrInstrKindRet,
  IrInstrKindRetVal,
  IrInstrKindCall,
  IrInstrKindAsm,
  IrInstrKindBinOp,
  IrInstrKindUnOp,
  IrInstrKindPreAssignOp,
  IrInstrKindCast,
} IrInstrKind;

typedef struct {
  Str   dest;
  Type *dest_type;
} IrInstrCreate;

typedef struct {
  Str   dest;
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
  Str   begin_label_name;
  Str   end_label_name;
} IrInstrWhile;

typedef struct {
  Str label_name;
} IrInstrJump;

typedef struct {
  Str name;
} IrInstrLabel;

typedef struct {
  Str    callee_name;
  Str    dest;
  IrArgs args;
} IrInstrCall;

typedef Da(Str) VarNames;

typedef struct {
  Str            dest;
  Type          *dest_type;
  Str            code;
  VarNames       var_names;
} IrInstrAsm;

typedef struct {
  Str dest;
  Str op;
  IrArg arg0;
  IrArg arg1;
} IrInstrBinOp;

typedef struct {
  Str dest;
  Str op;
  IrArg arg;
} IrInstrUnOp;

typedef struct {
  Str dest;
  Str op;
  IrArg arg;
} IrInstrPreAssignOp;

typedef struct {
  Str    dest;
  Type  *type;
  IrArg  arg;
} IrInstrCast;

typedef union {
  IrInstrCreate       create;
  IrInstrAssign       assign;
  IrInstrRetVal       ret_val;
  IrInstrIf           _if;
  IrInstrWhile        _while;
  IrInstrJump         jump;
  IrInstrLabel        label;
  IrInstrCall         call;
  IrInstrAsm          _asm;
  IrInstrBinOp        bin_op;
  IrInstrUnOp         un_op;
  IrInstrPreAssignOp  pre_assign_op;
  IrInstrCast         cast;
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
  bool          is_inlined;
} IrProc;

typedef Da(IrProc) IrProcs;

typedef struct {
  Str   name;
  Value value;
} StaticVariable;

typedef Da(StaticVariable) StaticVariables;

typedef struct {
  Str  name;
  u8  *data;
  u32  size;
} StaticBuffer;

typedef Da(StaticBuffer) StaticData;

typedef struct {
  IrProcs         procs;
  StaticVariables static_vars;
  StaticData      static_data;
} Ir;

#endif // IR_H
