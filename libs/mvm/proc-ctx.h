#ifndef PROC_CTX_H
#define PROC_CTX_H

#include "shl_arena.h"
#include "shl_defs.h"
#include "mvm.h"
#include "misc-platform.h"

typedef struct {
  bool *items;
  u32   len;
} RegsUsed;

typedef enum {
  ParamStateNone = 0,
  ParamStateParam,
  ParamStatePushed,
  ParamStateUsedAfterPush,
} ParamState;

typedef struct Variable Variable;

struct Variable {
  Str          name;
  ValueKind    kind;
  u32          mem_unit;
  u32          begin_index;
  u32          end_index;
  Instr       *begin;
  Da(Instr *)  used;
  bool         can_be_ref_target;
  bool         is_proc_param;
  bool         is_static;
  Variable    *next;
};

typedef Da(Variable *) Variables;

typedef struct {
  Instr     *key;
  Variable  *dest_var;
  Variables  arg_vars;
} InstrData;

typedef Da(InstrData) InstrsData;

struct ProcedureContext {
  Procedure  *proc;
  Variable   *vars;
  Variable   *vars_end;
  InstrsData  instrs_data;
  u32         max_params_pushed;
};

#ifndef MVM_H
typedef struct ProcedureContext ProcedureContext;
#endif

InstrData *get_instr_data(ProcedureContext *proc_ctx, Instr *key);
Variable *lookup_variable(Variable *vars, Str name);
ValueKind get_arg_kind(ProcedureContext *proc_ctx, Arg *arg);
ProcedureContext create_proc_ctx(Procedure *proc, Ops ops,
                                 VariableLayers layers,
                                 StaticData *static_data);

#endif // PROC_CTX_H
