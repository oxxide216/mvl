#include <string.h>

#include "proc-ctx.h"
#include "log.h"
#include "opt.h"
#include "misc.h"
#include "ops.h"

typedef struct {
  u32 begin_index;
  u32 end_index;
  u32 layer_index;
} VariableRange;

typedef Da(VariableRange) MemUnitRanges;

InstrData *get_instr_data(ProcedureContext *proc_ctx, Instr *key) {
  for (u32 i = 0; i < proc_ctx->instrs_data.len; ++i) {
    InstrData *instr_data = proc_ctx->instrs_data.items + i;
    if (instr_data->key == key)
      return instr_data;
  }

  InstrData new_instr_data = { key, NULL, {0} };
  DA_APPEND(proc_ctx->instrs_data, new_instr_data);

  return proc_ctx->instrs_data.items + proc_ctx->instrs_data.len - 1;
}

Variable *lookup_variable(Variable *vars, Str name) {
  Variable *last_var = NULL;

  while (vars) {
    if (str_eq(vars->name, name))
      last_var = vars;

    vars = vars->next;
  }

  return last_var;
}

ValueKind get_arg_kind(ProcedureContext *proc_ctx, Arg *arg) {
  if (arg->kind == ArgKindVar) {
    Variable *var = lookup_variable(proc_ctx->vars, arg->as.var);
    if (!var) {
      ERROR("Variable `"STR_FMT"` was not defined before usage\n", STR_ARG(arg->as.var));
      exit(1);
    }

    return var->kind;
  }

  return arg->as.value.kind;
}

static Variable *use_variable(Variable *vars, Instr *instr, Str name) {
  Variable *var = lookup_variable(vars, name);
  if (!var) {
    ERROR("Variable `"STR_FMT"` was not defined before usage\n", STR_ARG(name));
    exit(1);
  }

  if (var->end_index < instr->index)
    var->end_index = instr->index;
  DA_APPEND(var->used, instr);

  return var;
}

static Variable **vars_sort(Variable *vars, u32 vars_len) {
  Variable **var_pointers = malloc(sizeof(Variable *) * vars_len);

  u32 i = 0;
  Variable *var = vars;
  while (var) {
    var_pointers[i] = var;

    var = var->next;
    ++i;
  }

  for (u32 j = 0; j + 1 < vars_len; ++j) {
    for (u32 k = 0; k + j + 1 < vars_len; ++k) {
      Variable **lhs = var_pointers + k;
      Variable **rhs = var_pointers + k + 1;

      if ((*lhs)->used.len < (*rhs)->used.len) {
        Variable *temp = *lhs;
        *lhs = *rhs;
        *rhs = temp;
      }
    }
  }

  return var_pointers;
}

static bool var_ranges_collide(VariableRange *a, VariableRange *b) {
  if (a->layer_index != b->layer_index)
    return false;

  if (a->begin_index >= b->begin_index &&
      a->begin_index <= b->end_index)
    return true;

  if (a->begin_index <= b->begin_index &&
      a->end_index >= b->begin_index)
    return true;

  return false;
}

static void proc_ctx_set_vars_priorities(ProcedureContext *proc_ctx, VariableLayers layers) {
  u32 vars_len;
  LL_LEN(proc_ctx->vars, Variable, &vars_len);

  Variable **sorted_vars = vars_sort(proc_ctx->vars, vars_len);
  Da(MemUnitRanges) mem_units_ranges = {0};

  for (u32 i = 0; i < vars_len; ++i) {
    Variable *var = sorted_vars[i];

    if (var->can_be_ref_target || var->is_static)
      continue;

    VariableRange new_range = {
      var->begin_index,
      var->end_index,
      layers.indices[var->kind],
    };

    bool found_empty_space = false;

    for (u32 j = 0; j < mem_units_ranges.len; ++j) {
      MemUnitRanges *ranges = mem_units_ranges.items + j;
      bool found_collision = false;

      for (u32 k = 0; k < ranges->len; ++k) {
        if (var_ranges_collide(ranges->items + k, &new_range)) {
          found_collision = true;
          break;
        }
      }

      if (!found_collision) {
        var->mem_unit = j;
        found_empty_space = true;

        DA_APPEND(*ranges, new_range);
        break;
      }
    }

    if (!found_empty_space) {
      var->mem_unit = mem_units_ranges.len;
      MemUnitRanges new_ranges = {0};

      DA_APPEND(new_ranges, new_range);
      DA_APPEND(mem_units_ranges, new_ranges);
    }
  }

  free(sorted_vars);
}

