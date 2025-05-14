#include "mvm.h"
#include "log.h"
#include "checker.h"
#include "opt.h"
#include "gen.h"
#include "ops.h"
#include "misc.h"
#include "misc-platform.h"

Arg arg_value(Value value) {
  return (Arg) { ArgKindValue, { .value = value } };
}

Arg arg_var(Str name) {
  return (Arg) { ArgKindVar, { .var = name } };
}

Args create_args(void) {
  return (Args) {0};
}

void args_push_arg(Args *args, Arg arg) {
  DA_APPEND(*args, arg);
}

Ops get_ops(TargetPlatform target) {
  switch (target) {
  case TargetPlatformRaw_X86_64: return get_ops_x86_64();

  case TargetPlatformLinux_X86_64: {
    Ops a = get_ops_linux();
    Ops b = get_ops_x86_64();
    Op *buffer = arrays_concat(a.items, a.len,
                               b.items, b.len,
                               sizeof(Op));
    return (Ops) { buffer, a.len + b.len };
  }

  default: {
    ERROR("Wrong target platform\n");
    exit(1);
  }
  }
}

bool op_eq(Op *a, Op *b) {
  if (!str_eq(a->name, b->name))
    return false;

  if (a->arity != b->arity)
    return false;

  for (u32 i = 0; i < a->arity; ++i)
    if (a->args[i].kind != b->args[i].kind)
      return false;

  return true;
}


void proc_push_op(Procedure *proc, Str name, Str dest, Args args) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindOp;
  proc->instrs_end->as.op = (InstrOp) { name, dest, args };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_call(Procedure *proc, Str callee_name, Args args) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindCall;
  proc->instrs_end->as.call = (InstrCall) { callee_name, args, NULL };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_call_assign(Procedure *proc, Str dest, Str callee_name, Args args) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindCallAssign;
  proc->instrs_end->as.call_assign = (InstrCallAssign) { dest, callee_name, args, NULL };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_return(Procedure *proc) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindReturn;
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_return_value(Procedure *proc, Arg value) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindReturnValue;
  proc->instrs_end->as.ret_val = (InstrReturnValue) { value };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_jump(Procedure *proc, Str label_name) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindJump;
  proc->instrs_end->as.jump = (InstrJump) { label_name, NULL, false };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_cond_jump(Procedure *proc, RelOp rel_op,
                    Arg arg0, Arg arg1, Str label_name) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindConditionalJump;
  proc->instrs_end->as.cond_jump = (InstrConditionalJump) {
    arg0, arg1,
    rel_op, label_name,
    NULL,
  };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_add_label(Procedure *proc, Str name) {
  if (name.len == 0) {
    ERROR("Label name should not be empty\n");
    exit(1);
  }

  if (name.ptr[0] == '.') {
    ERROR("Label name should not start with a dot\n");
    exit(1);
  }

  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindLabel;
  proc->instrs_end->as.label = (InstrLabel) { name };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void proc_alloc(Procedure *proc, Str dest, u32 size) {
  Instr *prev = proc->instrs_end;
  LL_PREPEND(proc->instrs, proc->instrs_end, Instr);
  proc->instrs_end->kind = InstrKindAlloc;
  proc->instrs_end->as.alloc = (InstrAlloc) { dest, size };
  proc->instrs_end->visited = false;
  proc->instrs_end->removed = false;
  proc->instrs_end->next = NULL;
  proc->instrs_end->prev = prev;
}

void program_push_static_var(Program *program, Str name, Value value) {
  if (name.len == 0) {
    ERROR("Static variable name should not be empty\n");
    exit(1);
  }

  if (name.ptr[0] == '.') {
    ERROR("Static variable name should not start with a dot\n");
    exit(1);
  }

  u32 size = get_value_size(value.kind);
  ValueAs *data = aalloc(size);
  *data = value.as;

  StaticSegment segment = { name, (u8 *) data, size };
  DA_APPEND(program->static_data, segment);
}

void program_push_static_segment(Program *program, Str name, u8 *data, u32 size) {
  if (name.len == 0) {
    ERROR("Static segment name should not be empty\n");
    exit(1);
  }

  if (name.ptr[0] == '.') {
    ERROR("Static segment name should not start with a dot\n");
    exit(1);
  }

  StaticSegment segment = { name, data, size };
  DA_APPEND(program->static_data, segment);
}

Procedure *program_push_proc(Program *program, Str name,
                             ValueKind ret_val_kind,
                             ProcParams params) {
  if (name.len == 0) {
    ERROR("Procedure name should not be empty\n");
    exit(1);
  }

  if (name.ptr[0] == '.') {
    ERROR("Procedure name should not start with a dot\n");
    exit(1);
  }

  LL_PREPEND(program->procs, program->procs_end, Procedure);
  *program->procs_end = (Procedure) {
    name, ret_val_kind, params,
    NULL, NULL, NULL,
    false, false, NULL,
  };

  Procedure *proc = program->procs_end;

  return proc;
}

static void program_add_proc_ctxs(Program *program, TargetPlatform target) {
  Procedure *proc = program->procs;
  while (proc) {
    if (proc->ctx) {
      proc = proc->next;
      continue;
    }

    proc->ctx = aalloc(sizeof(ProcedureContext));

    Ops ops = get_ops(target);
    VariableLayers layers = get_layers(target);

    *proc->ctx = create_proc_ctx(proc, ops, layers,
                                 &program->static_data);

    proc = proc->next;
  }
}

void program_optimize(Program *program, TargetPlatform target) {
  program_check(program, target);
  program_add_proc_ctxs(program, target);
  program_type_check(program);

 Procedure *proc = program->procs;
  while (proc) {
    proc_opt_tail_recursion(proc);
    proc_opt_inline_args(proc, get_ops(target));
    proc_opt_remove_unused_var_defs(proc, get_ops(target));

    proc = proc->next;
  }
}

static OpGenProcs get_op_gen_procs(TargetPlatform target) {
  switch (target) {
  case TargetPlatformRaw_X86_64: return get_op_gen_procs_x86_64();

  case TargetPlatformLinux_X86_64: {
    OpGenProcs a = get_op_gen_procs_linux();
    OpGenProcs b = get_op_gen_procs_x86_64();
    OpGenProc *buffer = arrays_concat(a.items, a.len,
                                      b.items, b.len,
                                      sizeof(OpGenProc));

    return (OpGenProcs) { buffer, a.len + b.len };
  }

  default: {
    ERROR("Wrong target platform\n");
    exit(1);
  }
  }
}

Str program_gen_code(Program *program, TargetPlatform target) {
  program_check(program, target);
  program_add_proc_ctxs(program, target);
  program_type_check(program);

  StringBuilder sb = {0};

  Ops ops = get_ops(target);
  OpGenProcs op_gen_procs = get_op_gen_procs(target);

  sb_begin_program_wrap(&sb, program, target);

  switch (target) {
  case TargetPlatformRaw_X86_64:
  case TargetPlatformLinux_X86_64: {
    program_gen_asm_x86_64(program, &sb, &ops, &op_gen_procs);
  } break;

  default: {
    ERROR("Wrong target platform\n");
    exit(1);
  }
  }

  return sb_to_str(sb);
}
