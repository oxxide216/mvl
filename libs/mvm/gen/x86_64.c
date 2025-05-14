#include "../gen.h"
#include "../ops.h"
#include "../log.h"
#include "../proc-ctx.h"
#include "../misc.h"
#include "../misc-platform.h"

typedef struct {
  Variable *var;
  Loc       loc;
} VariableLocPair;

typedef Da(VariableLocPair) VariableLocPairs;

typedef struct {
  u32  cap;
  u32  end_index;
} StackSegment;

typedef struct {
  Da(StackSegment) segments;
  u32              max_size;
} Stack;

typedef struct {
  StringBuilder    sb;
  VariableLocPairs var_loc_pairs;
  Stack            stack;
  u32              max_mem_units_used[ValueKindCount];
  u32              labels_count;
  bool             found_return;
} ProcedureGenerationContext;

typedef struct {
  Str *items;
  u32  len;
} RegNames;

static Str reg_names_s64[] = { STR_LIT("rbx"), STR_LIT("r12"),
                               STR_LIT("r13"), STR_LIT("r14"),
                               STR_LIT("r15") };

static Str param_reg_names_s64[] = { STR_LIT("rdi"), STR_LIT("rsi"),
                                     STR_LIT("rdx"), STR_LIT("rcx"),
                                     STR_LIT("r8"), STR_LIT("r9") };

static Str cond_jump_cmds[RelOpCount] = {
  [RelOpEqual]          = STR_LIT("je"),
  [RelOpNotEqual]       = STR_LIT("jne"),
  [RelOpGreater]        = STR_LIT("jg"),
  [RelOpLess]           = STR_LIT("jl"),
  [RelOpGreaterOrEqual] = STR_LIT("jge"),
  [RelOpLessOrEqual]    = STR_LIT("jle"),
};

static RegNames get_reg_names(ValueKind kind) {
  switch (kind) {
  case ValueKindS64: {
    return (RegNames) {
      reg_names_s64,
      ARRAY_LEN(reg_names_s64),
    };
  }

  default: return (RegNames) {0};
  }
}

static RegNames get_param_reg_names(ValueKind kind) {
  switch (kind) {
  case ValueKindS64: {
    return (RegNames) {
      param_reg_names_s64,
      ARRAY_LEN(param_reg_names_s64),
    };
  }

  default: return (RegNames) {0};
  }
}

static u32 stack_alloc(Stack *stack, u32 cap,
                       u32 begin_index,
                       u32 end_index) {
  u32 offset = 0;

  for (u32 i = 0; i < stack->segments.len; ++i) {
    StackSegment *segment = stack->segments.items + i;
    offset += segment->cap;
    if (segment->cap >= cap && segment->end_index < begin_index) {
        segment->end_index = end_index;
        return offset;
    }
  }

  StackSegment new_segment = { cap, end_index };
  DA_APPEND(stack->segments, new_segment);

  stack->max_size += cap;

  return offset + cap;
}

static Str get_ptr_prefix(ValueKind kind) {
  switch (kind) {
  case ValueKindS64: return STR_LIT("qword");

  default: {
    ERROR("Wrong value kind\n");
    exit(1);
  }
  }
}

static Loc gen_var_loc(ProcedureGenerationContext *proc_gen_ctx,
                       Variable *var, u32 *mem_units_offsets) {
  RegNames reg_names = get_reg_names(var->kind);

  u32 mem_unit = var->mem_unit;
  if (mem_unit >= mem_units_offsets[var->kind])
    mem_unit -= mem_units_offsets[var->kind];
  else
    mem_unit = 0;

  if (mem_unit < reg_names.len &&
      !var->can_be_ref_target &&
      !var->is_static)
    return (Loc) { LocKindReg, reg_names.items[mem_unit] };

  if (var->can_be_ref_target)
    ++mem_units_offsets[var->kind];

  u32 size = get_value_size(var->kind);
  u32 offset = stack_alloc(&proc_gen_ctx->stack, size,
                           var->begin_index, var->end_index);

  StringBuilder sb = {0};
  sb_push_str(&sb, get_ptr_prefix(var->kind));
  sb_push(&sb, "[rbp-");
  sb_push_u32(&sb, offset);
  sb_push_char(&sb, ']');

  return (Loc) { LocKindStack, sb_to_str(sb) };
}