static Variable *get_param_var(Variable *vars, u32 index) {
  u32 params_count = 0;

  Variable *var = vars;
  while (var) {
    if (var->is_proc_param && params_count++ == index)
      return var;

    var = var->next;
  }

  return NULL;
}

static void proc_ctx_iterate_instrs(ProcedureContext *proc_ctx,
                                    Ops *ops, Instr *instr) {
  while (instr && !instr->visited) {
    if (instr->removed) {
      instr = instr->next;
      continue;
    }

    switch (instr->kind) {
    case InstrKindOp: {
      InstrOp *op_instr = &instr->as.op;
      Op *op = get_instr_op(proc_ctx, op_instr, ops);

      if (op->arity == 2 &&
          op_instr->args.items[0].kind == ArgKindValue &&
          op_instr->args.items[1].kind == ArgKindVar) {
        Arg temp_arg = op_instr->args.items[0];
        op_instr->args.items[0] = op_instr->args.items[1];
        op_instr->args.items[1] = temp_arg;
      }

      for (u32 i = 0; i < op->arity; ++i) {
        switch (op->args[i].cond) {
        case ArgConditionAny: break;

        case ArgConditionVar:
        case ArgConditionRefTarget: {
          if (op_instr->args.items[i].kind != ArgKindVar) {
            ERROR("Argument %u of `"STR_FMT"` operation should be a variable\n",
                  i + 1, STR_ARG(op->name));
            exit(1);
          }
        } break;
        }

        if (op_instr->args.items[i].kind == ArgKindVar) {
          use_variable(proc_ctx->vars, instr, op_instr->args.items[i].as.var);

          InstrData *data = get_instr_data(proc_ctx, instr);
          Variable *arg_var = lookup_variable(proc_ctx->vars,
                                              op_instr->args.items[i].as.var);
          DA_APPEND(data->arg_vars, arg_var);

          if (op->args[i].cond == ArgConditionRefTarget)
            arg_var->can_be_ref_target = true;
        }
      }

      if (op->dest_kind == ValueKindUnit)
        break;

      Variable *dest_var = lookup_variable(proc_ctx->vars, op_instr->dest);

      if (dest_var) {
        dest_var = use_variable(proc_ctx->vars, instr, op_instr->dest);
        get_instr_data(proc_ctx, instr)->dest_var = dest_var;
      } else {
        LL_PREPEND(proc_ctx->vars, proc_ctx->vars_end, Variable);
        *proc_ctx->vars_end = (Variable) {
          op_instr->dest, op->dest_kind,
          0, instr->index + 1, instr->index + 1,
          instr, {0}, false, false, false, NULL,
        };

        get_instr_data(proc_ctx, instr)->dest_var = proc_ctx->vars_end;
      }
    } break;

    case InstrKindCall: {
      InstrCall *instr_call = &instr->as.call;
      InstrData *data = get_instr_data(proc_ctx, instr);

      for (u32 i = 0; i < instr_call->args.len; ++i) {
        Arg *arg = instr_call->args.items + i;
        if (arg->kind == ArgKindVar) {
          Variable *arg_var = use_variable(proc_ctx->vars,
                                           instr, arg->as.var);
          DA_APPEND(data->arg_vars, arg_var);
        }

        Variable *param_var = get_param_var(proc_ctx->vars, i);
        if (param_var && param_var->end_index < instr->index)
          param_var->end_index = instr->index;
      }
    } break;

    case InstrKindCallAssign: {
      InstrCallAssign *instr_call_assign = &instr->as.call_assign;
      InstrData *data = get_instr_data(proc_ctx, instr);

      for (u32 i = 0; i < instr_call_assign->args.len; ++i) {
        Arg *arg = instr_call_assign->args.items + i;
        if (arg->kind == ArgKindVar) {
          Variable *arg_var = use_variable(proc_ctx->vars,
                                           instr, arg->as.var);
          DA_APPEND(data->arg_vars, arg_var);
        }

        Variable *param_var = get_param_var(proc_ctx->vars, i);
        if (param_var && param_var->end_index < instr->index)
            param_var->end_index = instr->index;
      }

      Variable *dest_var = lookup_variable(proc_ctx->vars,
                                           instr_call_assign->dest);

      if (dest_var) {
        dest_var = use_variable(proc_ctx->vars, instr, instr_call_assign->dest);
        get_instr_data(proc_ctx, instr)->dest_var = dest_var;
      } else {
        ValueKind ret_val_kind = instr_call_assign->callee->ret_val_kind;

        LL_PREPEND(proc_ctx->vars, proc_ctx->vars_end, Variable);
        *proc_ctx->vars_end = (Variable) {
          instr_call_assign->dest, ret_val_kind,
          0, instr->index + 1, instr->index + 1,
          instr, {0}, false, false, false, NULL,
        };

        get_instr_data(proc_ctx, instr)->dest_var = proc_ctx->vars_end;
      }
    } break;

    case InstrKindReturn: break;

    case InstrKindReturnValue: {
      if (instr->as.ret_val.arg.kind == ArgKindVar) {
        InstrData *data = get_instr_data(proc_ctx, instr);
        Variable *arg_var = use_variable(proc_ctx->vars, instr,
                                         instr->as.ret_val.arg.as.var);
        DA_APPEND(data->arg_vars, arg_var);
      }
    } break;

    case InstrKindJump: break;

    case InstrKindConditionalJump: {
      InstrData *data = get_instr_data(proc_ctx, instr);

      if (instr->as.cond_jump.arg0.kind == ArgKindVar) {
        Variable *arg0_var = use_variable(proc_ctx->vars, instr,
                                          instr->as.cond_jump.arg0.as.var);
        DA_APPEND(data->arg_vars, arg0_var);
      }
      if (instr->as.cond_jump.arg1.kind == ArgKindVar) {
        Variable *arg1_var = use_variable(proc_ctx->vars, instr,
                                          instr->as.cond_jump.arg1.as.var);
        DA_APPEND(data->arg_vars, arg1_var);
      }
    } break;

    case InstrKindLabel: break;

    case InstrKindAlloc: {
      Variable *dest_var = lookup_variable(proc_ctx->vars,
                                           instr->as.alloc.dest);

      if (dest_var) {
        dest_var = use_variable(proc_ctx->vars, instr, instr->as.alloc.dest);
        get_instr_data(proc_ctx, instr)->dest_var = dest_var;
      } else {
        LL_PREPEND(proc_ctx->vars, proc_ctx->vars_end, Variable);
        *proc_ctx->vars_end = (Variable) {
          instr->as.alloc.dest, ValueKindS64,
          0, instr->index + 1, instr->index + 1,
          instr, {0}, false, false, false, NULL,
        };

        get_instr_data(proc_ctx, instr)->dest_var = proc_ctx->vars_end;
      }
    } break;

    default: {
      ERROR("Wrong instruction kind: %u\n", instr->kind);
      exit(1);
    }
    }

    instr->visited = true;
    instr = instr->next;
  }

  instr = proc_ctx->proc->instrs;
  while (instr) {
    if (instr->kind == InstrKindOp) {
      InstrOp *op_instr = &instr->as.op;
      Op *op = get_instr_op(proc_ctx, op_instr, ops);
      InstrData *data = get_instr_data(proc_ctx, instr);

      for (u32 i = 0; i < op->arity; ++i)
        if (op->args[i].cond == ArgConditionRefTarget &&
            data->arg_vars.items[i]->end_index < data->dest_var->end_index)
          data->arg_vars.items[i]->end_index = data->dest_var->end_index;
    }

    instr = instr->next;
  }

  return;
}

