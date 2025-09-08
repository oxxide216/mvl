#ifndef IR_TO_MVM_H
#define IR_TO_MVM_H

#include "mvm/src/mvm.h"
#include "ir.h"

Value ir_arg_value_to_value(IrArgValue *value);
Arg ir_arg_to_arg(IrArg *ir_arg);
ValueKind type_to_value_kind(Type *type);

#endif // IR_TO_MVM_H