static Loc gen_param_var_loc(Variable *var, u32 *param_mem_units_offsets,
                             u32 *params_offset, u32 index) {
  RegNames reg_names = get_param_reg_names(var->kind);

  if (index >= param_mem_units_offsets[var->kind])
    index -= param_mem_units_offsets[var->kind];
  else
    index = 0;

  if (index < reg_names.len) {
    return (Loc) { LocKindReg, reg_names.items[index] };
  }

  if (var->can_be_ref_target)
    ++param_mem_units_offsets[var->kind];

  StringBuilder sb = {0};
  sb_push_str(&sb, get_ptr_prefix(var->kind));
  sb_push(&sb, "[rbp+");
  sb_push_u32(&sb, *params_offset);
  sb_push_char(&sb, ']');

  *params_offset -= get_value_size(var->kind);

  return (Loc) { LocKindStack, sb_to_str(sb) };
}

static void set_var_locs(ProcedureGenerationContext *proc_gen_ctx,
                         ProcedureContext *proc_ctx) {
  u32 mem_units_offsets[ValueKindCount] = {0};
  u32 param_mem_units_offsets[ValueKindCount] = {0};

  u32 params_offset = 8;
  u32 param_mem_units_used[ValueKindCount] = {0};
  u32 params_count = 0;

  for (u32 i = 0; i < proc_ctx->proc->params.len; ++i) {
    ValueKind kind = proc_ctx->proc->params.items[i].kind;
    RegNames reg_names = get_param_reg_names(kind);

    if (param_mem_units_used[kind] >= reg_names.len)
    params_offset += get_value_size(kind);

    ++param_mem_units_used[kind];
  }

  Variable *var = proc_ctx->vars;
  while (var) {
    if (var->used.len == 0) {
      var = var->next;
      continue;
    }

    VariableLocPair var_loc_pair = { var, {0} };

    if (var->is_static) {
      var_loc_pair.loc = (Loc) { LocKindStack, var->name };
      DA_APPEND(proc_gen_ctx->var_loc_pairs, var_loc_pair);

      var = var->next;
      continue;
    }


    if (var->is_proc_param && !proc_ctx->proc->has_callee) {
      var_loc_pair.loc = gen_param_var_loc(var, param_mem_units_offsets,
                                           &params_offset, params_count++);
    } else {
      var_loc_pair.loc = gen_var_loc(proc_gen_ctx, var,
                                     mem_units_offsets);

      if (proc_gen_ctx->max_mem_units_used[var->kind] < var->mem_unit + 1)
        proc_gen_ctx->max_mem_units_used[var->kind] = var->mem_unit + 1;

      if (var->is_proc_param) {
        Loc param_loc = gen_param_var_loc(var, param_mem_units_offsets,
                                          &params_offset, params_count++);

        sb_push(&proc_gen_ctx->sb, "  mov ");
        sb_push_str(&proc_gen_ctx->sb, var_loc_pair.loc.str);
        sb_push_char(&proc_gen_ctx->sb, ',');
        sb_push_str(&proc_gen_ctx->sb, param_loc.str);
        sb_push_char(&proc_gen_ctx->sb, '\n');
      }
    }

    DA_APPEND(proc_gen_ctx->var_loc_pairs, var_loc_pair);

    var = var->next;
  }
}

static Loc get_var_loc(ProcedureGenerationContext *proc_gen_ctx,
                       Variable *var, bool is_dest) {
  for (u32 i = 0; i < proc_gen_ctx->var_loc_pairs.len; ++i) {
    if (proc_gen_ctx->var_loc_pairs.items[i].var == var) {
      Loc loc = proc_gen_ctx->var_loc_pairs.items[i].loc;

      if (var->is_static && is_dest) {
        StringBuilder sb = {0};
        sb_push_str(&sb, get_ptr_prefix(var->kind));
        sb_push_char(&sb, '[');
        sb_push_str(&sb, loc.str);
        sb_push_char(&sb, ']');

        loc.str = sb_to_str(sb);
      }

      return loc;
    }
  }

  ERROR("Variable `"STR_FMT"` location was not set\n", STR_ARG(var->name));
  exit(1);
}

static Str arg_to_str(ProcedureGenerationContext *proc_gen_ctx, Arg *arg, Variable *var) {
  switch (arg->kind) {
  case ArgKindValue: {
    return value_to_str(arg->as.value);
  }

  case ArgKindVar: {
    return get_var_loc(proc_gen_ctx, var, false).str;
  }

  default: {
    ERROR("Wrong argument kind\n");
    exit(1);
  }
  }
}

