#ifndef MVM_H
#define MVM_H

#include "shl_arena.h"
#include "shl_defs.h"
#include "shl_str.h"

typedef enum {
  ValueKindUnit = 0,
  ValueKindS64,
  ValueKindCount,
} ValueKind;

typedef union {
  i64 s64;
} ValueAs;

typedef struct {
  ValueKind kind;
  ValueAs   as;
} Value;

typedef enum {
  ArgKindValue = 0,
  ArgKindVar,
} ArgKind;

typedef union {
  Value value;
  Str   var;
} ArgAs;

typedef struct {
  ArgKind kind;
  ArgAs   as;
} Arg;

typedef Da(Arg) Args;

typedef enum {
  InstrKindOp = 0,
  InstrKindCall,
  InstrKindCallAssign,
  InstrKindReturn,
  InstrKindReturnValue,
  InstrKindJump,
  InstrKindConditionalJump,
  InstrKindLabel,
  InstrKindAlloc,
} InstrKind;

typedef struct Instr Instr;

typedef struct {
  Str name, dest;
  Args args;
} InstrOp;

typedef struct Procedure Procedure;

typedef struct {
  Str        callee_name;
  Args       args;
  Procedure *callee;
} InstrCall;

typedef struct {
  Str        dest;
  Str        callee_name;
  Args       args;
  Procedure *callee;
} InstrCallAssign;

typedef struct {
  Arg arg;
} InstrReturnValue;

typedef struct {
  Str    label_name;
  Instr *target;
  bool   is_proc_call;
} InstrJump;

typedef enum {
  RelOpEqual = 0,
  RelOpNotEqual,
  RelOpGreater,
  RelOpLess,
  RelOpGreaterOrEqual,
  RelOpLessOrEqual,
  RelOpCount,
} RelOp;

typedef struct {
  Arg    arg0, arg1;
  RelOp  rel_op;
  Str    label_name;
  Instr *target;
} InstrConditionalJump;

typedef struct {
  Str    name;
} InstrLabel;

typedef struct {
  Str dest;
  u32 size;
} InstrAlloc;

typedef union {
  InstrOp              op;
  InstrCall            call;
  InstrCallAssign      call_assign;
  InstrReturnValue     ret_val;
  InstrJump            jump;
  InstrConditionalJump cond_jump;
  InstrLabel           label;
  InstrAlloc           alloc;
} InstrAs;

struct Instr {
  InstrKind  kind;
  InstrAs    as;
  u32        index;
  bool       visited;
  bool       removed;
  Instr     *next;
  Instr     *prev;
};

typedef struct {
  Str       name;
  ValueKind kind;
} ProcParam;

typedef Da(ProcParam) ProcParams;

typedef struct ProcedureContext ProcedureContext;

struct Procedure {
  Str               name;
  ValueKind         ret_val_kind;
  ProcParams        params;
  Instr            *instrs;
  Instr            *instrs_end;
  ProcedureContext *ctx;
  bool              is_used;
  bool              has_callee;
  Procedure        *next;
};

typedef struct {
  Str  name;
  u8  *data;
  u32  size;
} StaticSegment;

typedef Da(StaticSegment) StaticData;

typedef struct {
  Procedure  *procs;
  Procedure  *procs_end;
  StaticData  static_data;
} Program;

typedef enum {
  ArgConditionAny = 0,
  ArgConditionVar,
  ArgConditionRefTarget,
} ArgCondition;

typedef struct {
  ValueKind    kind;
  ArgCondition cond;
} OpArg;

typedef struct {
  Str        name;
  ValueKind  dest_kind;
  OpArg     *args;
  u32        arity;
  bool       can_be_inlined;
} Op;

typedef struct {
  Op  *items;
  u32  len;
} Ops;

typedef enum {
  TargetPlatformRaw_X86_64 = 0,
  TargetPlatformLinux_X86_64,
} TargetPlatform;

Arg arg_value(Value value);
Arg arg_var(Str name);

Args create_args(void);
void args_push_arg(Args *args, Arg arg);

Ops   get_ops(TargetPlatform target);
bool  op_eq(Op *a, Op *b);

void proc_push_op(Procedure *proc, Str name, Str dest, Args args);
void proc_call(Procedure *proc, Str callee_name, Args args);
void proc_call_assign(Procedure *proc, Str dest, Str callee_name, Args args);
void proc_return(Procedure *proc);
void proc_return_value(Procedure *proc, Arg value);
void proc_jump(Procedure *proc, Str label_name);
void proc_cond_jump(Procedure *proc, RelOp rel_op,
                    Arg arg0, Arg arg1, Str label_name);
void proc_add_label(Procedure *proc, Str name);
void proc_alloc(Procedure *proc, Str dest, u32 size);

void program_push_static_var(Program *program, Str name, Value value);
void program_push_static_segment(Program *program, Str name, u8 *bytes, u32 bytes_len);

Procedure *program_push_proc(Program *program, Str name,
                             ValueKind ret_val_kind,
                             ProcParams params);

void program_optimize(Program *program, TargetPlatform target);
Str  program_gen_code(Program *program, TargetPlatform target);

#endif // MVM_H
