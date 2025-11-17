#ifndef IR_TO_MVM_H
#define IR_TO_MVM_H

#include "mvm/src/mvm.h"
#include "ir.h"

Value ir_arg_value_to_value(IrArgValue *value);
Arg ir_arg_to_arg(IrArg *ir_arg);
ValueKind type_to_value_kind(Type *type);
Str mangle_proc_name_with_params(Str name, IrProcParams *params);
Str mangle_proc_name_with_args(Str name, IrArgs *args);

#endif // IR_TO_MVM_H