static u32 gen_call_params(ProcedureGenerationContext *proc_gen_ctx,
                           ProcedureContext *proc_ctx, Args *args,
                           InstrData *data) {
  u32 params_count[ValueKindCount] = {0};
  u32 params_vars_count = 0;
  u32 offset = 0;

  for (u32 i = 0; i < args->len; ++i) {
    Arg *arg = args->items + i;
    ValueKind param_kind = get_arg_kind(proc_ctx, arg);
    RegNames reg_names = get_param_reg_names(param_kind);

    u32 *current_params_count = params_count + param_kind;
    if (*current_params_count >= reg_names.len) {
      ++*current_params_count;
      continue;
    }

    Str reg_name = reg_names.items[*current_params_count];

    ++*current_params_count;

    Variable *arg_var = NULL;
    if (arg->kind == ArgKindVar && params_vars_count < data->arg_vars.len)
      arg_var = data->arg_vars.items[params_vars_count++];
    Str arg_str = arg_to_str(proc_gen_ctx, arg, arg_var);

    if (!str_eq(reg_name, arg_str)) {
      sb_push(&proc_gen_ctx->sb, "  mov ");
      sb_push_str(&proc_gen_ctx->sb, reg_name);
      sb_push_char(&proc_gen_ctx->sb, ',');
      sb_push_str(&proc_gen_ctx->sb, arg_str);
      sb_push_char(&proc_gen_ctx->sb, '\n');
    }

  }

  for (u32 i = args->len; i > 0; --i) {
    Arg *arg = args->items + i - 1;
    ValueKind param_kind = get_arg_kind(proc_ctx, arg);
    RegNames reg_names = get_param_reg_names(param_kind);

    u32 *current_params_count = params_count + param_kind;
    if (*current_params_count < reg_names.len)
      break;

    --*current_params_count;

    Variable *arg_var = NULL;
    if (arg->kind == ArgKindVar)
      arg_var = data->arg_vars.items[--params_vars_count];
    Str arg_str = arg_to_str(proc_gen_ctx, arg, arg_var);

    sb_push(&proc_gen_ctx->sb, "  push ");
    sb_push_str(&proc_gen_ctx->sb, arg_str);
    sb_push_char(&proc_gen_ctx->sb, '\n');

    offset += get_value_size(get_arg_kind(proc_ctx, arg));
  }

  return offset;
}

