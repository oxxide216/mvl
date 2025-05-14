#include "checker.h"
#include "log.h"
#include "proc-ctx.h"
#include "misc.h"
#include "misc-platform.h"

typedef Da(ValueKind) ProcParamKinds;

static Procedure *get_proc(Procedure *procs, Str name) {
  Procedure *proc = procs;
  while (proc) {
    if (str_eq(proc->name, name))
      return proc;

    proc = proc->next;
  }

  return NULL;
}

static Instr *get_label_instr(Procedure *proc, Str name, Instr *skip) {
  Instr *instr = proc->instrs;
  while (instr) {
    if (instr != skip &&
        instr->kind == InstrKindLabel &&
        str_eq(instr->as.label.name, name))
      return instr;

    instr = instr->next;
  }

  return NULL;
}

void program_check(Program *program, TargetPlatform target) {
  ReservedProcs reserved_procs = get_reserved_procs(target);
  Da(Str) checked_procedures_names = {0};

  Procedure *proc = program->procs;
  if (proc)
    proc->is_used = true;

  while (proc) {
    for (u32 i = 0; i < reserved_procs.len; ++i) {
      if (str_eq(proc->name, reserved_procs.items[i])) {
        ERROR("Cannot define `"STR_FMT"` procedure, because it is reserved\n",
              STR_ARG(proc->name));
        exit(1);
      }
    }

    for (u32 i = 0; i < checked_procedures_names.len; ++i) {
      if (str_eq(proc->name, checked_procedures_names.items[i])) {
        ERROR("Cannot define `"STR_FMT"` procedure, because it was already defined\n",
              STR_ARG(proc->name));
        exit(1);
      }
    }

    DA_APPEND(checked_procedures_names, proc->name);

    Instr *instr = proc->instrs;
    while (instr) {
      switch (instr->kind) {
      case InstrKindCall: {
        instr->as.call.callee = get_proc(program->procs, instr->as.call.callee_name);
        if (!instr->as.call.callee) {
          ERROR("Procedure `"STR_FMT"` with such signature was not found\n",
                STR_ARG(instr->as.call.callee_name));
          exit(1);
        }

        proc->has_callee = true;

        if (proc->is_used)
          instr->as.call.callee->is_used = true;
      } break;

      case InstrKindCallAssign: {
        instr->as.call_assign.callee = get_proc(program->procs, instr->as.call_assign.callee_name);
        if (!instr->as.call_assign.callee) {
          ERROR("Procedure `"STR_FMT"` with such signature was not found\n",
                STR_ARG(instr->as.call_assign.callee_name));
          exit(1);
        }

        proc->has_callee = true;

        if (proc->is_used)
          instr->as.call_assign.callee->is_used = true;
      } break;

      case InstrKindJump: {
        instr->as.jump.target = get_label_instr(proc, instr->as.jump.label_name, NULL);
        if (!instr->as.jump.target) {
          ERROR("Label `"STR_FMT"` was not found\n",
                STR_ARG(instr->as.jump.label_name));
          exit(1);
        }
      } break;

      case InstrKindConditionalJump: {
        instr->as.cond_jump.target = get_label_instr(proc, instr->as.cond_jump.label_name, NULL);
        if (!instr->as.cond_jump.target) {
          ERROR("Label `"STR_FMT"` was not found\n",
                STR_ARG(instr->as.cond_jump.label_name));
          exit(1);
        }
      } break;

      case InstrKindLabel: {
        Instr *prev_label_target = get_label_instr(proc, instr->as.label.name, instr);
        if (prev_label_target) {
          ERROR("Label `"STR_FMT"` was redefined\n", STR_ARG(instr->as.label.name));
          exit(1);
        }
      } break;

      default: break;
      }

      instr = instr->next;
    }

    proc = proc->next;
  }
}

void program_type_check(Program *program) {
  Procedure *proc = program->procs;

  if (proc) {
    if (proc->ret_val_kind != ValueKindUnit &&
        proc->ret_val_kind != ValueKindS64) {
      ERROR("`"STR_FMT"` procedure should return unit or integer\n",
            STR_ARG(proc->name));
      exit(1);
    }

    if (proc->params.len > 2) {
      ERROR("`"STR_FMT"` procedure should have 0-2 parameters\n",
            STR_ARG(proc->name));
      exit(1);
    }

    if (proc->params.len >= 1 &&
        proc->params.items[0].kind != ValueKindS64) {
      ERROR("First parameter of `"STR_FMT"` procedure should be an integer\n",
            STR_ARG(proc->name));
      exit(1);
    }

    if (proc->params.len >= 2 &&
        proc->params.items[1].kind != ValueKindS64) {
      ERROR("Second parameter of `"STR_FMT"` procedure should be an integer\n",
            STR_ARG(proc->name));
      exit(1);
    }
  }

  while (proc) {
    bool found_return = false;

    Instr *instr = proc->instrs;
    while (instr) {
      switch (instr->kind) {
      case InstrKindCall: {
        InstrCall *call_instr = &instr->as.call;

        if (call_instr->callee->params.len != call_instr->args.len) {
          ERROR("Expected %u, but got %u parameters for `"STR_FMT"` procedure\n",
                call_instr->callee->params.len, call_instr->args.len,
                STR_ARG(call_instr->callee_name));
          exit(1);
        }

        for (u32 i = 0; i < call_instr->callee->params.len; ++i) {
          if (get_arg_kind(proc->ctx, call_instr->args.items + i) !=
              call_instr->callee->params.items[i].kind) {
            ERROR("Unexpected type of parameter %u of `"STR_FMT"` procedure\n",
                  i + 1, STR_ARG(call_instr->callee_name));
            exit(1);
          }
        }
      } break;

      case InstrKindCallAssign: {
        InstrCallAssign *call_assign_instr = &instr->as.call_assign;

        if (call_assign_instr->callee->params.len != call_assign_instr->args.len) {
          ERROR("Expected %u, but got %u parameters for `"STR_FMT"` procedure\n",
                call_assign_instr->callee->params.len, call_assign_instr->args.len,
                STR_ARG(call_assign_instr->callee_name));
          exit(1);
        }

        for (u32 i = 0; i < call_assign_instr->callee->params.len; ++i) {
          if (get_arg_kind(proc->ctx, call_assign_instr->args.items + i) !=
              call_assign_instr->callee->params.items[i].kind) {
            ERROR("Unexpected type of parameter %u of `"STR_FMT"` procedure\n",
                  i + 1, STR_ARG(call_assign_instr->callee_name));
            exit(1);
          }
        }
      } break;

      case InstrKindReturn: {
        if (proc->ret_val_kind != ValueKindUnit) {
          ERROR("Non-unit proccedure should return something\n");
          exit(1);
        }

        found_return = true;
      } break;

      case InstrKindReturnValue: {
        ValueKind ret_val_kind = get_arg_kind(proc->ctx, &instr->as.ret_val.arg);
        if (proc->ret_val_kind != ret_val_kind) {
          ERROR("Wrong return value kind\n");
          exit(1);
        }

        found_return = true;
      } break;

      default: break;
      }

      instr = instr->next;
    }

    if (!found_return && proc->ret_val_kind != ValueKindUnit) {
      ERROR("Non-unit proccedure should return something\n");
      exit(1);
    }

    proc = proc->next;
  }
}