ProcedureContext create_proc_ctx(Procedure *proc,Ops ops,
                                 VariableLayers layers,
                                 StaticData *static_data) {
  ProcedureContext proc_ctx = {0};
  proc_ctx.proc = proc;

  if (proc->ret_val_kind != ValueKindUnit) {
      Instr *prev = NULL;
      Instr *instr = proc->instrs;
      while (instr) {
        prev = instr;
        instr = instr->next;
      }

      if (!prev || prev->kind != InstrKindReturnValue) {
        ERROR("Non unit type procedure should return something\n");
        exit(1);
      }
  }

  u32 instrs_count = 0;
  Instr *instr = proc->instrs;
  while (instr) {
    instr->index = instrs_count;

    instr = instr->next;
    ++instrs_count;
  }

  for (u32 i = 0; i < static_data->len; ++i) {
    StaticSegment *segment = static_data->items + i;
    LL_PREPEND(proc_ctx.vars, proc_ctx.vars_end, Variable);
    *proc_ctx.vars_end = (Variable) {
      segment->name, ValueKindS64, // should be a pointer type
      0, 1, 1, NULL, {0},
      false, false, true, NULL,
    };
  }

  for (u32 i = 0; i < proc->params.len; ++i) {
    ProcParam *param = proc->params.items + i;
    LL_PREPEND(proc_ctx.vars, proc_ctx.vars_end, Variable);
    *proc_ctx.vars_end = (Variable) {
      param->name, param->kind,
      0, 1, 1, NULL, {0},
      false, true, false, NULL,
    };
  }

  proc_ctx_iterate_instrs(&proc_ctx, &ops, proc->instrs);
  proc_ctx_set_vars_priorities(&proc_ctx, layers);

  return proc_ctx;
}