void proc_gen_ctx_gen_proc_asm(ProcedureGenerationContext *proc_gen_ctx,
                               Ops *ops, OpGenProcs *op_gen_procs,
                               Procedure *proc, u32 proc_index) {
  Instr *instr = proc->instrs;
  while (instr) {
    if (instr->removed) {
      instr = instr->next;
      continue;
    }

    switch (instr->kind) {
    case InstrKindOp: {
      bool found_op = false;
      bool skip = false;

      for (u32 i = 0; i < ops->len; ++i) {
        if (str_eq(ops->items[i].name, instr->as.op.name)) {
          InstrData *data = get_instr_data(proc->ctx, instr);

          if (data->dest_var && data->dest_var->used.len == 0) {
            skip = true;
            break;
          }

          found_op = true;

          Da(Str) args = {0};

          u32 var_index = 0;
          for (u32 j = 0; j < instr->as.op.args.len; ++j) {
            Variable *arg_var = NULL;
            if (instr->as.op.args.items[j].kind == ArgKindVar)
              arg_var = data->arg_vars.items[var_index++];
            Str arg = arg_to_str(proc_gen_ctx, &instr->as.op.args.items[j], arg_var);
            DA_APPEND(args, arg);
          }

          Loc dest_loc = {0};

          Variable *dest_var = data->dest_var;
          if (dest_var)
            dest_loc = get_var_loc(proc_gen_ctx, dest_var, true);

          (*op_gen_procs->items[i])(&proc_gen_ctx->sb, dest_loc, args.items);

          break;
        }
      }

      if (skip)
        break;

      if (!found_op) {
        ERROR("No such operation was found for current platform\n");
        exit(1);
      }
    } break;

    case InstrKindCall: {
      InstrCall *instr_call = &instr->as.call;
      InstrData *data = get_instr_data(proc->ctx, instr);

      u32 offset = gen_call_params(proc_gen_ctx, proc->ctx,
                                   &instr_call->args, data);

      sb_push(&proc_gen_ctx->sb, "  call $");
      sb_push_str(&proc_gen_ctx->sb, instr_call->callee_name);
      sb_push_char(&proc_gen_ctx->sb, '\n');

      if (offset > 0) {
        sb_push(&proc_gen_ctx->sb, "  add rsp,");
        sb_push_u32(&proc_gen_ctx->sb, offset);
        sb_push_char(&proc_gen_ctx->sb, '\n');
      }
    } break;

    case InstrKindCallAssign: {
      InstrCallAssign *instr_call_assign = &instr->as.call_assign;
      InstrData *data = get_instr_data(proc->ctx, instr);

      u32 offset = gen_call_params(proc_gen_ctx, proc->ctx,
                                   &instr_call_assign->args, data);

      sb_push(&proc_gen_ctx->sb, "  call $");
      sb_push_str(&proc_gen_ctx->sb, instr_call_assign->callee_name);
      sb_push_char(&proc_gen_ctx->sb, '\n');

      if (offset > 0) {
        sb_push(&proc_gen_ctx->sb, "  add rsp,");
        sb_push_u32(&proc_gen_ctx->sb, offset);
        sb_push_char(&proc_gen_ctx->sb, '\n');
      }

      Variable *dest_var = get_instr_data(proc->ctx, instr)->dest_var;

      if (dest_var->used.len > 0) {
        Loc dest_loc = get_var_loc(proc_gen_ctx, dest_var, true);

        sb_push(&proc_gen_ctx->sb, "  mov ");
        sb_push_str(&proc_gen_ctx->sb, dest_loc.str);
        sb_push(&proc_gen_ctx->sb, ",rax\n");
      }
    } break;

    case InstrKindReturn: {
      if (proc->ctx->proc->ret_val_kind != ValueKindUnit) {
        ERROR("Wrong return value kind\n");
        exit(1);
      }

      if (instr->next) {
        proc_gen_ctx->found_return = true;

        sb_push(&proc_gen_ctx->sb, "  jmp p");
        sb_push_u32(&proc_gen_ctx->sb, proc_index);
        sb_push(&proc_gen_ctx->sb, ".end\n");
      }
    } break;

    case InstrKindReturnValue: {
      ValueKind ret_val_kind = get_arg_kind(proc->ctx, &instr->as.ret_val.arg);
      if (proc->ret_val_kind != ret_val_kind) {
        ERROR("Wrong return value kind\n");
        exit(1);
      }

      Variable *ret_val_var = NULL;;
      if (instr->as.ret_val.arg.kind == ArgKindVar)
        ret_val_var = get_instr_data(proc->ctx, instr)->arg_vars.items[0];
      Str ret_val = arg_to_str(proc_gen_ctx, &instr->as.ret_val.arg, ret_val_var);

      sb_push(&proc_gen_ctx->sb, "  mov rax,");
      sb_push_str(&proc_gen_ctx->sb, ret_val);
      sb_push_char(&proc_gen_ctx->sb, '\n');

      if (instr->next) {
        proc_gen_ctx->found_return = true;

        sb_push(&proc_gen_ctx->sb, "  jmp p");
        sb_push_u32(&proc_gen_ctx->sb, proc_index);
        sb_push(&proc_gen_ctx->sb, ".end\n");
      }
    } break;

    case InstrKindJump: {
      sb_push(&proc_gen_ctx->sb, "  jmp p");
      sb_push_u32(&proc_gen_ctx->sb, proc_index);
      sb_push_str(&proc_gen_ctx->sb, instr->as.jump.label_name);
      sb_push_char(&proc_gen_ctx->sb, '\n');
    } break;

    case InstrKindConditionalJump: {
      Variable *arg0_var = NULL;
      if (instr->as.cond_jump.arg0.kind == ArgKindVar)
        arg0_var = get_instr_data(proc->ctx, instr)->arg_vars.items[0];
      Str arg0 = arg_to_str(proc_gen_ctx, &instr->as.cond_jump.arg0, arg0_var);

      Variable *arg1_var = NULL;
      if (instr->as.cond_jump.arg1.kind == ArgKindVar)
        arg1_var = get_instr_data(proc->ctx, instr)->arg_vars.items[1];
      Str arg1 = arg_to_str(proc_gen_ctx, &instr->as.cond_jump.arg1, arg1_var);

      if (arg0_var && get_var_loc(proc_gen_ctx, arg0_var, false).kind == LocKindStack &&
          arg1_var && get_var_loc(proc_gen_ctx, arg1_var, false).kind == LocKindStack) {
        sb_push(&proc_gen_ctx->sb, "  mov rax,");
        sb_push_str(&proc_gen_ctx->sb, arg0);
        sb_push_char(&proc_gen_ctx->sb, '\n');

        arg0 = STR_LIT("rax");
      }

      sb_push(&proc_gen_ctx->sb, "  cmp ");
      sb_push_str(&proc_gen_ctx->sb, arg0);
      sb_push_char(&proc_gen_ctx->sb, ',');
      sb_push_str(&proc_gen_ctx->sb, arg1);
      sb_push(&proc_gen_ctx->sb, "\n  ");
      sb_push_str(&proc_gen_ctx->sb, cond_jump_cmds[instr->as.cond_jump.rel_op]);
      sb_push_char(&proc_gen_ctx->sb, ' ');

      sb_push(&proc_gen_ctx->sb, "p");
      sb_push_u32(&proc_gen_ctx->sb, proc_index);
      sb_push_str(&proc_gen_ctx->sb, instr->as.cond_jump.label_name);
      sb_push_char(&proc_gen_ctx->sb, '\n');
    } break;

    case InstrKindLabel: {
      sb_push(&proc_gen_ctx->sb, " p");
      sb_push_u32(&proc_gen_ctx->sb, proc_index);
      sb_push_str(&proc_gen_ctx->sb, instr->as.label.name);
      sb_push(&proc_gen_ctx->sb, ":\n");

      ++proc_gen_ctx->labels_count;
    } break;

    case InstrKindAlloc: {
      InstrData *data = get_instr_data(proc->ctx, instr);
      Loc dest_loc = get_var_loc(proc_gen_ctx, data->dest_var, true);
      u32 offset = stack_alloc(&proc_gen_ctx->stack, instr->as.alloc.size,
                               data->dest_var->begin_index, (u32) -1);

      sb_push(&proc_gen_ctx->sb, "  lea ");
      sb_push_str(&proc_gen_ctx->sb, dest_loc.str);
      sb_push(&proc_gen_ctx->sb, ",[rbp-");
      sb_push_u32(&proc_gen_ctx->sb, offset);
      sb_push(&proc_gen_ctx->sb, "]\n");
    } break;

    default: {
      ERROR("Wrong instruction kind: %u\n", instr->kind);
      exit(1);
    } break;
    }

    instr = instr->next;
  }
}

void program_gen_asm_x86_64(Program *program, StringBuilder *sb,
                            Ops *ops, OpGenProcs *op_gen_procs) {
  Procedure *proc = program->procs;
  u32 i = 0;
  while (proc) {
    if (!proc->is_used) {
      proc = proc->next;
      ++i;
      continue;
    }

    ProcedureGenerationContext proc_gen_ctx = {0};

    set_var_locs(&proc_gen_ctx, proc->ctx);
    proc_gen_ctx_gen_proc_asm(&proc_gen_ctx, ops,
                              op_gen_procs, proc, i);

    sb_push_char(sb, '$');
    sb_push_str(sb, proc->name);
    sb_push(sb, ":\n");

    for (u32 j = 0; j < ValueKindCount; ++j) {
      RegNames reg_names = get_reg_names(j);
      u32 max = proc_gen_ctx.max_mem_units_used[j];
      if (max > reg_names.len)
        max = reg_names.len;

      for (u32 k = 0; k < max; ++k) {
        sb_push(sb, "  push ");
        sb_push_str(sb, reg_names.items[k]);
        sb_push_char(sb, '\n');
      }
    }

    if (proc_gen_ctx.stack.max_size > 0) {
      sb_push(sb, "  push rbp\n");
      sb_push(sb, "  mov rbp,rsp\n");
      sb_push(sb, "  sub rsp,");
      sb_push_u32(sb, proc_gen_ctx.stack.max_size);
      sb_push_char(sb, '\n');
    }

    sb_push_str(sb, sb_to_str(proc_gen_ctx.sb));
    free(proc_gen_ctx.sb.buffer);

    if (proc_gen_ctx.found_return) {
      sb_push(sb, " p");
      sb_push_u32(sb, i);
      sb_push(sb, ".end:\n");
    }

    if (proc_gen_ctx.stack.max_size > 0) {
      sb_push(sb, "  leave\n");
    }

    for (u32 j = ValueKindCount; j > 0; --j) {
      RegNames reg_names = get_reg_names(j - 1);
      u32 max = proc_gen_ctx.max_mem_units_used[j - 1];
      if (max > reg_names.len)
        max = reg_names.len;

      for (u32 k = max; k > 0; --k) {
        sb_push(sb, "  pop ");
        sb_push_str(sb, reg_names.items[k - 1]);
        sb_push_char(sb, '\n');
      }
    }

    sb_push(sb, "  ret\n");

    proc = proc->next;
    ++i;
  }

  if (program->static_data.len > 0) {
    sb_push(sb, "section .data\n");

    for (u32 i = 0; i < program->static_data.len; ++i) {
      StaticSegment *segment = program->static_data.items + i;
      sb_push_str(sb, segment->name);
      sb_push(sb, ": db ");
      for (u32 j = 0; j < segment->size; ++j) {
        if (j > 0)
          sb_push_char(sb, ',');
        sb_push_u32(sb, segment->data[j]);
      }
      sb_push_char(sb, '\n');
    }
  }
}
